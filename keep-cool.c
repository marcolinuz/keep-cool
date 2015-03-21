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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <IOKit/IOKitLib.h>
#include "keep-cool.h"
#include <libkern/OSAtomic.h>

// Cache the keyInfo to lower the energy impact of SMCReadKey() / SMCReadKey2()
#define KEY_INFO_CACHE_SIZE 100
struct {
    UInt32 key;
    SMCKeyData_keyInfo_t keyInfo;
} g_keyInfoCache[KEY_INFO_CACHE_SIZE];

int g_keyInfoCacheCount = 0;
OSSpinLock g_keyInfoSpinLock = 0;
KC_Status_t *gbl_state = NULL;

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure, io_connect_t conn);

#pragma mark C Helpers

UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++)
    {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
           total += ((unsigned char) (str[i]) << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c", 
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

float _strtof(unsigned char *str, int size, int e)
{
    float total = 0;
    int i;
    
    for (i = 0; i < size; i++)
    {
        if (i == (size - 1))
            total += (str[i] & 0xff) >> e;
        else
            total += str[i] << (size - 1 - i) * (8 - e);
    }
    
	total += (str[size-1] & 0x03) * 0.25;
    
    return total;
}


void printFP1F(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 32768.0);
}

void printFP4C(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 4096.0);
}

void printFP5B(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 2048.0);
}

void printFP6A(SMCVal_t val)
{
    printf("%.4f ", ntohs(*(UInt16*)val.bytes) / 1024.0);
}

void printFP79(SMCVal_t val)
{
    printf("%.4f ", ntohs(*(UInt16*)val.bytes) / 512.0);
}

void printFP88(SMCVal_t val)
{
    printf("%.3f ", ntohs(*(UInt16*)val.bytes) / 256.0);
}

void printFPA6(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 64.0);
}

void printFPC4(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 16.0);
}

void printFPE2(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 4.0);
}

void printUInt(SMCVal_t val)
{
    printf("%u ", (unsigned int) _strtoul((char *)val.bytes, val.dataSize, 10));
}

