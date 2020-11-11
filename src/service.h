/*
 * Copyright 2013 Canonical Ltd.
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

#ifndef __INDICATOR_NOTIFICATIONS_SERVICE_H__
#define __INDICATOR_NOTIFICATIONS_SERVICE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* standard GObject macros */
#define INDICATOR_NOTIFICATIONS_SERVICE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_NOTIFICATIONS_SERVICE, IndicatorNotificationsService))
#define INDICATOR_TYPE_NOTIFICATIONS_SERVICE (indicator_notifications_service_get_type())
#define INDICATOR_IS_NOTIFICATIONS_SERVICE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_NOTIFICATIONS_SERVICE))

typedef struct _IndicatorNotificationsService IndicatorNotificationsService;
typedef struct _IndicatorNotificationsServiceClass IndicatorNotificationsServiceClass;
typedef struct _IndicatorNotificationsServicePrivate IndicatorNotificationsServicePrivate;

/* signal keys */
#define INDICATOR_NOTIFICATIONS_SERVICE_SIGNAL_NAME_LOST "name-lost"

/**
 * The Indicator Notifications Service.
 */
struct _IndicatorNotificationsService
{
    /*< private >*/
    GObject parent;
    IndicatorNotificationsServicePrivate * priv;
};

struct _IndicatorNotificationsServiceClass
{
    GObjectClass parent_class;

    /* signals */

    void (* name_lost)(IndicatorNotificationsService * self);
};

GType indicator_notifications_service_get_type (void);

IndicatorNotificationsService *indicator_notifications_service_new();

G_END_DECLS

#endif /* __INDICATOR_NOTIFICATIONS_SERVICE_H__ */
