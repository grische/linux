// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#pragma GCC pop_options

#include "debug.h"
#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>

#include "consts.h"
#include "appl.h"
#include "ca.h"
#include "host.h"
#include "capi_pipe.h"
#include "capi_events.h"
#include "zugriff.h"
#include "local_capi.h"

/**
 */
static int atoi(char *p)
{
	unsigned int negativ = 0;
	signed int value = 0;

	if (*p == ' ')
		p++;
	if (*p == '+')
		p++;
	if (*p == '.') {
		negativ = 1;
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		value *= 10;
		value += *p - '0';
		p++;
	}
	return negativ ? -value : value;
}

/**
 */
void CAPI_INIT(void)
{
	LOCAL_CAPI_INIT(SOURCE_KERNEL_CAPI);
}
EXPORT_SYMBOL(CAPI_INIT);

/**
 */
unsigned int CAPI_REGISTER(unsigned int MessageBufferSize, unsigned int MaxNCCIs,
			   unsigned int WindowSize, unsigned int B3BlockSize, unsigned int *ApplId)
{
	return LOCAL_CAPI_REGISTER(SOURCE_KERNEL_CAPI, MessageBufferSize, MaxNCCIs, WindowSize, B3BlockSize, ApplId);
}
EXPORT_SYMBOL(CAPI_REGISTER);

/**
 */
unsigned int CAPI_RELEASE(unsigned int ApplId)
{
	return LOCAL_CAPI_RELEASE(SOURCE_KERNEL_CAPI, ApplId);
}
EXPORT_SYMBOL(CAPI_RELEASE);

/**
 */
unsigned int CAPI_GET_MESSAGE_WAIT_QUEUE(unsigned int ApplId, wait_queue_head_t *rx_wait_queue,
					 wait_queue_head_t *tx_wait_queue)
{
	return LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(SOURCE_KERNEL_CAPI, ApplId, rx_wait_queue, tx_wait_queue) != 0;
}
EXPORT_SYMBOL(CAPI_GET_MESSAGE_WAIT_QUEUE);

/**
 */
unsigned int CAPI_GET_MESSAGE(unsigned int ApplId, unsigned char **pCapiMessage, unsigned int Suspend)
{
	return LOCAL_CAPI_GET_MESSAGE(SOURCE_KERNEL_CAPI, ApplId, pCapiMessage, Suspend);
}
EXPORT_SYMBOL(CAPI_GET_MESSAGE);

/**
 */
unsigned int CAPI_PUT_MESSAGE(unsigned int ApplId, unsigned char *CapiMessage)
{
	return LOCAL_CAPI_PUT_MESSAGE(SOURCE_KERNEL_CAPI, ApplId, CapiMessage);
}
EXPORT_SYMBOL(CAPI_PUT_MESSAGE);

/**
 */
void CAPI_GET_MANUFACTURER(char *Buffer)
{
	strcpy(Buffer, "AVM Berlin");
}
EXPORT_SYMBOL(CAPI_GET_MANUFACTURER);

/**
 *	siehe implementation stack: cm.c
 */
#define VERSION_STRING_WORD_REVISION        0
#define VERSION_STRING_WORD_VERSION         1
#define VERSION_STRING_WORD_EEPROM          2
#define VERSION_STRING_WORD_SERIAL_NUMBER   3
#define VERSION_STRING_WORD_ADD_SERVICE     4
#define VERSION_STRING_WORD_PROTOCOLL       5
#define VERSION_STRING_WORD_PROFILE         6

/**
 */
unsigned char *LOCAL_CAPI_GET_VERSION_WORD(unsigned int Controller, unsigned int Word)
{
	unsigned char *Buffer = CAPI_Version + (Controller * 256) + 1;
	unsigned char *End    = Buffer + 256 - 1;

	while (Word && Buffer < End) {
		unsigned int Len = *Buffer;

		Buffer += Len + 1, Word--;
	}
	return Buffer;
}

/**
 */