void printSP1E(SMCVal_t val)
{
    printf("%.5f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 16384.0);
}

void printSP3C(SMCVal_t val)
{
    printf("%.5f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 4096.0);
}

void printSP4B(SMCVal_t val)
{
    printf("%.4f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 2048.0);
}

void printSP5A(SMCVal_t val)
{
    printf("%.4f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 1024.0);
}

void printSP69(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 512.0);
}

void printSP78(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0);
}

void printSP87(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 128.0);
}

void printSP96(SMCVal_t val)
{
    printf("%.2f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 64.0);
}

void printSPB4(SMCVal_t val)
{
    printf("%.2f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 16.0);
}

void printSPF0(SMCVal_t val)
{
    printf("%.0f ", (float)ntohs(*(UInt16*)val.bytes));
}

void printSI8(SMCVal_t val)
{
    printf("%d ", (signed char)*val.bytes);
}

void printSI16(SMCVal_t val)
{
    printf("%d ", ntohs(*(SInt16*)val.bytes));
}

void printPWM(SMCVal_t val)
{
    printf("%.1f%% ", ntohs(*(UInt16*)val.bytes) * 100 / 65536.0);
}

void printBytesHex(SMCVal_t val)
{
    int i;

    printf("(bytes");
    for (i = 0; i < val.dataSize; i++)
        printf(" %02x", (unsigned char) val.bytes[i]);
    printf(")\n");
}

void printVal(SMCVal_t val)
{
    printf("  %-4s  [%-4s]  ", val.key, val.dataType);
    if (val.dataSize > 0)
    {
        if ((strcmp(val.dataType, DATATYPE_UINT8) == 0) ||
            (strcmp(val.dataType, DATATYPE_UINT16) == 0) ||
            (strcmp(val.dataType, DATATYPE_UINT32) == 0))
            printUInt(val);
        else if (strcmp(val.dataType, DATATYPE_FP1F) == 0 && val.dataSize == 2)
            printFP1F(val);
        else if (strcmp(val.dataType, DATATYPE_FP4C) == 0 && val.dataSize == 2)
            printFP4C(val);
        else if (strcmp(val.dataType, DATATYPE_FP5B) == 0 && val.dataSize == 2)
            printFP5B(val);
        else if (strcmp(val.dataType, DATATYPE_FP6A) == 0 && val.dataSize == 2)
            printFP6A(val);
        else if (strcmp(val.dataType, DATATYPE_FP79) == 0 && val.dataSize == 2)
            printFP79(val);
        else if (strcmp(val.dataType, DATATYPE_FP88) == 0 && val.dataSize == 2)
            printFP88(val);
        else if (strcmp(val.dataType, DATATYPE_FPA6) == 0 && val.dataSize == 2)
            printFPA6(val);
        else if (strcmp(val.dataType, DATATYPE_FPC4) == 0 && val.dataSize == 2)
            printFPC4(val);
        else if (strcmp(val.dataType, DATATYPE_FPE2) == 0 && val.dataSize == 2)
            printFPE2(val);
		else if (strcmp(val.dataType, DATATYPE_SP1E) == 0 && val.dataSize == 2)
			printSP1E(val);
		else if (strcmp(val.dataType, DATATYPE_SP3C) == 0 && val.dataSize == 2)
			printSP3C(val);
		else if (strcmp(val.dataType, DATATYPE_SP4B) == 0 && val.dataSize == 2)
			printSP4B(val);
		else if (strcmp(val.dataType, DATATYPE_SP5A) == 0 && val.dataSize == 2)
			printSP5A(val);
		else if (strcmp(val.dataType, DATATYPE_SP69) == 0 && val.dataSize == 2)
			printSP69(val);
		else if (strcmp(val.dataType, DATATYPE_SP78) == 0 && val.dataSize == 2)
			printSP78(val);
		else if (strcmp(val.dataType, DATATYPE_SP87) == 0 && val.dataSize == 2)
			printSP87(val);
		else if (strcmp(val.dataType, DATATYPE_SP96) == 0 && val.dataSize == 2)
			printSP96(val);
		else if (strcmp(val.dataType, DATATYPE_SPB4) == 0 && val.dataSize == 2)
			printSPB4(val);
		else if (strcmp(val.dataType, DATATYPE_SPF0) == 0 && val.dataSize == 2)
			printSPF0(val);
		else if (strcmp(val.dataType, DATATYPE_SI8) == 0 && val.dataSize == 1)
			printSI8(val);
		else if (strcmp(val.dataType, DATATYPE_SI16) == 0 && val.dataSize == 2)
			printSI16(val);
		else if (strcmp(val.dataType, DATATYPE_PWM) == 0 && val.dataSize == 2)
			printPWM(val);

        printBytesHex(val);
    }
    else
    {
            printf("no data\n");
    }
}

#pragma mark Shared SMC functions

kern_return_t SMCOpen(io_connect_t *conn)
{
    kern_return_t result;
    mach_port_t   masterPort;
    io_iterator_t iterator;
    io_object_t   device;
    
	IOMasterPort(MACH_PORT_NULL, &masterPort);
    
    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }
    
    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
    {
        printf("Error: no SMC found\n");
        return 1;
    }
    
    result = IOServiceOpen(device, mach_task_self(), 0, conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }
    
    return kIOReturnSuccess;
}

kern_return_t SMCClose(io_connect_t conn)
{
    return IOServiceClose(conn);
}

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure,io_connect_t conn)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;
    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);
    
    return IOConnectCallStructMethod(conn, index, inputStructure, structureInputSize, outputStructure, &structureOutputSize);
}

// Provides key info, using a cache to dramatically improve the energy impact of smcFanControl
kern_return_t SMCGetKeyInfo(UInt32 key, SMCKeyData_keyInfo_t* keyInfo, io_connect_t conn)
{
    SMCKeyData_t inputStructure;
    SMCKeyData_t outputStructure;
    kern_return_t result = kIOReturnSuccess;
    int i = 0;
    
    OSSpinLockLock(&g_keyInfoSpinLock);
    
    for (; i < g_keyInfoCacheCount; ++i)
    {
        if (key == g_keyInfoCache[i].key)
        {
            *keyInfo = g_keyInfoCache[i].keyInfo;
            break;
        }
    }
    
    if (i == g_keyInfoCacheCount)
    {
        // Not in cache, must look it up.
        memset(&inputStructure, 0, sizeof(inputStructure));
        memset(&outputStructure, 0, sizeof(outputStructure));
        
        inputStructure.key = key;
        inputStructure.data8 = SMC_CMD_READ_KEYINFO;
        
        result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure, conn);
        if (result == kIOReturnSuccess)
        {
            *keyInfo = outputStructure.keyInfo;
            if (g_keyInfoCacheCount < KEY_INFO_CACHE_SIZE)
            {
                g_keyInfoCache[g_keyInfoCacheCount].key = key;
                g_keyInfoCache[g_keyInfoCacheCount].keyInfo = outputStructure.keyInfo;
                ++g_keyInfoCacheCount;
            }
        }
    }
    
    OSSpinLockUnlock(&g_keyInfoSpinLock);
    
    return result;
}

