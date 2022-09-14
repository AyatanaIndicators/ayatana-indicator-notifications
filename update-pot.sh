#!/bin/bash

# Copyright (C) 2017 by Mike Gabriel <mike.gabriel@das-netzwerkteam.de>
#
# This package is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3 of the License.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>

GETTEXT_DOMAIN=$(cat CMakeLists.txt | grep 'set.*(.*GETTEXT_PACKAGE' | sed -r -e 's/.*\"([^"]+)\"\)/\1/')

cd po/ && intltool-update --gettext-package ${GETTEXT_DOMAIN} --pot && cd - 1>/dev/null

sed -E						\
    -e 's/\.xml\.in\.in.\h:/.xml.in.in:/g'	\
    -e 's/\.xml\.in\.h:/.xml.in:/g'		\
    -e 's/\.ini\.in\.h:/.ini.in:/g'		\
    -e 's/\.xml\.h:/.xml:/g'			\
    -e 's/\.ini\.h:/.ini:/g'			\
    -e 's@^#: \.\./@#: @g'			\
    -e 's@(:[0-9]+) \.\./@\1 @g'		\
    -i po/${GETTEXT_DOMAIN}.pot
