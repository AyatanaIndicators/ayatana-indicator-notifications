ayatananotificationslibdir = $(INDICATORDIR)
ayatananotificationslib_LTLIBRARIES = libayatana-notifications.la
libayatana_notifications_la_SOURCES = \
	dbus-spy.c \
	dbus-spy.h \
	urlregex.c \
	urlregex.h \
	notification-menuitem.c \
	notification-menuitem.h \
	indicator-notifications.c \
	notification.c \
	notification.h
libayatana_notifications_la_CFLAGS = \
	$(INDICATOR_CFLAGS) \
	-Wall \
	-DG_LOG_DOMAIN=\"ayatana-indicator-notifications\"
libayatana_notifications_la_LIBADD = \
	$(INDICATOR_LIBS)
libayatana_notifications_la_LDFLAGS = \
	-module \
	-avoid-version

