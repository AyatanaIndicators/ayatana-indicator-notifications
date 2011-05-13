/*
An indicator to display recent notifications.

Adapted from: indicator-datetime/src/indicator-datetime.c by
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include <time.h>

/* GStuff */
#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

/* Indicator Stuff */
#include <libindicator/indicator.h>
#include <libindicator/indicator-object.h>
#include <libindicator/indicator-service-manager.h>

/* DBusMenu */
#include <libdbusmenu-gtk/menu.h>
#include <libido/libido.h>
#include <libdbusmenu-gtk/menuitem.h>

#include "dbus-shared.h"
#include "settings-shared.h"

#define INDICATOR_NOTIFICATIONS_TYPE            (indicator_notifications_get_type ())
#define INDICATOR_NOTIFICATIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), INDICATOR_NOTIFICATIONS_TYPE, IndicatorNotifications))
#define INDICATOR_NOTIFICATIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), INDICATOR_NOTIFICATIONS_TYPE, IndicatorNotificationsClass))
#define IS_INDICATOR_NOTIFICATIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INDICATOR_NOTIFICATIONS_TYPE))
#define IS_INDICATOR_NOTIFICATIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), INDICATOR_NOTIFICATIONS_TYPE))
#define INDICATOR_NOTIFICATIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), INDICATOR_NOTIFICATIONS_TYPE, IndicatorNotificationsClass))

typedef struct _IndicatorNotifications         IndicatorNotifications;
typedef struct _IndicatorNotificationsClass    IndicatorNotificationsClass;
typedef struct _IndicatorNotificationsPrivate  IndicatorNotificationsPrivate;

struct _IndicatorNotificationsClass {
  IndicatorObjectClass parent_class;
};

struct _IndicatorNotifications {
  IndicatorObject parent;
  IndicatorNotificationsPrivate *priv;
};

struct _IndicatorNotificationsPrivate {
  GtkLabel *label;

  IndicatorServiceManager *sm;
  DbusmenuGtkMenu *menu;

  GCancellable *service_proxy_cancel;
  GDBusProxy *service_proxy;
};

#define INDICATOR_NOTIFICATIONS_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_NOTIFICATIONS_TYPE, IndicatorNotificationsPrivate))

GType indicator_notifications_get_type(void);

static void indicator_notifications_class_init(IndicatorNotificationsClass *klass);
static void indicator_notifications_init(IndicatorNotifications *self);
static void indicator_notifications_dispose(GObject *object);
static void indicator_notifications_finalize(GObject *object);
static GtkLabel *get_label(IndicatorObject *io);
static GtkMenu *get_menu(IndicatorObject *io);
static const gchar *get_accessible_desc(IndicatorObject *io);
static void update_label(IndicatorNotifications *io);
static void receive_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data);
static void service_proxy_cb(GObject *object, GAsyncResult *res, gpointer user_data);

/* Indicator Module Config */
INDICATOR_SET_VERSION
INDICATOR_SET_TYPE(INDICATOR_NOTIFICATIONS_TYPE)

G_DEFINE_TYPE (IndicatorNotifications, indicator_notifications, INDICATOR_OBJECT_TYPE);

static void
indicator_notifications_class_init(IndicatorNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private(klass, sizeof(IndicatorNotificationsPrivate));

  object_class->dispose = indicator_notifications_dispose;
  object_class->finalize = indicator_notifications_finalize;

  IndicatorObjectClass *io_class = INDICATOR_OBJECT_CLASS(klass);

  io_class->get_label = get_label;
  io_class->get_menu = get_menu;
  io_class->get_accessible_desc = get_accessible_desc;

  return;
}

static void
menu_visible_notify_cb(GtkWidget *menu, G_GNUC_UNUSED GParamSpec *pspec, gpointer user_data)
{
  /*IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(user_data);*/
  g_debug("notify visible signal received");

  gboolean visible;
  g_object_get(G_OBJECT(menu), "visible", &visible, NULL);
  if(visible) {
    g_debug("notify visible menu shown");
  }
  else {
    g_debug("notify visible menu hidden");
  }
}

static void
indicator_notifications_init(IndicatorNotifications *self)
{
  self->priv = INDICATOR_NOTIFICATIONS_GET_PRIVATE(self);

  self->priv->label = NULL;

  self->priv->service_proxy = NULL;

  self->priv->sm = NULL;
  self->priv->menu = NULL;

  self->priv->sm = indicator_service_manager_new_version(SERVICE_NAME, SERVICE_VERSION);

  self->priv->menu = dbusmenu_gtkmenu_new(SERVICE_NAME, MENU_OBJ);

  g_signal_connect(self->priv->menu, "notify::visible", G_CALLBACK(menu_visible_notify_cb), self);

  /*DbusmenuGtkClient *client = dbusmenu_gtkmenu_get_client(self->priv->menu);*/

  self->priv->service_proxy_cancel = g_cancellable_new();

  g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                           G_DBUS_PROXY_FLAGS_NONE,
                           NULL,
                           SERVICE_NAME,
                           SERVICE_OBJ,
                           SERVICE_IFACE,
                           self->priv->service_proxy_cancel,
                           service_proxy_cb,
                           self);

  return;
}