kern_return_t SMCReadKey2(UInt32Char_t key, SMCVal_t *val,io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));
    
    inputStructure.key = _strtoul(key, 4, 16);
    /*sprintf(val->key, key);*/
    strncpy(val->key, key, sizeof(UInt32Char_t));
    val->key[sizeof(UInt32Char_t)-1] = '\0';
    
    result = SMCGetKeyInfo(inputStructure.key, &outputStructure.keyInfo, conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;
    
    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));
    
    return kIOReturnSuccess;
}

#pragma mark Command line only

io_connect_t g_conn = 0;

void smc_init(){
	SMCOpen(&g_conn);
}

void smc_close(){
	SMCClose(g_conn);
}

kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
    return SMCCall2(index, inputStructure, outputStructure, g_conn);
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val)
{
    return SMCReadKey2(key, val, g_conn);
}

kern_return_t SMCWriteKey2(SMCVal_t writeVal, io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    SMCVal_t      readVal;
    
    result = SMCReadKey2(writeVal.key, &readVal,conn);
    if (result != kIOReturnSuccess)
        return result;
    
    if (readVal.dataSize != writeVal.dataSize)
        return kIOReturnError;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    
    inputStructure.key = _strtoul(writeVal.key, 4, 16);
    inputStructure.data8 = SMC_CMD_WRITE_BYTES;
    inputStructure.keyInfo.dataSize = writeVal.dataSize;
    memcpy(inputStructure.bytes, writeVal.bytes, sizeof(writeVal.bytes));
    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
    
    if (result != kIOReturnSuccess)
        return result;
    return kIOReturnSuccess;
}

kern_return_t SMCWriteKey(SMCVal_t writeVal)
{
    return SMCWriteKey2(writeVal, g_conn);
}

UInt32 SMCReadIndexCount(void)
{
    SMCVal_t val;
    
    SMCReadKey("#KEY", &val);
    return _strtoul((char *)val.bytes, val.dataSize, 10);
}

kern_return_t SMCPrintAll(void)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    int           totalKeys, i;
    UInt32Char_t  key;
    SMCVal_t      val;
    
    totalKeys = SMCReadIndexCount();
    for (i = 0; i < totalKeys; i++)
    {
        memset(&inputStructure, 0, sizeof(SMCKeyData_t));
        memset(&outputStructure, 0, sizeof(SMCKeyData_t));
        memset(&val, 0, sizeof(SMCVal_t));
        
        inputStructure.data8 = SMC_CMD_READ_INDEX;
        inputStructure.data32 = i;
        
        result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
        if (result != kIOReturnSuccess)
            continue;
        
        _ultostr(key, outputStructure.key);
        
	if (key[0] == 'T') {
		SMCReadKey(key, &val);
		printVal(val);
	}
    }
    
    return kIOReturnSuccess;
}

