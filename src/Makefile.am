ayatananotificationslibdir = $(INDICATORDIR)
ayatananotificationslib_LTLIBRARIES = libayatana-notifications.la
libayatana_notifications_la_SOURCES = \
	dbus-spy.c \
	dbus-spy.h \
	urlregex.c \
	urlregex.h \
	notification-menuitem.c \
	notification-menuitem.h \
	settings.h \
	indicator-notifications.c \
	notification.c \
	notification.h
libayatana_notifications_la_CFLAGS = \
	-DSETTINGS_PATH=\""$(libexecdir)/$(PACKAGE)/indicator-notifications-settings"\" \
	$(INDICATOR_CFLAGS) \
	-Wall \
	-DG_LOG_DOMAIN=\"ayatana-indicator-notifications\"
libayatana_notifications_la_LIBADD = \
	$(INDICATOR_LIBS)
libayatana_notifications_la_LDFLAGS = \
	-module \
	-avoid-version

pkglibexec_PROGRAMS = indicator-notifications-settings

indicator_notifications_settings_SOURCES = \
	settings.h \
	indicator-notifications-settings.c

indicator_notifications_settings_CFLAGS = \
	$(SETTINGS_CFLAGS) \
	-Wall

indicator_notifications_settings_LDADD = \
	$(SETTINGS_LIBS)