/* Callback from trying to create the proxy for the serivce, this
   could include starting the service.  Sometime it'll fail and
   we'll try to start that dang service again! */
static void
service_proxy_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;

  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(user_data);
  g_return_if_fail(self != NULL);

  GDBusProxy *proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

  IndicatorNotificationsPrivate *priv = INDICATOR_NOTIFICATIONS_GET_PRIVATE(self);

  if(priv->service_proxy_cancel != NULL) {
    g_object_unref(priv->service_proxy_cancel);
    priv->service_proxy_cancel = NULL;
  }

  if(error != NULL) {
    g_warning("Could not grab DBus proxy for %s: %s", SERVICE_NAME, error->message);
    g_error_free(error);
    return;
  }

  /* Okay, we're good to grab the proxy at this point, we're
  sure that it's ours. */
  priv->service_proxy = proxy;

  g_signal_connect(proxy, "g-signal", G_CALLBACK(receive_signal), self);

  return;
}

static void
indicator_notifications_dispose(GObject *object)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(object);

  if(self->priv->label != NULL) {
    g_object_unref(self->priv->label);
    self->priv->label = NULL;
  }

  if(self->priv->menu != NULL) {
    g_object_unref(G_OBJECT(self->priv->menu));
    self->priv->menu = NULL;
  }

  if(self->priv->sm != NULL) {
    g_object_unref(G_OBJECT(self->priv->sm));
    self->priv->sm = NULL;
  }

  if(self->priv->service_proxy != NULL) {
    g_object_unref(self->priv->service_proxy);
    self->priv->service_proxy = NULL;
  }

  G_OBJECT_CLASS (indicator_notifications_parent_class)->dispose (object);
  return;
}

static void
indicator_notifications_finalize(GObject *object)
{
  /*IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(object);*/

  G_OBJECT_CLASS (indicator_notifications_parent_class)->finalize (object);
  return;
}

/* Updates the accessible description */
static void
update_accessible_description(IndicatorNotifications *io)
{
  GList *entries = indicator_object_get_entries(INDICATOR_OBJECT(io));
  IndicatorObjectEntry *entry = (IndicatorObjectEntry *)entries->data;

  entry->accessible_desc = get_accessible_desc(INDICATOR_OBJECT(io));

  g_signal_emit(G_OBJECT(io),
                INDICATOR_OBJECT_SIGNAL_ACCESSIBLE_DESC_UPDATE_ID,
                0,
                entry,
                TRUE);

  g_list_free(entries);

  return;
}

/* Updates the label */
static void
update_label(IndicatorNotifications *io)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(io);

  if(self->priv->label == NULL) return;

  gtk_label_set_text(self->priv->label, "Test");

  update_accessible_description(io);

  return;
}

/* Receives all signals from the service, routed to the appropriate functions */
static void
receive_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
               GVariant *parameters, gpointer user_data)
{
  /*IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(user_data);*/

  return;
}

/* React to the style changing, which could mean an font
   update. */
static void
style_changed(GtkWidget *widget, GtkStyle *oldstyle, gpointer data)
{
  g_debug("New style for label");
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(data);
  update_label(self);
  return;
}

/* Respond to changes in the screen to update the text gravity */
static void
update_text_gravity(GtkWidget *widget, GdkScreen *previous_screen, gpointer data)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(data);
  if(self->priv->label == NULL) return;

  PangoLayout *layout;
  PangoContext *context;

  layout = gtk_label_get_layout(GTK_LABEL(self->priv->label));
  context = pango_layout_get_context(layout);
  pango_context_set_base_gravity(context, PANGO_GRAVITY_AUTO);
}

/* Grabs the label.  Creates it if it doesn't
   exist already */
static GtkLabel *
get_label(IndicatorObject *io)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(io);

  /* If there's not a label, we'll build ourselves one */
  if(self->priv->label == NULL) {
    self->priv->label = GTK_LABEL(gtk_label_new("Test Init"));
    gtk_label_set_justify(GTK_LABEL(self->priv->label), GTK_JUSTIFY_CENTER);
    g_object_ref(G_OBJECT(self->priv->label));
    g_signal_connect(G_OBJECT(self->priv->label), "style-set", G_CALLBACK(style_changed), self);
    g_signal_connect(G_OBJECT(self->priv->label), "screen-changed", G_CALLBACK(update_text_gravity), self);
    update_label(self);
    gtk_widget_set_visible(GTK_WIDGET(self->priv->label), TRUE);
  }

  return self->priv->label;
}

static GtkMenu *
get_menu(IndicatorObject *io)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(io);

  return GTK_MENU(self->priv->menu);
}

static const gchar *
get_accessible_desc(IndicatorObject *io)
{
  IndicatorNotifications *self = INDICATOR_NOTIFICATIONS(io);
  const gchar *name;

  if(self->priv->label != NULL) {
    name = gtk_label_get_text(self->priv->label);
    return name;
  }
  return NULL;
}