kern_return_t SMCPrintFans(void)
{
    kern_return_t result;
    SMCVal_t      val;
    UInt32Char_t  key;
    int           totalFans, i;
    
    result = SMCReadKey("FNum", &val);
    if (result != kIOReturnSuccess)
        return kIOReturnError;
    
    totalFans = _strtoul((char *)val.bytes, val.dataSize, 10);
    printf("Total fans in system: %d\n", totalFans);
    
    for (i = 0; i < totalFans; i++)
    {
        printf("\nFan #%d:\n", i);
        sprintf(key, "F%dID", i);
        SMCReadKey(key, &val);
        printf("    Fan ID       : %s\n", val.bytes+4);
        sprintf(key, "F%dAc", i); 
        SMCReadKey(key, &val); 
        printf("    Actual speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
        sprintf(key, "F%dMn", i);
        SMCReadKey(key, &val);
        printf("    Minimum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));
        sprintf(key, "F%dMx", i);
        SMCReadKey(key, &val);
        printf("    Maximum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));
        sprintf(key, "F%dSf", i);
        SMCReadKey(key, &val);
        printf("    Safe speed   : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
        sprintf(key, "F%dTg", i);
        SMCReadKey(key, &val);
        printf("    Target speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
        SMCReadKey("FS! ", &val);
        if ((_strtoul((char *)val.bytes, 2, 16) & (1 << i)) == 0)
            printf("    Mode         : auto\n");
        else
            printf("    Mode         : forced\n");
    }
    
    return kIOReturnSuccess;
}

void usage(char* prog)
{
    printf("Keep-Cool! (Version %s)\n", VERSION);
    printf("Dynamically modifies Fan Speed in order to keep your MAC cooler\n");
    printf("NOTE: it won't harm your MAC cause it only modifies the Minimum Fan Speed.\n");
    printf("\nUsage:\n");
    printf("%s [options]\n", prog);
    printf("  -a <alg>   : selects fan speed computing alghoritm: (default is quadratic)\n");
    printf("      s    -> linear: speed increments constantly between t-min and t-max,\n");
    printf("                      this is the _Simple approach\n");
    printf("      c    -> logarithmic: speed increments quickly for lower temperatures\n");
    printf("                      but slowly for high temperatures, this is the\n");
    printf("                      _Conservative (or noisy) approach\n");
    printf("      q    -> quadratic: speed increments slowly for lower temperatures\n");
    printf("                      but quickly for high temperatures, this is the\n");
    printf("                      _Quiet or (noise-reduction) approach\n");
    printf("      b    -> cubic: speed increments quickly for lower temperatures, slowly\n");
    printf("                      for mid-range temperatures and fastly for high \n");
    printf("                      temperatures, this is a _Balanced compromise between\n");
    printf("                      the quiet and conservative approaches\n");
    printf("      i    -> i-cubic: speed increments slowly for lower temperatures, quickly\n");
    printf("                      for mid-range temperatures and slowly for high \n");
    printf("                      temperatures. This can be seen as an Inverse-Balanced\n");
    printf("                      approach, cause it's still a compromise between the\n");
    printf("                      quiet and conservative approaches, though it tends to\n");
    printf("                      be more quiet, especially for low temperatures.\n");
    printf("  -d         : enable debug mode, dump internal state and values\n");
    printf("  -f         : run forever (runs as daemon)\n");
    printf("  -g         : generates in the current directory the plist file required to\n");
    printf("               run as service using the same arguments passed from command line\n");
    printf("  -h         : prints this help\n");
    printf("  -l         : dump fan info decoded\n");
    printf("  -L         : list all SMC temperature sensors keys and values\n");
    printf("  -m <value> : set minimum temperature to start fan throttling (default %dºC)\n", KC_DEF_MIN_TEMP);
    printf("  -M <value> : set maximum temperature to set fan max speed (default %dºC)\n", KC_DEF_MAX_TEMP);
    printf("  -n         : dry run, do not actually modify fan speed\n");
    printf("  -r         : run once and exits\n");
    printf("  -s <value> : simulates temperature read as value (for testing purposes)\n");
    printf("  -t         : print current temperature\n");
    printf("  -T <key>   : uses the key as sensor for the temperature (default \"%s\")\n", KC_DEF_TEMP_KEY);
    printf("  -v         : print version\n");
    printf("\n");
}

kern_return_t SMCWriteSimple(UInt32Char_t key, char *wvalue, io_connect_t conn)
{
    kern_return_t result;
    SMCVal_t   val;
    int i;
    char c[3];
    for (i = 0; i < strlen(wvalue); i++)
    {
        sprintf(c, "%c%c", wvalue[i * 2], wvalue[(i * 2) + 1]);
        val.bytes[i] = (int) strtol(c, NULL, 16);
    }
    val.dataSize = i / 2;
    /*sprintf(val.key, key);*/
    strncpy(val.key, key, sizeof(UInt32Char_t));
    val.key[sizeof(UInt32Char_t)-1] = '\0';
    result = SMCWriteKey2(val, conn);
    if (result != kIOReturnSuccess)
        printf("Error: SMCWriteKey() = %08x\n", result);
	
    
    return result;
}

double SMCGetTemperature(char *key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
                // convert fp78 value to temperature
                int intValue = (val.bytes[0] * 256 + val.bytes[1]) >> 2;
                return intValue / 64.0;
            }
        }
    }
    // read failed
    return 0.0;
}

kern_return_t SMCCountFans(KC_Status_t *state) {
    kern_return_t result;
    SMCVal_t      val;
    
    result = SMCReadKey("FNum", &val);
    if (result != kIOReturnSuccess)
        return kIOReturnError;
    
    state->num_fans = _strtoul((char *)val.bytes, val.dataSize, 10);
    if (state->debug)
	printf("Number of Fans: %d\n", state->num_fans);
    
    return kIOReturnSuccess;
}

kern_return_t SMCUpdateFans(KC_Status_t *state) {
    kern_return_t result = kIOReturnSuccess;
    SMCVal_t      val;
    UInt32Char_t  key;
    int           i;
    
    for (i = 0; i < state->num_fans; i++)
    {
        sprintf(key, "F%dMn", i);
        result = SMCReadKey(key, &val);
        state->fan[i].min_speed=(UInt32)_strtof(val.bytes, val.dataSize, 2);

        sprintf(key, "F%dAc", i);
        result = SMCReadKey(key, &val);
        state->fan[i].current_speed=(UInt32)_strtof(val.bytes, val.dataSize, 2);

        if (state->debug)
	    printf("Fan [%d]: Min Speed = %d Current Speed = %d\n", i, state->fan[i].min_speed, state->fan[i].current_speed);
    }
    
    return result;
}

kern_return_t SMCSetFanSpeed(KC_Status_t *state) {
    kern_return_t result = kIOReturnSuccess;
    SMCVal_t      val;
    int           i;
    UInt16        newSpeed = (*state->compute_fan_speed)((void *)state);
    UInt16        byteVal = newSpeed << 2;
    char	  *value = (char *)&byteVal;

    if (state->debug)
        printf("HexValue = %x\n",byteVal);

    val.bytes[0] = (int)value[1];
    val.bytes[1] = (int)value[0];
    val.dataSize = sizeof(byteVal);

    for (i = 0; i < state->num_fans; i++)
    {
    	if (state->fan[i].min_speed != newSpeed) {
	    if (state->debug)
		printf("Changing speed of Fan[%d] from %d to %d\n",i,state->fan[i].min_speed,newSpeed);
	    state->fan[i].min_speed = newSpeed;
            sprintf(val.key, "F%dMn", i);
	    result = SMCWriteKey(val);
	} else {
	    if (state->debug)
		printf("No need to change min speed of Fan[%d]\n",i);
	}
    }
    return result;
}

UInt16 KCLinearSpeedAlghoritm(void *structure) {
    KC_Status_t	*state = (KC_Status_t *)structure;
    double curTemp = state->cur_temp;
    double minTemp = (double)state->min_temp;
    double maxTemp = (double)state->max_temp;
    UInt16 newSpeed = KC_SMC_DEF_SPEED;

    if (curTemp <= minTemp ) {
	newSpeed = KC_SMC_DEF_SPEED;
    } else if (curTemp > maxTemp) {
	newSpeed = KC_FAN_MAX_SPEED;
    } else {
	double delta_t = maxTemp-minTemp;
	double increment = (curTemp-minTemp)*(state->delta_v/delta_t);
	newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
    } 
    return newSpeed;
}

UInt16 KCLogarithmicSpeedAlghoritm(void *structure) {
    KC_Status_t	*state = (KC_Status_t *)structure;
    double curTemp = state->cur_temp;
    double minTemp = (double)state->min_temp;
    double maxTemp = (double)state->max_temp;
    UInt16 newSpeed = KC_SMC_DEF_SPEED;

    if (curTemp <= minTemp ) {
	newSpeed = KC_SMC_DEF_SPEED;
    } else if (curTemp > maxTemp) {
	newSpeed = KC_FAN_MAX_SPEED;
    } else {
	double delta_t = maxTemp-minTemp;
	double increment = (state->delta_v/2.0)*(2.0+log10((curTemp-minTemp)/delta_t));
	newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
    } 
    return newSpeed;
}

UInt16 KCQuadraticSpeedAlghoritm(void *structure) {
    KC_Status_t	*state = (KC_Status_t *)structure;
    double curTemp = state->cur_temp;
    double minTemp = (double)state->min_temp;
    double maxTemp = (double)state->max_temp;
    UInt16 newSpeed = KC_SMC_DEF_SPEED;

    if (curTemp <= minTemp ) {
	newSpeed = KC_SMC_DEF_SPEED;
    } else if (curTemp > maxTemp) {
	newSpeed = KC_FAN_MAX_SPEED;
    } else {
	double delta_t = maxTemp-minTemp;
	double increment = pow((curTemp-minTemp)*(sqrt(state->delta_v)/delta_t),2);
	newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
    } 
    return newSpeed;
}

UInt16 KCCubicSpeedAlghoritm(void *structure) {
    KC_Status_t	*state = (KC_Status_t *)structure;
    double curTemp = state->cur_temp;
    double minTemp = (double)state->min_temp;
    double maxTemp = (double)state->max_temp;
    UInt16 newSpeed = KC_SMC_DEF_SPEED;

    if (curTemp <= minTemp ) {
	newSpeed = KC_SMC_DEF_SPEED;
    } else if (curTemp > maxTemp) {
	newSpeed = KC_FAN_MAX_SPEED;
    } else {
	double delta_t = maxTemp-minTemp;
	double increment = (state->delta_v/2.0)*(1.0+pow(((2.0*(curTemp-minTemp))/delta_t)-1.0,3));
	newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
    } 
    return newSpeed;
}

UInt16 KCInverseCubicSpeedAlghoritm(void *structure) {
    KC_Status_t	*state = (KC_Status_t *)structure;
    double curTemp = state->cur_temp;
    double minTemp = (double)state->min_temp;
    double maxTemp = (double)state->max_temp;
    UInt16 newSpeed = KC_SMC_DEF_SPEED;

    if (curTemp <= minTemp ) {
	newSpeed = KC_SMC_DEF_SPEED;
    } else if (curTemp > maxTemp) {
	newSpeed = KC_FAN_MAX_SPEED;
    } else { 
        double delta_t = maxTemp-minTemp;
        double half_t = delta_t/2.0;
	double temp_idx = curTemp-minTemp;
        if (temp_idx <= half_t) {
	    double increment = (state->delta_v/2.0)*(pow(((2.0*temp_idx)/delta_t),3));
	    newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
        } else {
	    double increment = (state->delta_v/2.0)*(2.0+pow(((2.0*temp_idx)/delta_t)-2.0,3));
	    newSpeed = (UInt16)(KC_FAN_MIN_SPEED + (UInt32)increment);
	}
    }
    return newSpeed;
}

UInt16 KCResetSpeedAlghoritm(void *structure) {
    return (UInt16)KC_SMC_DEF_SPEED;
}

void KCSelectAlgothitm(char alg, KC_Status_t *state) {
    switch (alg) {
        case 's':
	    state->compute_fan_speed=&KCLinearSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Speed Computing Algorithm: linear (Simple)\n");
            break;
        case 'c':
	    state->compute_fan_speed=&KCLogarithmicSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Speed Computing Algorithm: logarithmic (Conservative)\n");
            break;
        case 'q':
	    state->compute_fan_speed=&KCQuadraticSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Speed Computing Algorithm: quadratic (Quiet)\n");
            break;
        case 'b':
	    state->compute_fan_speed=&KCCubicSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Speed Computing Algorithm: cubic (Balanced)\n");
            break;
        case 'i':
	    state->compute_fan_speed=&KCInverseCubicSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Speed Computing Algorithm: cosine (I-Balanced)\n");
            break;
        case 'r':
	    state->compute_fan_speed=&KCResetSpeedAlghoritm;
	    if (state->debug)
		printf("Selected Reset Speed Computing Algorithm\n");
            break;
    }
}

void KCRegisterSignalHandler() {
    if (signal(SIGHUP, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGHUP\n");
    if (signal(SIGUSR1, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGUSR1\n");
    if (signal(SIGUSR2, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGUSR2\n");
    if (signal(SIGINT, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGINT\n");
    if (signal(SIGTERM, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGTERM\n");
    if (signal(SIGQUIT, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGQUIT\n");
    if (signal(SIGABRT, KCSigHandler) == SIG_ERR)
	printf("\ncan't catch SIGABRT\n");
}

void KCSigHandler(int sigNum) {
    if (gbl_state->debug)
       printf("Received Signal %d\n",sigNum);
    switch (sigNum) {
        case SIGUSR1:
	    KCSelectAlgothitm('i', gbl_state);
            break;
        case SIGUSR2:
	    KCSelectAlgothitm('b', gbl_state);
            break;
        case SIGHUP:
	    KCSelectAlgothitm('q', gbl_state);
            break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
	case SIGABRT:
	    /* Reset SMC Min Speed before exit */
	    if (gbl_state->debug)
	       printf("Restoring SMC Fan Speed to default value.\n");
            KCSelectAlgothitm('r', gbl_state);
	    SMCSetFanSpeed(gbl_state);
	    if (gbl_state->debug)
	       printf("Bye.\n");
	    smc_close();
	    exit(0);
            break;
    }
}

kern_return_t KCDumpOptions(FILE *fp, KC_Status_t *state) {
	kern_return_t retVal = 0;

	if (strncmp(state->temp_key, KC_DEF_TEMP_KEY, sizeof(UInt32Char_t)) != 0) {
		fprintf(fp,"%s-T%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%s%s%s",KC_PLIST_PRE_ARGUMENT,state->temp_key,KC_PLIST_POST_ARGUMENT);
	}

	if (state->min_temp != KC_DEF_MIN_TEMP) {
		fprintf(fp,"%s-m%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%s%d%s",KC_PLIST_PRE_ARGUMENT,state->min_temp,KC_PLIST_POST_ARGUMENT);
	}

	if (state->max_temp != KC_DEF_MAX_TEMP) {
		fprintf(fp,"%s-M%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%s%d%s",KC_PLIST_PRE_ARGUMENT,state->max_temp,KC_PLIST_POST_ARGUMENT);
	}

	if (state->compute_fan_speed == &KCLinearSpeedAlghoritm) {
		fprintf(fp,"%s-a%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%ss%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
	} else if (state->compute_fan_speed == &KCLogarithmicSpeedAlghoritm) {
		fprintf(fp,"%s-a%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%sc%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
	} else if (state->compute_fan_speed == &KCCubicSpeedAlghoritm) {
		fprintf(fp,"%s-a%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
		fprintf(fp,"%sb%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
	} else if (state->compute_fan_speed == &KCInverseCubicSpeedAlghoritm) {
                fprintf(fp,"%s-a%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
                fprintf(fp,"%si%s",KC_PLIST_PRE_ARGUMENT,KC_PLIST_POST_ARGUMENT);
        }
	return retVal;
}

kern_return_t KCWritePlistFile(KC_Status_t *state) {
    kern_return_t retVal = 1;
    FILE *fp;
    fp = fopen(KC_PLIST_FILENAME, "w+");
    if (fp == NULL) {
    	return retVal;
    }

    fprintf(fp, KC_PLIST_HEADER);
    retVal = KCDumpOptions(fp, state);
    fprintf(fp, KC_PLIST_FOOTER);
    fclose(fp);

    return retVal;
}

int main(int argc, char *argv[])
{
    int c;
    extern char   *optarg;
    
    kern_return_t result;
    int           op = OP_NONE;

    KC_Status_t kc_state = { KC_DEF_TEMP_KEY, 
       			     KC_DEF_MIN_TEMP, 
			     KC_DEF_MAX_TEMP, 
			     KC_DEF_MIN_TEMP/2.0, 
			     (double)(KC_FAN_MAX_SPEED-KC_FAN_MIN_SPEED),
			     0,
                             { {KC_SMC_DEF_SPEED,0}, 
			       {KC_SMC_DEF_SPEED,0}, 
			       {KC_SMC_DEF_SPEED,0}, 
			       {KC_SMC_DEF_SPEED,0}, 
			       {KC_SMC_DEF_SPEED,0} },
			     (char)0,
			     (char)0,
			     &KCQuadraticSpeedAlghoritm};

    while ((c = getopt(argc, argv, "a:Lls:nrfvdtT:m:M:g")) != -1)
    {
        switch(c)
        {
	    case 'a':
	    	KCSelectAlgothitm(optarg[0], &kc_state);
		break;
            case 'L':
                op = OP_LIST;
                break;
            case 'l':
                op = OP_READ_FAN;
                break;
	    case 't':
	        op = OP_READ_TEMP;
		break;
            case 'g':
	        op = OP_GENERATE_PLIST;
		break;
            case 's':
	    	op = OP_SIMULATE;
                kc_state.cur_temp = strtod(optarg, NULL);
                break;
            case 'n':
                kc_state.dry_run = (char)1;
                break;
            case 'r':
                op = OP_RUNONCE;
                break;
            case 'f':
                op = OP_RUNFOREVER;
                break;
            case 'v':
                printf("%s\n", VERSION);
                return 0;
                break;

            case 'd':
                kc_state.debug = (char)1;
                break;
            case 'T': 
                strncpy(kc_state.temp_key, optarg, sizeof(kc_state.temp_key));   //fix for buffer overflow
                kc_state.temp_key[sizeof(kc_state.temp_key) - 1] = '\0';
                break;
            case 'm': 
                kc_state.min_temp = strtol(optarg, NULL, 10);
                break;
            case 'M': 
                kc_state.max_temp = strtol(optarg, NULL, 10);
                break;
            
            default:
                op = OP_NONE;
                break;
        }
    }
    
    if (op == OP_NONE)
    {
        usage(argv[0]);
        return 1;
    }
    
    smc_init();
    switch(op)
    {
        case OP_LIST:
            result = SMCPrintAll();
            if (result != kIOReturnSuccess)
                printf("Error: SMCPrintAll() = %08x\n", result);
            break;
        case OP_READ_FAN:
            result = SMCPrintFans();
            if (result != kIOReturnSuccess)
                printf("Error: SMCPrintFans() = %08x\n", result);
            break;

        case OP_READ_TEMP:
	    kc_state.cur_temp = SMCGetTemperature(kc_state.temp_key);
	    if (kc_state.cur_temp == 0.0 )
                    printf("Error: SMCGetTemperature() can't read value\n");
            else
		    printf("SMC Sensor %s: Temperature = %.2fºC\n",kc_state.temp_key,kc_state.cur_temp);
	    break;
	
	case OP_GENERATE_PLIST:
	    result = KCWritePlistFile(&kc_state);
	    if (result)
                    printf("Error: KCWritePlistFile() can't write file %s\n", KC_PLIST_FILENAME);
            else
		    printf("File %s written succesfully\nNow run \"sudo make install\" in order to install the service.\n",KC_PLIST_FILENAME);
	    break;

        case OP_RUNONCE:
	    kc_state.cur_temp = SMCGetTemperature(kc_state.temp_key);
	    if (kc_state.cur_temp == 0.0 ) {
                    printf("Error: SMCGetTemperature() can't read value from sensor %s\n",kc_state.temp_key);
		    break;
            }
        case OP_SIMULATE:
	    if (kc_state.debug)
	    	printf("Sensor %s, current temperature: %.2fºC\n",kc_state.temp_key, kc_state.cur_temp);

	    result = SMCCountFans(&kc_state);
            if (result != kIOReturnSuccess)
                printf("Error: SMCCountFans() = %08x\n", result);

	    result = SMCUpdateFans(&kc_state);
            if (result != kIOReturnSuccess)
                printf("Error: SMCUpdateFans() = %08x\n", result);

	    if (kc_state.debug)
	    	printf("Computed new fan speed: %d\n", (*kc_state.compute_fan_speed)((void *)&kc_state));

            if (!(kc_state.dry_run)) {
		    result = SMCSetFanSpeed(&kc_state);
                    if (result != kIOReturnSuccess)
                        printf("Error: SMCSetFanSpeed() = %08x\n", result);
	    }
            break;

        case OP_RUNFOREVER:
	    gbl_state = &kc_state;
            KCRegisterSignalHandler();

	    SMCCountFans(&kc_state);
	    while (OP_RUNFOREVER) {
	        kc_state.cur_temp = SMCGetTemperature(kc_state.temp_key);
	        if (kc_state.cur_temp == KC_ERROR_READING_TEMP) {
                    printf("Error: SMCGetTemperature() can't read value\n");
	            sleep(KC_UPDATE_PERIOD);
		    continue;
                } else if (kc_state.cur_temp >= KC_WAKEUP_IGNORE_TEMP) {
	            if (kc_state.debug)
	    	        printf("Ignoring Temperature reading from sensor %s (too high)\n..just awaken from stand-by?.\n",kc_state.temp_key);
	            sleep(KC_UPDATE_PERIOD);
		    continue;
		}

	        if (kc_state.debug)
	    	    printf("\nSensor %s, current temperature: %.2fºC\n",kc_state.temp_key, kc_state.cur_temp);

	        result = SMCCountFans(&kc_state);
                if (result != kIOReturnSuccess)
                    printf("Error: SMCCountFans() = %08x\n", result);

	        result = SMCUpdateFans(&kc_state);
                if (result != kIOReturnSuccess)
                    printf("Error: SMCUpdateFans() = %08x\n", result);

	        if (kc_state.debug)
	    	    printf("Computed new fan speed: %d\n", (*kc_state.compute_fan_speed)((void *)&kc_state));

                if (!(kc_state.dry_run)) {
		    result = SMCSetFanSpeed(&kc_state);
                    if (result != kIOReturnSuccess)
                        printf("Error: SMCSetFanSpeed() = %08x\n", result);
	        }
	        sleep(KC_UPDATE_PERIOD);
            }
            break;
    }
    
    smc_close();
    return 0;
}
