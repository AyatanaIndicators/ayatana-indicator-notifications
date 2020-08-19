AC_INIT([ayatana-indicator-notifications], [0.8.0], [jason.conti@gmail.com])

AM_INIT_AUTOMAKE([-Wall -Werror])

AM_SILENT_RULES([yes])

# Required compilers

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AR
AC_STDC_HEADERS
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Setup gettext with intltool

AC_DEFUN([AC_DEFINE_PATH], [
	test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
	ac_define_path=`eval echo [$]$2`
	ac_define_path=`eval echo [$]ac_define_path`
	$1="$ac_define_path"
	AC_SUBST($1)
	ifelse($3, ,
		AC_DEFINE_UNQUOTED($1, "$ac_define_path"),
		AC_DEFINE_UNQUOTED($1, "$ac_define_path", $3))
])

IT_PROG_INTLTOOL
GETTEXT_PACKAGE=ayatana-indicator-notifications
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext package])
AC_DEFINE_PATH([LOCALEDIR], "${datadir}/locale", [locale directory])
AM_GLIB_GNU_GETTEXT

# Helpers for GSettings

GLIB_GSETTINGS

# Required libraries from pkg-config

INDICATOR_REQUIRED_VERSION=0.3.19
GTK3_REQUIRED_VERSION=3.0

INDICATOR3_PKG_NAME=ayatana-indicator3-0.4

PKG_CHECK_MODULES(INDICATOR, $INDICATOR3_PKG_NAME >= $INDICATOR_REQUIRED_VERSION
                             gtk+-3.0 >= GTK3_REQUIRED_VERSION)

AC_SUBST(INDICATOR_CFLAGS)
AC_SUBST(INDICATOR_LIBS)

# Library directories from pkg-config

INDICATORDIR=`$PKG_CONFIG --variable=indicatordir $INDICATOR3_PKG_NAME`
INDICATORICONSDIR="${datadir}/pixmaps/"

AC_SUBST(INDICATORDIR)
AC_SUBST(INDICATORICONSDIR)

PKG_CHECK_MODULES(SETTINGS, gtk+-3.0 >= GTK3_REQUIRED_VERSION)

AC_SUBST(SETTINGS_CFLAGS)
AC_SUBST(SETTINGS_LIBS)

#########################
# Check for systemd
#########################
PKG_CHECK_MODULES(SYSTEMD,  systemd,
    [has_systemd=yes],
    []
)
if test "x$has_systemd" = "xyes"; then
	SYSTEMD_USERDIR=`$PKG_CONFIG --variable=systemduserunitdir systemd`
	AC_SUBST(SYSTEMD_USERDIR)
	AC_DEFINE(HAVE_SYSTEMD, 1, [create ayatana-indicator-application.service for systemd])
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  data/Makefile
  data/icons/Makefile
  po/Makefile.in
  tests/Makefile
])

AC_OUTPUT

AC_MSG_NOTICE([

Recent Notifications Indicator Configuration:

	Prefix:        $prefix
	Indicator Dir: $INDICATORDIR
])
