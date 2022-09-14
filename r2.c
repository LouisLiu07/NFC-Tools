/*-
* Free/Libre Near Field Communication (NFC) library
*
* Libnfc historical contributors:
* Copyright (C) 2009      Roel Verdult
* Copyright (C) 2009-2013 Romuald Conty
* Copyright (C) 2010-2012 Romain Tartière
* Copyright (C) 2010-2013 Philippe Teuwen
* Copyright (C) 2012-2013 Ludovic Rousseau
* See AUTHORS file for a more comprehensive list of contributors.
* Additional contributors of this file:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*  1) Redistributions of source code must retain the above copyright notice,
*  this list of conditions and the following disclaimer.
*  2 )Redistributions in binary form must reproduce the above copyright
*  notice, this list of conditions and the following disclaimer in the
*  documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* Note that this license only applies on the examples, NFC library itself is under LGPL
*
*/

/**
* @file nfc-relay.c
* @brief Relay example using two devices.
*/
#define MINGW32

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <nfc/nfc.h>

//#include "utils/nfc-utils.h"

#include <unistd.h>
#ifdef __WIN32__
# include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#define MAXLINE 1024
#define MAX_FRAME_LEN 96
//#define MAX_DEVICE_COUNT 2

static uint8_t abtReaderRx[MAX_FRAME_LEN];
static uint8_t abtReaderRxPar[MAX_FRAME_LEN];
static int szReaderRx;
static uint8_t abtTagRx[MAX_FRAME_LEN];
static uint8_t abtTagRxPar[MAX_FRAME_LEN];
//static int szTagRxBits;
static nfc_device *pndReader;
//static nfc_device *pndTag;
static bool quitting = false;

static void
intr_hdlr(int sig)
{
	(void)sig;
	printf("\nQuitting...\n");
	quitting = true;
	return;
}

static void
print_hex(const uint8_t *pbtData, const size_t szBytes)
{
	size_t  szPos;

	for (szPos = 0; szPos < szBytes; szPos++) {
		printf("%02x  ", pbtData[szPos]);
		if ((szPos + 1) % 16 == 0)
		{
			printf("\r\n");
		}
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	int     arg;
	bool    quiet_output = false;
	const char *acLibnfcVersion = nfc_version();
	nfc_modulation nm;
	nm.nmt = NMT_ISO14443A;
	nm.nbr = NBR_106;
	nfc_target pnt;
	// Display libnfc version
	printf("%s uses libnfc %s\n", argv[0], acLibnfcVersion);

#ifdef WIN32
	signal(SIGINT, (void(__cdecl *)(int)) intr_hdlr);
#else
	signal(SIGINT, intr_hdlr);
#endif

	nfc_context *context;
	nfc_init(&context);
	if (context == NULL) {
		//ERR("Unable to init libnfc (malloc)");
		exit(EXIT_FAILURE);
	}

	// Try to open the NFC reader
	pndReader = nfc_open(context, NULL);
	if (pndReader == NULL) {
		printf("Error opening NFC reader device\n");
		//nfc_close(pndTag);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	printf("NFC reader device: %s opened\n", nfc_device_get_name(pndReader));
	printf("%s", "Configuring NFC reader settings...\n");

	if (nfc_initiator_init(pndReader) < 0) {
		//nfc_perror(pndReader, "nfc_initiator_init");
		//nfc_close(pndTag);
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	if (nfc_initiator_select_passive_target(pndReader, nm, NULL, 0, &pnt)<0)
	{
		printf("Select failed. \n");
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	printf("Initialization done. \n");
	/*
	printf("Card info: \n");
	print_hex(pnt.nti.nfi.abtId, 8);
	print_hex(pnt.nti.nfi.abtPad, 8);
	print_hex(pnt.nti.nfi.abtSysCode, 2);
	*/
	char info[18];
	for (int i = 0; i < 8; i++)
	{
		info[i] = pnt.nti.nfi.abtId[i];
	}
	for (int i = 0; i < 8; i++)
	{
		info[i + 8] = pnt.nti.nfi.abtPad[i];
	}
	info[16] = pnt.nti.nfi.abtSysCode[0];
	info[17] = pnt.nti.nfi.abtSysCode[1];
	printf("\n");
	//print_hex(info, 18);

	if ((nfc_device_set_property_bool(pndReader, NP_HANDLE_CRC, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_EASY_FRAMING, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_INFINITE_SELECT, true) < 0)) {
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}
	uint8_t buffer[MAX_FRAME_LEN];
	buffer[0] = 0x60;
	buffer[1] = 0x00;
	iso14443a_crc_append(buffer, 2);
	int n = nfc_initiator_transceive_bytes(pndReader,buffer,4,abtTagRx,sizeof(abtTagRx),0);
	if (n>0)
	{
		print_hex(abtTagRx, n);
	}
	else
	{
		printf("TRANSCEIVE ERROR %d\n", n);
	}
	//nfc_close(pndTag);
	nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_SUCCESS);
}
