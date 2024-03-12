/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Ted Gould <ted@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ayatana/common/utils.h>
#include "service.h"
#include "dbus-spy.h"
#include "urlregex.h"

#define BUS_NAME "org.ayatana.indicator.notifications"
#define BUS_PATH "/org/ayatana/indicator/notifications"
#define HINT_MAX 10

static guint m_nSignal = 0;

enum
{
    SECTION_HEADER = (1<<0),
    SECTION_NOTIFICATIONS = (1<<1),
    SECTION_DO_NOT_DISTURB = (1<<2),
    SECTION_CLEAR = (1<<3)
};

enum
{
    PROFILE_PHONE,
    PROFILE_DESKTOP,
    N_PROFILES
};

static const char * const menu_names[N_PROFILES] =
{
    "phone",
    "desktop"
};

struct ProfileMenuInfo
{
    GMenu *pMenu;
    GMenu *pSubmenu;
    guint nExportId;
};

struct _IndicatorNotificationsServicePrivate
{
    GCancellable *pCancellable;
    GSettings *pSettings;
    guint nOwnId;
    guint nActionsId;
    GDBusConnection *pConnection;
    gboolean bMenusBuilt;
    struct ProfileMenuInfo lMenus[N_PROFILES];
    GSimpleActionGroup *pActionGroup;
    GSimpleAction *pHeaderAction;
    GSimpleAction *pClearAction;
    GSimpleAction *pRemoveAction;
    GSimpleAction *pDoNotDisturbAction;
    GList *lVisibleItems;
    GList *lHiddenItems;
    gboolean bDoNotDisturb;
    gboolean bHasUnread;
    gint nMaxItems;
    DBusSpy *pBusSpy;
    GHashTable *lFilters;
    GList *lHints;
    GMenu *pNotificationsSection;
    gboolean bHasDoNotDisturb;
};

typedef IndicatorNotificationsServicePrivate priv_t;

G_DEFINE_TYPE_WITH_PRIVATE(IndicatorNotificationsService, indicator_notifications_service, G_TYPE_OBJECT)

static void rebuildNow(IndicatorNotificationsService *self, guint nSections);
static void updateFilters(IndicatorNotificationsService *self);

static void saveHints(IndicatorNotificationsService *self)
{
    gchar *hints[HINT_MAX + 1];
    int i = 0;
    GList *l;

    for (l = self->priv->lHints; (l != NULL) && (i < HINT_MAX); l = l->next, i++)
    {
        hints[i] = (gchar *) l->data;
    }

    hints[i] = NULL;

    g_settings_set_strv(self->priv->pSettings, "filter-list-hints", (const gchar **) hints);
}

static void updateHints(IndicatorNotificationsService *self, Notification *notification)
{
    g_return_if_fail(IS_NOTIFICATION(notification));

    const gchar *appname = notification_get_app_name(notification);

    // Avoid duplicates
    GList *l;
    for (l = self->priv->lHints; l != NULL; l = l->next)
    {
        if (g_strcmp0(appname, (const gchar *) l->data) == 0)
        {
            return;
        }
    }

    // Add the appname
    self->priv->lHints = g_list_prepend(self->priv->lHints, g_strdup(appname));

    // Keep only a reasonable number
    while (g_list_length(self->priv->lHints) > HINT_MAX)
    {
        GList *last = g_list_last(self->priv->lHints);
        g_free(last->data);
        self->priv->lHints = g_list_delete_link(self->priv->lHints, last);
    }

    // Save the hints
    saveHints(self);
}

static gchar *createMarkup(const gchar *body)
{
    GList *list = urlregex_split_all(body);
    guint len = g_list_length(list);
    gchar **str_array = g_new0(gchar *, len + 1);
    guint i = 0;
    GList *item;
    gchar *escaped_text;
    gchar *escaped_expanded;

    for (item = list; item; item = item->next, i++)
    {
        MatchGroup *group = (MatchGroup *)item->data;

        if (group->type == MATCHED)
        {
            escaped_text = g_markup_escape_text(group->text, -1);
            escaped_expanded = g_markup_escape_text(group->expanded, -1);
            str_array[i] = g_strdup_printf("<a href=\"%s\">%s</a>", escaped_expanded, escaped_text);
            g_free(escaped_text);
            g_free(escaped_expanded);
        }
        else
        {
            str_array[i] = g_markup_escape_text(group->text, -1);
        }
    }

    urlregex_matchgroup_list_free(list);
    gchar *result = g_strjoinv(NULL, str_array);
    g_strfreev(str_array);

    return result;
}

