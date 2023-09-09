/*
 * Copyright 2013 Canonical Ltd.
 * Copyright 2023 Robert Tari <robert@tari.in>
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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

#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "service.h"

static void on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop)
{
    g_message ("exiting: service couldn't acquire or lost ownership of busname");
    g_main_loop_quit ((GMainLoop*)loop);
}

int main (int argc G_GNUC_UNUSED, char ** argv G_GNUC_UNUSED)
{
    IndicatorNotificationsService * service;
    GMainLoop * loop;

    /* boilerplate i18n */
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    textdomain (GETTEXT_PACKAGE);

    /* run */
    service = indicator_notifications_service_new ();
    loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (service, INDICATOR_NOTIFICATIONS_SERVICE_SIGNAL_NAME_LOST, G_CALLBACK(on_name_lost), loop);
    g_main_loop_run (loop);

    /* cleanup */
    g_main_loop_unref (loop);
    g_clear_object (&service);

    return 0;
}
