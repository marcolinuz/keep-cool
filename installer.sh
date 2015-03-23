#!/bin/bash
# Keep-Cool Installer for systems without developer tools
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

echo "# ############################## #"
echo "# Installer script for ${EXEC} #"
echo "# ############################## #"

echo
echo "Step 1: Find the right Temeperature sensor."
echo "List sensors:"
./${EXEC} -L
SENSORS=$(./${EXEC} -L | awk '{print $1}')
KC_Sensor=$(./${EXEC} -t | sed -e "s/.*\(TC..\).*/\1/")

echo  
echo -n "Do you want to use \"${KC_Sensor}\" as default sensor (Y/n)? "
read Answer

if [ "${Answer}" -o "${Answer}" = "Y" -o "${Answer}" = "y" ]; then
	Sensor=${KC_Sensor}
else
	numChoices=0
	for s in ${SENSORS}; do
		numChoices=$(( ${numChoices} + 1))
	done
	echo
	PS3="Which sensor should be monitored (1-${numChoices})? "
	select opt in ${SENSORS}; do
		isValid=$(echo ${SENSORS}|sed -e "s/.*\(${opt}\).*/\1/")
		if [ -n "${isValid}" -a "${isValid}" = "${opt}" ]; then
			Sensor=${opt}
			break;
		elif [ -z "opt" ]; then
			Sensor=${defaultChoice}
			break
		else
			echo "Wrong choice.. retry."
		fi
	done
fi
echo "Okay, sensor ${Sensor} selected"

echo
echo "Step 2: Select the Algorithm:"
ALGORITHMS="Simple Quiet Conservative Balanced I-Balanced Wave"
PS3="Which algorithm should be used (1-6)? "
select opt in ${ALGORITHMS}; do
	isValid=$(echo ${ALGORITHMS}|sed -e "s/.*\(${opt}\).*/\1/")
	if [ -n "${isValid}" -a "${isValid}" = "${opt}" ]; then
		Algorithm=${opt}
		break;
	else
		echo "Wrong choice.. retry."
	fi
done
echo "Okay, ${Algorithm} algorithm selected"

MinTemp=0
MaxTemp=0
echo
echo "Step 3: Select the temeperatures range:"
while [ ${MinTemp} -lt 30 -o ${MinTemp} -gt 80 ]; do
	echo "Which is the Minimum temperature to start increase fan(s) speed?"
	echo -n "(Normally between 50 and 70°C, leave blank for 60°C): "
	read MinTemp
	if [ -z "${MinTemp}" ]; then
		MinTemp=60
		break
	fi
done
echo "Okay, starting temperature will be ${MinTemp}°C"

echo
while [ ${MaxTemp} -le ${MinTemp} -o ${MaxTemp} -ge 120 ]; do
	echo "Which is the Maximum Temperature to set fan(s) at full speed?"
	echo -n "(Normally between 80 and 95°C, leave blank for 92°C): "
	read MaxTemp
	if [ -z "${MaxTemp}" ]; then
		MaxTemp=92
		break
	fi
done
echo "Okay, maximum temperature will be ${MaxTemp}°C"

echo
echo "Step 4: Generating plist file with your settings.."
Command="./${EXEC} -g -T ${Sensor}"

case ${Algorithm} in
	Simple)
		Command="${Command} -a s"
		;;
	Quiet)
		Command="${Command} -a q"
		;;
	Conservative)
		Command="${Command} -a c"
		;;
	Balanced)
		Command="${Command} -a b"
		;;
	I-Balanced)
		Command="${Command} -a i"
		;;
	Wave)
		Command="${Command} -a w"
		;;
esac

if ! [ ${MinTemp} -eq 60 ]; then
	Command="${Command} -m ${MinTemp}"
fi

if ! [ ${MaxTemp} -eq 92 ]; then
	Command="${Command} -M ${MaxTemp}"
fi

${Command}

echo
echo -n "Step 5: Installing the service.. "
install -d ${PREFIX}/sbin
install ${EXEC} ${PREFIX}/sbin
install -m 0644 ${PLIST} ${LAUNCHD}/${PLIST}
launchctl load -w ${LAUNCHD}/${PLIST}
echo "done!"
