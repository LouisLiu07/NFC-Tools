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

#define MAXLINE 1024
#define MAX_FRAME_LEN 96
//#define MAX_DEVICE_COUNT 2

static uint8_t abtReaderRx[MAX_FRAME_LEN];
static uint8_t abtReaderRxPar[MAX_FRAME_LEN];
static int szReaderRx;
static uint8_t abtTagRx[MAX_FRAME_LEN];
static uint8_t abtTagRxPar[MAX_FRAME_LEN];
//static int szTagRxBits;
static nfc_device* pndReader;
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

long long current_timestamp()
{
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // caculate milliseconds
																	 // printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

static void
print_hex(const uint8_t* pbtData, const size_t szBytes)
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
main(int argc, char* argv[])
{
	int     arg;
	bool    quiet_output = false;
	const char* acLibnfcVersion = nfc_version();
	nfc_modulation nm;
	nm.nmt = NMT_ISO14443A;
	nm.nbr = NBR_106;
	nfc_target pnt;
	// Display libnfc version
	printf("%s uses libnfc %s\n", argv[0], acLibnfcVersion);

#ifdef WIN32
	signal(SIGINT, (void(__cdecl*)(int)) intr_hdlr);
#else
	signal(SIGINT, intr_hdlr);
#endif

	nfc_context* context;
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

	if (nfc_initiator_select_passive_target(pndReader, nm, NULL, 0, &pnt) < 0)
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
	print_hex(pnt.nti.nai.abtAtqa, 2);
	print_hex(pnt.nti.nai.abtUid, 4);
	printf("%d\n\n", pnt.nti.nai.btSak);
	//print_hex(info, 18);

	if ((nfc_device_set_property_bool(pndReader, NP_HANDLE_CRC, true) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_EASY_FRAMING, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_INFINITE_SELECT, true) < 0)) {
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}
	uint8_t buffer[MAX_FRAME_LEN];
	int total = 0;
	int count = 0;
	while (!quitting)
	{
		printf("Please input number. \n");
		scanf("%d", &count);

		if (count == 999)
		{
			quitting = true;
			count = 1;
		}
		for (size_t i = 0; i < count; i++)
		{
			buffer[0] = 0x60;
			buffer[1] = 0x00;
			for (size_t i = 0; i < 4; i++)
			{
				buffer[i + 2] = pnt.nti.nai.abtUid[i];
			}
			for (size_t i = 0; i < 6; i++)
			{
				buffer[i + 6] = 0xff;
			}
			iso14443a_crc_append(buffer, 12);
			print_hex(buffer, 14);
			long long currentTime = current_timestamp();
			int n = nfc_initiator_transceive_bytes(pndReader, buffer, 10, abtTagRx, sizeof(abtTagRx), 0);
			if (n > 0)
			{
				print_hex(abtTagRx, n);
				int interval = current_timestamp() - currentTime;
				total += interval;
				printf("Interval: %I64d\n", current_timestamp() - currentTime);
			}
			else
			{
				printf("TRANSCEIVE ERROR %d\n", n);
				//quitting = true;
			}
			/*
			buffer[0] = 0x30;
			buffer[1] = 0x00;
			iso14443a_crc_append(buffer, 2);
			print_hex(buffer, 4);
			n = nfc_initiator_transceive_bytes(pndReader, buffer, 4, abtTagRx, sizeof(abtTagRx), 0);
			if (n>0)
			{
				print_hex(abtTagRx, n);
			}
			else
			{
				printf("TRANSCEIVE ERROR %d\n", n);
				quitting = true;
			}
			*/
		}
		printf("Average interval: %dms. \n", total / count);
		total = 0;
	}

	/*
	for(int i = 2; i < 6; i++)
	{
	buffer[i] = pnt.nti.nai.abtUid[i-2];
	}
	*/



	//nfc_close(pndTag);
	nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_SUCCESS);
}