static void updateClearItem(IndicatorNotificationsService *self)
{
    guint visible_length = g_list_length(self->priv->lVisibleItems);
    guint hidden_length = g_list_length(self->priv->lHiddenItems);
    guint total_length = visible_length + hidden_length;

    g_simple_action_set_enabled(self->priv->pClearAction, total_length != 0);
}

static void setUnread(IndicatorNotificationsService *self, gboolean unread)
{
    self->priv->bHasUnread = unread;
    rebuildNow(self, SECTION_HEADER);
}

static void onMessageReceived(DBusSpy *pBusSpy, Notification *note, gpointer user_data)
{
    g_return_if_fail(IS_DBUS_SPY(pBusSpy));
    g_return_if_fail(IS_NOTIFICATION(note));
    IndicatorNotificationsService *self = INDICATOR_NOTIFICATIONS_SERVICE(user_data);

    // Discard useless notifications
    if(notification_is_private(note) || notification_is_empty(note))
    {
        g_object_unref(note);

        return;
    }

    // Discard notifications on the filter list
    if(self->priv->lFilters != NULL && g_hash_table_contains(self->priv->lFilters, notification_get_app_name(note)))
    {
        g_object_unref(note);

        return;
    }

    updateHints(self, note);

    gchar *unescaped_timestamp_string = notification_timestamp_for_locale(note);
    gchar *app_name = g_markup_escape_text(notification_get_app_name(note), -1);
    gchar *summary = g_markup_escape_text(notification_get_summary(note), -1);
    gchar *body = createMarkup(notification_get_body(note));
    g_object_unref(note);
    gchar *timestamp_string = g_markup_escape_text(unescaped_timestamp_string, -1);
    gchar *markup = g_strdup_printf("<b>%s</b>\n%s\n<small><i>%s %s <b>%s</b></i></small>", summary, body, timestamp_string, _("from"), app_name);
    g_free(app_name);
    g_free(summary);
    g_free(body);
    g_free(unescaped_timestamp_string);
    g_free(timestamp_string);
    GMenuItem * item = g_menu_item_new(markup, NULL);
    g_free(markup);
    gint64 nTimestamp = notification_get_timestamp(note);
    g_menu_item_set_action_and_target_value(item, "indicator.remove-notification", g_variant_new_int64(nTimestamp));
    g_menu_item_set_attribute_value(item, "x-ayatana-timestamp", g_variant_new_int64(nTimestamp));
    g_menu_item_set_attribute_value(item, "x-ayatana-use-markup", g_variant_new_boolean(TRUE));
    g_menu_item_set_attribute(item, "x-ayatana-type", "s", "org.ayatana.indicator.removable");

    GList *last_item;
    GMenuItem *last_menu_item;

    // List holds a ref to the menuitem
    self->priv->lVisibleItems = g_list_prepend(self->priv->lVisibleItems, g_object_ref(item));
    g_menu_prepend_item (self->priv->pNotificationsSection, item);
    g_object_unref(item);

    // Move items that overflow to the hidden list
    while (g_list_length(self->priv->lVisibleItems) > self->priv->nMaxItems)
    {
        last_item = g_list_last(self->priv->lVisibleItems);
        last_menu_item = G_MENU_ITEM(last_item->data);

        // Steal the ref from the visible list
        self->priv->lVisibleItems = g_list_delete_link(self->priv->lVisibleItems, last_item);
        self->priv->lHiddenItems = g_list_prepend(self->priv->lHiddenItems, last_menu_item);
        last_item = NULL;
        last_menu_item = NULL;
    }

    while (g_menu_model_get_n_items(G_MENU_MODEL(self->priv->pNotificationsSection)) > self->priv->nMaxItems)
    {
        g_menu_remove(self->priv->pNotificationsSection, self->priv->nMaxItems);
    }

    updateClearItem(self);
    setUnread(self, TRUE);
}

