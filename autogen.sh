#!/bin/sh

PKG_NAME="ayatana-indicator-notifications"

which mate-autogen || {
	echo "You need mate-common from https://git.mate-desktop.org"
	exit 1
}

USE_GNOME2_MACROS=1 \
. mate-autogen $@
