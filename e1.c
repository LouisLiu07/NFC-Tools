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
#define LISTENQ 10
#define MAX_FRAME_LEN 96
/*
static uint8_t IDm[] = { 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f };
static uint8_t PMm[] = { 0x01, 0x20, 0x22, 0x04, 0x27, 0x67, 0x4e, 0xff };
static uint8_t SC[] = { 0x80, 0x08 };
static uint8_t PREAMBLE[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb2, 0x4d };
*/
static uint8_t pbtRxPar[MAX_FRAME_LEN];
static uint8_t abtReaderRx[MAX_FRAME_LEN];
static int szReaderRx;
static nfc_device *pndTag;
static bool quitting = false;

static void
intr_hdlr(int sig)
{
	(void)sig;
	printf("\nQuitting...\n");
	quitting = true;
	return;
}

long long current_timestamp()
{
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // caculate milliseconds
																	 // printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

static void
print_hex(const uint8_t *pbtData, const size_t szBytes)
{
	size_t  szPos;

	for (szPos = 0; szPos < szBytes; szPos++) {
		printf("%02x  ", pbtData[szPos]);
		if ((szPos + 1) % 8 == 0)
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
		printf("Unable to init libnfc (malloc)");
		exit(EXIT_FAILURE);
	}

	// Try to open the NFC emulator device
	pndTag = nfc_open(context, NULL);
	if (pndTag == NULL) {
		printf("Error opening NFC emulator device");
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	nfc_target nt = {
		.nm = {
		.nmt = NMT_FELICA,
		.nbr = NBR_212
	},
		.nti = {
		.nfi = {
		.abtId = { 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f },
		.abtPad = { 0x01, 0x20, 0x22, 0x04, 0x27, 0x67, 0x4e, 0xff },
		.abtSysCode = { 0x80, 0x08 },
	},
	},
	};

	printf("Card info: \n");
	print_hex(nt.nti.nfi.abtId, 8);
	print_hex(nt.nti.nfi.abtPad, 8);
	print_hex(nt.nti.nfi.abtSysCode, 2);
	printf("%s", "Configuring emulator settings...\n");

	printf("Initializing NFC emulator\n");
	if ((szReaderRx = (nfc_target_init(pndTag, &nt, abtReaderRx, sizeof(abtReaderRx), 0))) < 0) {
		printf("%s", "Initialization of NFC emulator failed");
		nfc_close(pndTag);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	if ((nfc_device_set_property_bool(pndTag, NP_HANDLE_CRC, true) < 0) ||
		(nfc_device_set_property_bool(pndTag, NP_INFINITE_SELECT, false) < 0)) {
		//nfc_perror(pndTag, "nfc_device_set_property_bool");
		nfc_close(pndTag);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	while (!quitting)
	{
		long long currentTime = current_timestamp();
		// Test if we received a frame from the reader
		printf("Trying to receive from reader. \n");

		if (szReaderRx > 0)
		{
			// Print the reader frame to the screen

			printf("%d bytes from original Reader. \n", szReaderRx);

			print_hex(abtReaderRx, szReaderRx);
			printf("\n");
			if (abtReaderRx[1] == 0x00 && abtReaderRx[2] == 0x80 && abtReaderRx[3] == 0x08)
			{
				if (abtReaderRx[4] == 0x01)
				{
					uint8_t sendData[20];
					sendData[0] = 0x12;
					sendData[1] = 0x01;
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 2] = nt.nti.nfi.abtId[i];
					}
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 10] = nt.nti.nfi.abtPad[i];
					}
					sendData[18] = 0x80;
					sendData[19] = 0x08;
					int s = nfc_target_send_bytes(pndTag, sendData, 20, 0);
					printf("Res polling \n");
					print_hex(abtReaderRx, szReaderRx);
					printf("%d bytes sent to reader. \n", s);
					print_hex(sendData, 20);
				}
				else
				{
					uint8_t sendData[18];
					sendData[0] = 0x12;
					sendData[1] = 0x01;
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 2] = nt.nti.nfi.abtId[i];
					}
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 10] = nt.nti.nfi.abtPad[i];
					}
					int s = nfc_target_send_bytes(pndTag, sendData, 18, 0);
					printf("Res polling \n");
					print_hex(abtReaderRx, szReaderRx);
					printf("%d bytes sent to reader. \n", s);
					print_hex(sendData, 18);
				}
				
			}
			else if (abtReaderRx[1] == 0x00 && abtReaderRx[2] == 0xff && abtReaderRx[3] == 0xff)
			{
				if (abtReaderRx[4] == 0x01)
				{
					uint8_t sendData[20];
					sendData[0] = 0x12;
					sendData[1] = 0x01;
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 2] = nt.nti.nfi.abtId[i];
					}
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 10] = nt.nti.nfi.abtPad[i];
					}
					sendData[18] = 0x80;
					sendData[19] = 0x08;
					int s = nfc_target_send_bytes(pndTag, sendData, 20, 0);
					printf("Res polling \n");
					print_hex(abtReaderRx, szReaderRx);
					printf("%d bytes sent to reader. \n", s);
					print_hex(sendData, 20);
				}
				else
				{
					uint8_t sendData[18];
					sendData[0] = 0x12;
					sendData[1] = 0x01;
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 2] = nt.nti.nfi.abtId[i];
					}
					for (int i = 0; i < 8; i++)
					{
						sendData[i + 10] = nt.nti.nfi.abtPad[i];
					}
					int s = nfc_target_send_bytes(pndTag, sendData, 18, 0);
					printf("Res polling \n");
					print_hex(abtReaderRx, szReaderRx);
					printf("%d bytes sent to reader. \n", s);
					print_hex(sendData, 18);
				}
			}
			else if (abtReaderRx[1] == 0x04)
			{
				uint8_t sendData[] = { 0x0b,  0x05,  0x01,  0x01,  0x07,  0x01,  0xf4,  0x15,  0x87,  0x0f,  0x00 };
				int s = nfc_target_send_bytes(pndTag, sendData, 11, 0);
				printf("Res card exist \n");
				//print_hex(abtReaderRx, szReaderRx);
				if (s > 0)
				{
					printf("%d bytes sent to reader. \n", s);
					print_hex(sendData, 11);
					printf("\n");
				}
				else
				{
					printf("Send to reader failed. \n");
					quitting = true;
				}
			}
			else if (abtReaderRx[1] == 0x02)
			{
				uint8_t buffer[] = { 0x1d, 0x03, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f, 0x09, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00 };

				int length = buffer[0];
				print_hex(buffer, length);

				int s = nfc_target_send_bytes(pndTag, buffer, (uint8_t)length, -1);
				if (s > 0) {
					printf("%d bytes sent to reader. \n\n", s);
				}
				else
				{
					printf("Send to reader failed. \n");
					quitting = true;
				}
			}
			else if (abtReaderRx[1] == 0x06)
			{
				uint8_t buffer[] = { 0x1d, 0x07, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x8f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09 };

				int length = buffer[0];
				print_hex(buffer, length);

				int s = nfc_target_send_bytes(pndTag, buffer, (uint8_t)length, -1);
				if (s > 0)
				{
					printf("%d bytes sent to reader. \n\n", s);
				}
				else
				{
					printf("Send to reader failed. \n");
					quitting = true;
				}
			}
			else if (abtReaderRx[1] == 0x0c)
			{
				uint8_t buffer[] = { 0x0d, 0x0d, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f, 0x01, 0x80, 0x08 };
				int length = buffer[0];
				print_hex(buffer, length);

				int s = nfc_target_send_bytes(pndTag, buffer, (uint8_t)length, -1);
				if (s > 0) {
					printf("%d bytes sent to reader. \n\n", s);
				}
				else
				{
					printf("Send to reader failed. \n");
					quitting = true;
				}
			}
			else if (abtReaderRx[1] == 0x0a)
			{
				uint8_t buffer[] = { 0x0c, 0x0b, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f, 0xff, 0xff };
				int length = buffer[0];
				print_hex(buffer, length);

				int s = nfc_target_send_bytes(pndTag, buffer, (uint8_t)length, -1);
				if (s > 0) {
					printf("%d bytes sent to reader. \n\n", s);
				}
				else
				{
					printf("Send to reader failed. \n");
					quitting = true;
				}
			}
			else
			{
				printf("Unknown command. \n");
				print_hex(abtReaderRx, szReaderRx);
				//quitting = true;
			}
		}
		else
		{
			printf("Failed to receive data from reader. \n");
			quitting = true;
		}
		szReaderRx = nfc_target_receive_bytes(pndTag, abtReaderRx, sizeof(abtReaderRx), 0);
#ifdef WIN32
		printf("Interval between sending to and receiving from reader: %I64d\n\n", current_timestamp() - currentTime);
#else
		printf("Interval between sending to and receiving from reader: %llu\n\n", current_timestamp() - currentTime);
#endif
		//szReaderRx = nfc_target_receive_bits(pndTag, abtReaderRx, sizeof(abtReaderRx), pbtRxPar);
	}

	nfc_close(pndTag);
	//nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_SUCCESS);
}
