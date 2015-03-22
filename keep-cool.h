/*
 * Apple System Management Control (SMC) Tool
 * Copyright (C) 2006 devnull 
 * Portions Copyright (C) 2013 Michael Wilber
 * Turned into a Fan Speed controller by Marcolinuz (marcolinuz@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __SMC_H__
#define __SMC_H__
#endif

#define VERSION               "0.03"

#define OP_NONE               0
#define OP_LIST               1
#define OP_READ_TEMP          2
#define OP_READ_FAN           3
#define OP_SIMULATE           4 
#define OP_RUNONCE            5
#define OP_RUNFOREVER         6
#define OP_GENERATE_PLIST     7

#define KERNEL_INDEX_SMC      2

#define SMC_CMD_READ_BYTES    5
#define SMC_CMD_WRITE_BYTES   6
#define SMC_CMD_READ_INDEX    8
#define SMC_CMD_READ_KEYINFO  9
#define SMC_CMD_READ_PLIMIT   11
#define SMC_CMD_READ_VERS     12

#define DATATYPE_FP1F         "fp1f"
#define DATATYPE_FP4C         "fp4c"
#define DATATYPE_FP5B         "fp5b"
#define DATATYPE_FP6A         "fp6a"
#define DATATYPE_FP79         "fp79"
#define DATATYPE_FP88         "fp88"
#define DATATYPE_FPA6         "fpa6"
#define DATATYPE_FPC4         "fpc4"
#define DATATYPE_FPE2         "fpe2"

#define DATATYPE_SP1E         "sp1e"
#define DATATYPE_SP3C         "sp3c"
#define DATATYPE_SP4B         "sp4b"
#define DATATYPE_SP5A         "sp5a"
#define DATATYPE_SP69         "sp69"
#define DATATYPE_SP78         "sp78"
#define DATATYPE_SP87         "sp87"
#define DATATYPE_SP96         "sp96"
#define DATATYPE_SPB4         "spb4"
#define DATATYPE_SPF0         "spf0"

#define DATATYPE_UINT8        "ui8 "
#define DATATYPE_UINT16       "ui16"
#define DATATYPE_UINT32       "ui32"

#define DATATYPE_SI8          "si8 "
#define DATATYPE_SI16         "si16"

#define DATATYPE_PWM          "{pwm"

#define KC_UPDATE_PERIOD        1
#define KC_MAX_FANS   		5
#define KC_FAN_MIN_SPEED     	2000
#define KC_FAN_MAX_SPEED     	6200
#define KC_SMC_DEF_SPEED     	0
#define KC_DEF_MIN_TEMP	 	60
#define KC_DEF_MAX_TEMP	 	92
#define KC_DEF_TEMP_KEY	 	"TCXC"
#define KC_ERROR_READING_TEMP   0.0
#define KC_WAKEUP_IGNORE_TEMP   120.0

#define KC_LOG_BUFSIZE		512
#define KC_PLIST_FILENAME	"m.c.m.keepcool.plist"
#define KC_PLIST_HEADER		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n\t<key>Disabled</key>\n\t<false/>\n\t<key>GroupName</key>\n\t<string>wheel</string>\n\t<key>UserName</key>\n\t<string>root</string>\n\t<key>KeepAlive</key>\n\t<true/>\n\t<key>Label</key>\n\t<string>m.c.m.keepcool</string>\n\t<key>ProgramArguments</key>\n\t<array>\n\t<string>/usr/local/sbin/keep-cool</string>\n\t\t<string>-f</string>\n"
#define KC_PLIST_PRE_ARGUMENT   "\t\t<string>"
#define KC_PLIST_POST_ARGUMENT  "</string>\n"
#define KC_PLIST_FOOTER         "\t</array>\n\t<key>RunAtLoad</key>\n\t<true/>\n</dict>\n</plist>"


typedef struct {
    char                  major;
    char                  minor;
    char                  build;
    char                  reserved[1]; 
    UInt16                release;
} SMCKeyData_vers_t;

typedef struct {
    UInt16                version;
    UInt16                length;
    UInt32                cpuPLimit;
    UInt32                gpuPLimit;
    UInt32                memPLimit;
} SMCKeyData_pLimitData_t;

typedef struct {
    UInt32                dataSize;
    UInt32                dataType;
    char                  dataAttributes;
} SMCKeyData_keyInfo_t;

typedef unsigned char              SMCBytes_t[32];

typedef struct {
  UInt32                  key; 
  SMCKeyData_vers_t       vers; 
  SMCKeyData_pLimitData_t pLimitData;
  SMCKeyData_keyInfo_t    keyInfo;
  char                    result;
  char                    status;
  char                    data8;
  UInt32                  data32;
  SMCBytes_t              bytes;
} SMCKeyData_t;

typedef char              UInt32Char_t[5];

typedef struct {
  UInt32Char_t            key;
  UInt32                  dataSize;
  UInt32Char_t            dataType;
  SMCBytes_t              bytes;
} SMCVal_t;

typedef struct {
  UInt32	min_speed;
  UInt32	current_speed;
} KC_FanState_t;

typedef struct {
  UInt32Char_t            temp_key;
  UInt32                  min_temp;
  UInt32                  max_temp;
  double		  cur_temp;
  double                  delta_v;
  UInt32		  num_fans;
  KC_FanState_t		  fan[KC_MAX_FANS];
  char			  debug;
  char			  dry_run;
  UInt16                  (*compute_fan_speed)(void *);
} KC_Status_t;


UInt32 _strtoul(char *str, int size, int base);
float _strtof(unsigned char *str, int size, int e);

void smc_init();
void smc_close();
kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val);
kern_return_t SMCWriteSimple(UInt32Char_t key,char *wvalue,io_connect_t conn);

kern_return_t SMCOpen(io_connect_t *conn);
kern_return_t SMCClose(io_connect_t conn);
kern_return_t SMCReadKey2(UInt32Char_t key, SMCVal_t *val,io_connect_t conn);

void KCRegisterSignalHandler();
void KCSigHandler(int);
void KCSelectAlgothitm(char, KC_Status_t *);
void KCSwitchAlgothitm(int, KC_Status_t *);
void KCSysLog(int, char *);
double SMCGetTemperature(char *);
kern_return_t KCWritePlistFile(KC_Status_t *);
kern_return_t KCDumpOptions(FILE *, KC_Status_t *);
kern_return_t SMCCountFans(KC_Status_t *);
kern_return_t SMCUpdateFans(KC_Status_t *);
kern_return_t SMCSetFanSpeed(KC_Status_t *);
UInt16 KCLinearSpeedAlghoritm(void *);
UInt16 KCLogarithmicSpeedAlghoritm(void *);
UInt16 KCQuadraticSpeedAlghoritm(void *);
UInt16 KCCubicSpeedAlghoritm(void *);
UInt16 KCInverseCubicSpeedAlghoritm(void *);
UInt16 KCWaveSpeedAlghoritm(void *);
UInt16 KCResetSpeedAlghoritm(void *);