static GVariant *createHeaderState(IndicatorNotificationsService *self)
{
    GVariantBuilder b;

    g_variant_builder_init (&b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add (&b, "{sv}", "title", g_variant_new_string (_("Notifications")));
    g_variant_builder_add (&b, "{sv}", "tooltip", g_variant_new_string (_("List of past system notifications, do-not-disturb switch")));

    /* notifications indicator is not designed for running in Lomiri, so let's hide it when running in Lomiri */
    if (ayatana_common_utils_is_lomiri()) {
        g_variant_builder_add (&b, "{sv}", "visible", g_variant_new_boolean (FALSE));
    }
    else
    {
        g_variant_builder_add (&b, "{sv}", "visible", g_variant_new_boolean (TRUE));
    }
    gchar *sIcon = NULL;

    if (self->priv->bHasUnread)
    {
        if (self->priv->bHasDoNotDisturb && self->priv->bDoNotDisturb)
        {
            sIcon = "ayatana-indicator-notification-unread-dnd";
        }
        else
        {
            sIcon = "ayatana-indicator-notification-unread";
        }
    }
    else
    {
        if (self->priv->bHasDoNotDisturb && self->priv->bDoNotDisturb)
        {
            sIcon = "ayatana-indicator-notification-read-dnd";
        }
        else
        {
            sIcon = "ayatana-indicator-notification-read";
        }
    }

    GIcon * icon = g_themed_icon_new_with_default_fallbacks(sIcon);
    g_variant_builder_add (&b, "{sv}", "accessible-desc", g_variant_new_string (_("Notifications")));

    if (icon)
    {
        GVariant * serialized_icon = g_icon_serialize (icon);

        if (serialized_icon != NULL)
        {
            g_variant_builder_add (&b, "{sv}", "icon", serialized_icon);
            g_variant_unref (serialized_icon);
        }

        g_object_unref (icon);
    }

    return g_variant_builder_end (&b);
}

static GMenuModel *createDesktopNotificationsSection(IndicatorNotificationsService *self, int profile)
{
    self->priv->pNotificationsSection = g_menu_new();

    return G_MENU_MODEL(self->priv->pNotificationsSection);
}

static GMenuModel *createDesktopClearSection(IndicatorNotificationsService *self)
{
    GMenu * pMenu = g_menu_new ();
    g_menu_append(pMenu, _("Clear"), "indicator.clear-notifications");

    return G_MENU_MODEL(pMenu);
}

static GMenuModel *createDesktopDoNotDisturbSection(IndicatorNotificationsService *self)
{
    GMenu *pMenu = g_menu_new();

    if (self->priv->bHasDoNotDisturb)
    {
        GMenuItem *item = g_menu_item_new(_("Do not disturb"), NULL);
        g_menu_item_set_attribute(item, "x-ayatana-type", "s", "org.ayatana.indicator.switch");
        g_menu_item_set_action_and_target(item, "indicator.do-not-disturb", NULL);
        g_menu_append_item(pMenu, item);
        g_object_unref(item);
    }

    return G_MENU_MODEL(pMenu);
}

static void rebuildSection(GMenu *parent, int pos, GMenuModel *new_section)
{
    g_menu_remove (parent, pos);
    g_menu_insert_section (parent, pos, NULL, new_section);
    g_object_unref (new_section);
}

static void rebuildNow(IndicatorNotificationsService *self, guint sections)
{
    priv_t *p = self->priv;
    struct ProfileMenuInfo *desktop = &p->lMenus[PROFILE_DESKTOP];

    if (sections & SECTION_HEADER)
    {
        g_simple_action_set_state (p->pHeaderAction, createHeaderState (self));
    }

    if (!p->bMenusBuilt)
    {
        return;
    }

    if (sections & SECTION_NOTIFICATIONS)
    {
        rebuildSection (desktop->pSubmenu, 0, createDesktopNotificationsSection (self, PROFILE_DESKTOP));
    }

    if (sections & SECTION_DO_NOT_DISTURB)
    {
        rebuildSection (desktop->pSubmenu, 1, createDesktopDoNotDisturbSection (self));
    }

    if (sections & SECTION_CLEAR)
    {
        rebuildSection (desktop->pSubmenu, 2, createDesktopClearSection (self));
    }
}

static void createMenu(IndicatorNotificationsService *self, int profile)
{
    GMenu * pMenu;
    GMenu * pSubmenu;
    GMenuItem * header;
    GMenuModel * sections[16];
    guint i;
    guint n = 0;

    g_assert (0 <= profile && profile < N_PROFILES);
    g_assert (self->priv->lMenus[profile].pMenu == NULL);

    // Build the sections
    switch (profile)
    {
        case PROFILE_PHONE:
        case PROFILE_DESKTOP:
        {
            sections[n++] = createDesktopNotificationsSection(self, PROFILE_DESKTOP);
            sections[n++] = createDesktopDoNotDisturbSection(self);
            sections[n++] = createDesktopClearSection(self);

            break;
        }

        break;
    }

    // Add sections to the submenu
    pSubmenu = g_menu_new();

    for (i=0; i<n; ++i)
    {
        g_menu_append_section (pSubmenu, NULL, sections[i]);
        g_object_unref (sections[i]);
    }

    // Add submenu to the header
    header = g_menu_item_new (NULL, "indicator._header");
    g_menu_item_set_attribute (header, "x-ayatana-type", "s", "org.ayatana.indicator.root");
    g_menu_item_set_submenu(header, G_MENU_MODEL (pSubmenu));
    g_object_unref(pSubmenu);

    // Add header to the menu
    pMenu = g_menu_new ();
    g_menu_append_item (pMenu, header);
    g_object_unref (header);

    self->priv->lMenus[profile].pMenu = pMenu;
    self->priv->lMenus[profile].pSubmenu = pSubmenu;
}

static void clearMenuItems(IndicatorNotificationsService *self)
{
    // Remove each visible item from the menu
    while (g_menu_model_get_n_items(G_MENU_MODEL(self->priv->pNotificationsSection)) > 0)
    {
        g_menu_remove(self->priv->pNotificationsSection, 0);
    }

    // Clear the lists
    g_list_free_full(self->priv->lVisibleItems, g_object_unref);
    self->priv->lVisibleItems = NULL;

    g_list_free_full(self->priv->lHiddenItems, g_object_unref);
    self->priv->lHiddenItems = NULL;

    updateClearItem(self);
}

static void onRemoveNotification(GSimpleAction *a, GVariant *param, gpointer user_data)
{
    IndicatorNotificationsService *self = INDICATOR_NOTIFICATIONS_SERVICE(user_data);
    guint nItems = g_menu_model_get_n_items(G_MENU_MODEL(self->priv->pNotificationsSection));
    gint64 nTimestampIn = g_variant_get_int64(param);

    for (guint nItem = 0; nItem < nItems; nItem++)
    {
        gint64 nTimestamp;
        g_menu_model_get_item_attribute(G_MENU_MODEL(self->priv->pNotificationsSection), nItem, "x-ayatana-timestamp", "x", &nTimestamp);

        if (nTimestamp == nTimestampIn)
        {
            g_menu_remove(self->priv->pNotificationsSection, nItem);

            break;
        }
    }

    for (GList *pItem = self->priv->lVisibleItems; pItem; pItem = pItem->next)
    {
        gint64 nTimestamp;
        g_menu_item_get_attribute(G_MENU_ITEM(pItem->data), "x-ayatana-timestamp", "x", &nTimestamp);

        if (nTimestamp == nTimestampIn)
        {
            // Remove the item
            self->priv->lVisibleItems = g_list_delete_link(self->priv->lVisibleItems, pItem);

            // Add an item from the hidden list, if available
            if (g_list_length(self->priv->lHiddenItems) > 0)
            {
                pItem = g_list_first(self->priv->lHiddenItems);
                GMenuItem *pMenuItem = G_MENU_ITEM(pItem->data);
                self->priv->lHiddenItems = g_list_delete_link(self->priv->lHiddenItems, pItem);
                g_menu_append_item(self->priv->pNotificationsSection, pMenuItem);

                // Steal the ref back from the hidden list
                self->priv->lVisibleItems = g_list_append(self->priv->lVisibleItems, pMenuItem);
            }

            updateClearItem(self);

            if (self->priv->lVisibleItems == NULL)
            {
                setUnread(self, FALSE);
            }

            break;
        }
    }
}

static void onClear(GSimpleAction *a, GVariant *param, gpointer user_data)
{
    IndicatorNotificationsService *self = INDICATOR_NOTIFICATIONS_SERVICE(user_data);

    clearMenuItems(self);
    setUnread(self, FALSE);
}

static void onDoNotDisturb(GSimpleAction *a, GVariant *param, gpointer user_data)
{
    IndicatorNotificationsService *self = INDICATOR_NOTIFICATIONS_SERVICE(user_data);

    if (self->priv->bHasDoNotDisturb)
    {
        self->priv->bDoNotDisturb = g_variant_get_boolean(param);
        GSettings *pSettings = g_settings_new("org.mate.NotificationDaemon");
        g_settings_set_boolean(pSettings, "do-not-disturb", self->priv->bDoNotDisturb);
        g_object_unref(pSettings);
        g_settings_set_boolean(self->priv->pSettings, "do-not-disturb", self->priv->bDoNotDisturb);
        rebuildNow(self, SECTION_HEADER);
    }
}

static void onSettingsChanged(GSettings *pSettings, gchar *key, gpointer user_data)
{
    IndicatorNotificationsService *self = INDICATOR_NOTIFICATIONS_SERVICE(user_data);

    if (g_str_equal(key, "filter-list"))
    {
        updateFilters(self);
    }
    else if (g_str_equal(key, "do-not-disturb"))
    {
        if (self->priv->bHasDoNotDisturb)
        {
            self->priv->bDoNotDisturb = g_settings_get_boolean(self->priv->pSettings, key);
            GSettings *pSettings = g_settings_new("org.mate.NotificationDaemon");
            g_settings_set_boolean(pSettings, key, self->priv->bDoNotDisturb);
            g_object_unref(pSettings);
            g_action_change_state(G_ACTION(self->priv->pDoNotDisturbAction), g_variant_new_boolean(self->priv->bDoNotDisturb));
            rebuildNow(self, SECTION_HEADER);
        }
        else if (g_settings_get_boolean(self->priv->pSettings, key))
        {
            g_settings_set_boolean(self->priv->pSettings, key, FALSE);
        }
    }
}

static void initActions(IndicatorNotificationsService *self)
{
    GSimpleAction * a;
    GAction *max_items_action;
    self->priv->pActionGroup = g_simple_action_group_new();

    // Add the header action
    a = g_simple_action_new_stateful ("_header", NULL, createHeaderState(self));
    g_action_map_add_action(G_ACTION_MAP(self->priv->pActionGroup), G_ACTION(a));
    self->priv->pHeaderAction = a;

    // Add the remove action
    a = g_simple_action_new("remove-notification", G_VARIANT_TYPE_INT64);
    g_action_map_add_action(G_ACTION_MAP(self->priv->pActionGroup), G_ACTION(a));
    self->priv->pRemoveAction = a;
    g_signal_connect(a, "activate", G_CALLBACK(onRemoveNotification), self);

    a = g_simple_action_new("clear-notifications", NULL);
    g_simple_action_set_enabled (a, FALSE);
    g_action_map_add_action(G_ACTION_MAP(self->priv->pActionGroup), G_ACTION(a));
    self->priv->pClearAction = a;
    g_signal_connect(a, "activate", G_CALLBACK(onClear), self);

    if (self->priv->bHasDoNotDisturb)
    {
        a = g_simple_action_new_stateful("do-not-disturb", G_VARIANT_TYPE_BOOLEAN, g_variant_new_boolean(self->priv->bDoNotDisturb));
        g_action_map_add_action(G_ACTION_MAP(self->priv->pActionGroup), G_ACTION(a));
        self->priv->pDoNotDisturbAction = a;
        g_signal_connect(a, "activate", G_CALLBACK(onDoNotDisturb), self);
    }

    // Add the max-items action
    max_items_action = g_settings_create_action(self->priv->pSettings, "max-items");
    g_action_map_add_action(G_ACTION_MAP(self->priv->pActionGroup), max_items_action);

    g_signal_connect(self->priv->pSettings, "changed", G_CALLBACK(onSettingsChanged), self);

    rebuildNow(self, SECTION_HEADER);

    g_object_unref(max_items_action);
}

static void onBusAcquired(GDBusConnection *connection, const gchar *name, gpointer gself)
{
    int i;
    guint id;
    GError * err = NULL;
    IndicatorNotificationsService * self = INDICATOR_NOTIFICATIONS_SERVICE(gself);
    priv_t * p = self->priv;
    GString * path = g_string_new (NULL);

    g_debug ("bus acquired: %s", name);

    p->pConnection = (GDBusConnection*)g_object_ref(G_OBJECT (connection));

    // Export the actions
    if ((id = g_dbus_connection_export_action_group (connection, BUS_PATH, G_ACTION_GROUP (p->pActionGroup), &err)))
    {
        p->nActionsId = id;
    }
    else
    {
        g_warning ("cannot export action group: %s", err->message);
        g_clear_error (&err);
    }

    // Export the menus
    for (i=0; i<N_PROFILES; ++i)
    {
        struct ProfileMenuInfo * menu = &p->lMenus[i];

        g_string_printf (path, "%s/%s", BUS_PATH, menu_names[i]);

        if ((id = g_dbus_connection_export_menu_model (connection, path->str, G_MENU_MODEL (menu->pMenu), &err)))
        {
            menu->nExportId = id;
        }
        else
        {
            g_warning ("cannot export %s menu: %s", path->str, err->message);
            g_clear_error (&err);
        }
    }

    g_string_free (path, TRUE);
}

static void unexport(IndicatorNotificationsService *self)
{
    int i;
    priv_t * p = self->priv;

    // Unexport the menus
    for (i=0; i<N_PROFILES; ++i)
    {
        guint * id = &self->priv->lMenus[i].nExportId;

        if (*id)
        {
            g_dbus_connection_unexport_menu_model (p->pConnection, *id);
            *id = 0;
        }
    }

    // Unexport the actions
    if (p->nActionsId)
    {
        g_dbus_connection_unexport_action_group (p->pConnection, p->nActionsId);
        p->nActionsId = 0;
    }
}

static void onNameLost(GDBusConnection *connection, const gchar *name, gpointer gself)
{
    IndicatorNotificationsService * self = INDICATOR_NOTIFICATIONS_SERVICE (gself);

    g_debug("%s %s name lost %s", G_STRLOC, G_STRFUNC, name);

    unexport(self);
}

static void onDispose(GObject *o)
{
    IndicatorNotificationsService * self = INDICATOR_NOTIFICATIONS_SERVICE(o);
    priv_t * p = self->priv;

    if (self->priv->lVisibleItems != NULL)
    {
        g_list_free_full(self->priv->lVisibleItems, g_object_unref);
        self->priv->lVisibleItems = NULL;
    }

    if (self->priv->lHiddenItems != NULL)
    {
        g_list_free_full(self->priv->lHiddenItems, g_object_unref);
        self->priv->lHiddenItems = NULL;
    }

    if (self->priv->pBusSpy != NULL)
    {
        g_object_unref(G_OBJECT(self->priv->pBusSpy));
        self->priv->pBusSpy = NULL;
    }

    if(self->priv->lFilters != NULL)
    {
        g_hash_table_unref(self->priv->lFilters);
        self->priv->lFilters = NULL;
    }

    if(self->priv->lHints != NULL)
    {
        g_list_free_full(self->priv->lHints, g_free);
        self->priv->lHints = NULL;
    }

    if (p->nOwnId)
    {
        g_bus_unown_name (p->nOwnId);
        p->nOwnId = 0;
    }

    unexport (self);

    if (p->pCancellable != NULL)
    {
        g_cancellable_cancel (p->pCancellable);
        g_clear_object (&p->pCancellable);
    }

    if (p->pSettings != NULL)
    {
        g_signal_handlers_disconnect_by_data (p->pSettings, self);
        g_clear_object (&p->pSettings);
    }

    if (self->priv->bHasDoNotDisturb)
    {
        g_clear_object(&p->pDoNotDisturbAction);
    }

    g_clear_object (&p->pClearAction);
    g_clear_object (&p->pRemoveAction);
    g_clear_object (&p->pHeaderAction);
    g_clear_object (&p->pActionGroup);
    g_clear_object (&p->pConnection);

    G_OBJECT_CLASS (indicator_notifications_service_parent_class)->dispose (o);
}

static void updateFilters(IndicatorNotificationsService *self)
{
    g_return_if_fail(self->priv->lFilters != NULL);

    g_hash_table_remove_all(self->priv->lFilters);
    gchar **items = g_settings_get_strv(self->priv->pSettings, "filter-list");
    int i;

    for(i = 0; items[i] != NULL; i++)
    {
        g_hash_table_insert(self->priv->lFilters, g_strdup(items[i]), NULL);
    }

    g_strfreev(items);
}

static void loadHints(IndicatorNotificationsService *self)
{
    g_return_if_fail(self->priv->lHints == NULL);

    gchar **items = g_settings_get_strv(self->priv->pSettings, "filter-list-hints");
    int i;

    for (i = 0; items[i] != NULL; i++)
    {
        self->priv->lHints = g_list_prepend(self->priv->lHints, items[i]);
    }

    g_free(items);
}

static gboolean getDoNotDisturb()
{
    // Check if we can access the schema
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();

    if (source == NULL)
    {
        return FALSE;
    }

    // Lookup the schema
    GSettingsSchema *source_schema = g_settings_schema_source_lookup(source, "org.mate.NotificationDaemon", TRUE);

    // Couldn't find the schema
    if (source_schema == NULL)
    {
        return FALSE;
    }

    gboolean bResult = FALSE;

    // Found the schema, make sure we have the key
    if (g_settings_schema_has_key(source_schema, "do-not-disturb"))
    {
        // Make sure the key is of boolean type
        GSettingsSchemaKey *source_key = g_settings_schema_get_key(source_schema, "do-not-disturb");

        if (g_variant_type_equal(g_settings_schema_key_get_value_type(source_key), G_VARIANT_TYPE_BOOLEAN))
        {
            bResult = TRUE;
        }

        g_settings_schema_key_unref(source_key);
    }

    g_settings_schema_unref(source_schema);

    return bResult;
}

static void indicator_notifications_service_init(IndicatorNotificationsService *self)
{
    int i;
    self->priv = indicator_notifications_service_get_instance_private(self);
    self->priv->pCancellable = g_cancellable_new();
    self->priv->bHasDoNotDisturb = getDoNotDisturb();
    self->priv->pSettings = g_settings_new("org.ayatana.indicator.notifications");
    self->priv->bHasUnread = FALSE;
    self->priv->lVisibleItems = NULL;
    self->priv->lHiddenItems = NULL;

    // Watch for notifications from dbus
    self->priv->pBusSpy = dbus_spy_new();
    g_signal_connect(self->priv->pBusSpy, DBUS_SPY_SIGNAL_MESSAGE_RECEIVED, G_CALLBACK(onMessageReceived), self);

    // Initialize an empty filter list
    self->priv->lFilters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->priv->nMaxItems = g_settings_get_int(self->priv->pSettings, "max-items");

    if (self->priv->bHasDoNotDisturb)
    {
        self->priv->bDoNotDisturb = g_settings_get_boolean(self->priv->pSettings, "do-not-disturb");
    }

    updateFilters(self);

    // Set up filter-list hints
    self->priv->lHints = NULL;
    loadHints(self);

    initActions(self);

    for (i=0; i<N_PROFILES; ++i)
    {
        createMenu(self, i);
    }

    self->priv->bMenusBuilt = TRUE;
    self->priv->nOwnId = g_bus_own_name(G_BUS_TYPE_SESSION, BUS_NAME, G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT, onBusAcquired, NULL, onNameLost, self, NULL);
}

static void indicator_notifications_service_class_init(IndicatorNotificationsServiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = onDispose;
    m_nSignal = g_signal_new(INDICATOR_NOTIFICATIONS_SERVICE_SIGNAL_NAME_LOST, G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (IndicatorNotificationsServiceClass, name_lost), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

IndicatorNotificationsService *indicator_notifications_service_new()
{
    GObject *o = g_object_new(INDICATOR_TYPE_NOTIFICATIONS_SERVICE, NULL);

    return INDICATOR_NOTIFICATIONS_SERVICE(o);
}