unsigned int CAPI_GET_VERSION(unsigned int *pCAPIMajor,
			      unsigned int *pCAPIMinor,
			      unsigned int *pManufacturerMajor,
			      unsigned int *pManufacturerMinor)
{
	unsigned char *Version;
	unsigned int Major, Middle, Minor;
	unsigned int ManufacturerMajor, ManufacturerMinor;

	Version = LOCAL_CAPI_GET_VERSION_WORD(0, VERSION_STRING_WORD_REVISION);
	/*--- hat die Form: 3.01-11 ---*/
	DEB_INFO("%s: (len=%u) %s\n", __func__, Version[0], Version + 1);

	if (Version[0])
		Version++;  /*--- längenbyte überspringen wenn nicht 0 ---*/

	Major = atoi((char *)Version);
	while (*Version && *Version != '.')
		Version++;  /*--- alles bis zum Punkt ueberspringen ---*/
	while (*Version && *Version == '.')
		Version++;  /*--- Punkt ueberspringen ---*/
	Middle = atoi((char *)Version);
	while (*Version && *Version != '-')
		Version++;  /*--- alles bis zum Strich ueberspringen ---*/
	while (*Version && *Version == '-')
		Version++;  /*--- Strich ueberspringen ---*/
	Minor = atoi((char *)Version);

	copy_dword_to_le_unaligned((unsigned char *)pCAPIMajor, 2);
	copy_dword_to_le_unaligned((unsigned char *)pCAPIMinor, 0);

	ManufacturerMajor  = Major << 4;
	ManufacturerMinor  = Minor;
	ManufacturerMajor |= (Middle / 10);
	ManufacturerMinor |= (Middle % 10) << 4;

	copy_dword_to_le_unaligned((unsigned char *)pManufacturerMajor, ManufacturerMajor);
	copy_dword_to_le_unaligned((unsigned char *)pManufacturerMinor, ManufacturerMinor);

	return 0x0000;
}
EXPORT_SYMBOL(CAPI_GET_VERSION);

/**
 */
unsigned int CAPI_GET_SERIAL_NUMBER(unsigned int Controller, char *Buffer)
{
	unsigned char *pSerial;

	if (capi_oslib_stack == NULL) {
		memcpy(Buffer, "NoCAPI ", 8);
		DEB_INFO("%s: no capi\n", __func__);
		return ERR_IllegalController;
	}
	if (Controller == 0) {
		memcpy(Buffer, "0004711", 8);
		DEB_INFO("%s: illegal controller 0\n", __func__);
		return ERR_IllegalController;
	}
	Controller -= Karte;
	Controller--; /*--- intern wird ab 0 gezaehlt ---*/
	if (Controller >= capi_oslib_stack->controllers) {
		memcpy(Buffer, "Illegal", 8);
		DEB_INFO("%s: illegal controller 0x%x (count from 0)\n", __func__, Controller);
		return ERR_IllegalController;
	}
	pSerial = LOCAL_CAPI_GET_VERSION_WORD(Controller, VERSION_STRING_WORD_SERIAL_NUMBER);
	memcpy(Buffer, pSerial + 1, min_t(unsigned int, 8, *pSerial));
	return 0x0000;
}
EXPORT_SYMBOL(CAPI_GET_SERIAL_NUMBER);

/**
 */
unsigned int CAPI_GET_PROFILE(unsigned char *Buffer, unsigned int Controller)
{

	if (capi_oslib_stack == NULL)
		return ERR_IllegalController;
	if (Controller == 0) {
		if (capi_oslib_stack)
			Controller = capi_oslib_stack->controllers;
		else
			Controller = 0;
		SET_DWORD(Buffer, Controller);
		return 0x0000;
	}
	Controller -= Karte;
	Controller--; /*--- intern wird ab 0 gezaehlt ---*/
	if (Controller >= capi_oslib_stack->controllers) {
		DEB_INFO("%s: illegal controller 0x%x >= %d (count from 0)\n",
			 __func__,
			 Controller, capi_oslib_stack->controllers);
		return ERR_IllegalController;
	}
	memcpy(Buffer, capi_oslib_stack->cm_getprofile(Controller), 64);
	return 0x0000;
}
EXPORT_SYMBOL(CAPI_GET_PROFILE);

/**
 */
unsigned int CAPI_INSTALLED(unsigned int Controller)
{
	DEB_INFO("%s(Controller=%u)\n", __func__, Controller);
	Controller -= Karte;

	if (capi_oslib_stack == NULL)
		return ERR_IllegalController;

	if (Controller > capi_oslib_stack->controllers)
		return ERR_IllegalController;
	return ERR_NoError;
}
EXPORT_SYMBOL(CAPI_INSTALLED);
