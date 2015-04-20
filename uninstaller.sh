#!/bin/bash
# Keep-Cool Uninstaller for systems without developer tools
# * Copyright (C) 2015 Marcolinuz (marcolinuz@gmail.com)
# *
# * This program is free software; you can redistribute it and/or
# * modify it under the terms of the GNU General Public License
# * as published by the Free Software Foundation; either version 2
# * of the License, or (at your option) any later version.
#
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
#
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

EXEC=keep-cool
PREFIX=/usr/local
PLIST=m.c.m.keepcool.plist
LAUNCHD=/Library/LaunchDaemons

if [[ "${USER}" != "root" ]] ; then
	echo
	echo "The uninstaller must run with root privileges."
	echo "Please insert your password."
	echo
	sudo "${0}" 
	if [ ${?} -eq 0 ]; then
		exit 0
	else
		echo "Operation aborted."
		exit 1
	fi
fi

# Uninstall keep-cool
echo "Uninstalling ${EXEC} service.."
if [ -f "${LAUNCHD}/${PLIST}" ]; then
	echo -n "disabling service and removing plist file.. "
	launchctl unload -w ${LAUNCHD}/${PLIST}
	rm -f ${LAUNCHD}/${PLIST}
	echo "done"
fi

if [ -f "${PREFIX}/sbin/${EXEC}" ]; then
	echo -n "removing ${EXEC} executable from ${PREFIX}/sbin.. "
	rm -f ${PREFIX}/sbin/${EXEC}
	echo "done"
fi
echo "Complete."
