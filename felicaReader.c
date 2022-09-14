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
#define MAX_FRAME_LEN 196
//#define MAX_DEVICE_COUNT 2

static uint8_t abtReaderRx[MAX_FRAME_LEN];
//static uint8_t abtReaderRxPar[MAX_FRAME_LEN];
static int szReaderRx;
static uint8_t abtTagRx[MAX_FRAME_LEN];
//static uint8_t abtTagRxPar[MAX_FRAME_LEN];
//static int szTagRxBits;
static nfc_device* pndReader;
//static nfc_device *pndTag;
static bool quitting = false;
static int timeOut = 0;

long long current_timestamp()
{
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // caculate milliseconds
																	 // printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

static void
intr_hdlr(int sig)
{
	(void)sig;
	printf("\nQuitting...\n");
	quitting = true;
	return;
}

static void
print_hex(const uint8_t* pbtData, const size_t szBytes)
{
	size_t  szPos;

	for (szPos = 0; szPos < szBytes; szPos++) {
		printf("%02x  ", pbtData[szPos]);
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
	nm.nmt = NMT_FELICA;
	nm.nbr = NBR_212;
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
	/*
	int e;
	if ((e=nfc_initiator_poll_target(pndReader, &nm, 1, 2, 2, &pnt))<0)
	{
	printf("Poll failed. %d\n", e);
	nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_FAILURE);
	}
	*/
	printf("Initialization done. \n");

	print_hex(pnt.nti.nfi.abtId, 8);
	print_hex(pnt.nti.nfi.abtPad, 8);
	print_hex(pnt.nti.nfi.abtSysCode, 2);
	printf("\n");
	if ((nfc_device_set_property_bool(pndReader, NP_HANDLE_CRC, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_INFINITE_SELECT, true) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_EASY_FRAMING, false) < 0)) {
		//nfc_perror(pndReader, "nfc_device_set_property_bool");
		//nfc_close(pndTag);
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}
	int count = 0;
	int total = 0;
	while (!quitting) {
		//int count = 0;
		printf("Please input number. \n");
		scanf("%d", &count);

		if (count == 999)
		{
			quitting = true;
			count = 1;
		}
		long long currentTime = current_timestamp();
		/*
		uint8_t buffer[] = { 0x06, 0x00, 0x80, 0x08, 0x00, 0x01 };
		for (int i = 0; i < 6; i++)
		{
			abtReaderRx[i] = buffer[i];
		}
		printf("Sent command. \n");
		print_hex(abtReaderRx, 6);
		szReaderRx = 6;
		*/
		/*
		// Forward the frame to the original tag
		int x = nfc_initiator_transceive_bytes(pndReader, buffer, 6, abtTagRx, sizeof(abtTagRx), timeOut);
		if (x > 0) {
			printf("%d Bytes received from original tag \n", x);
			// Print the tag frame to the screen
			printf("Trans Res: \n");
			print_hex(abtTagRx, abtTagRx[0]);
			printf("Interval: %I64d\n", current_timestamp() - currentTime);
			printf("\n");
		}
		else
		{
			printf("Trans to tag error. Error code: %d\n\n", x);
			//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
		}
		*/
		/*
		uint8_t cmdRE[] = { 0x0a,  0x04,  0x01, 0x01,  0x07,  0x01,  0xf4,  0x15,  0x87,  0x0f };
		for (int i = 0; i < 8; i++)
		{
			cmdRE[i + 2] = pnt.nti.nfi.abtId[i];
		}
		currentTime = current_timestamp();
		x = nfc_initiator_transceive_bytes(pndReader, cmdRE, cmdRE[0], abtTagRx, sizeof(abtTagRx), timeOut);
		printf("Sent command. \n");
		print_hex(cmdRE, cmdRE[0]);
		if (x > 0)
		{
			printf("%d Bytes received from original tag \n", x);
			printf("Trans Res: \n");
			print_hex(abtTagRx, x);
			printf("Interval: %I64d\n", current_timestamp() - currentTime);
			printf("\n");
		}
		else
		{
			printf("Trans to tag error. Error code: %d\n\n", x);
			//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
		}

		uint8_t cmdRS[] = { 0x1d, 0x02, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f, 0x09, 0xff, 0xff, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x10, 0x01, 0x08, 0x03, 0x0c, 0x04, 0x08, 0x09, 0x08, 0x07 };
		for (int i = 0; i < 8; i++)
		{
			cmdRS[i + 2] = pnt.nti.nfi.abtId[i];
		}
		currentTime = current_timestamp();
		x = nfc_initiator_transceive_bytes(pndReader, cmdRS, cmdRS[0], abtTagRx, sizeof(abtTagRx), timeOut);
		printf("Sent command. \n");
		print_hex(cmdRS, cmdRS[0]);
		if (x > 0)
		{
			printf("%d Bytes received from original tag \n", x);
			printf("Trans Res: \n");
			print_hex(abtTagRx, x);
			printf("Interval: %I64d\n", current_timestamp() - currentTime);
			printf("\n");
		}
		else
		{
			printf("Trans to tag error. Error code: %d\n\n", x);
			//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
		}
		*/

		for (size_t i = 0; i < count; i++)
		{
			uint8_t cmdRead[] = { 0x19, 0x06, 0x01, 0x01,  0x07,  0x01,  0xf4,  0x15,  0x87,  0x0f};
			for (int i = 0; i < 8; i++)
			{
				cmdRead[i + 2] = pnt.nti.nfi.abtId[i];
			}
			currentTime = current_timestamp();
			int x = nfc_initiator_transceive_bytes(pndReader, cmdRead, cmdRead[0], abtTagRx, sizeof(abtTagRx), timeOut);
			printf("Sent command. \n");
			print_hex(cmdRead, cmdRead[0]);
			if (x > 0)
			{
				printf("%dBytes received from original tag \n", x);
				printf("Trans Res: \n");
				print_hex(abtTagRx, x);
				int interval = current_timestamp() - currentTime;
				total += interval;
				printf("Interval: %I64d\n", current_timestamp() - currentTime);
				printf("\n");
			}
			else
			{
				printf("Trans to tag error. Error code: %d\n\n", x);
				//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
			}
		}
		printf("Average interval: %dms. \n", total / count);
		total = 0;
		/*
		uint8_t cmdA[] = { 0x0c, 0x0a, 0x01, 0x01, 0x07, 0x01, 0xf4, 0x15, 0x87, 0x0f };
		for (int i = 0; i < 8; i++)
		{
			cmdA[i + 2] = pnt.nti.nfi.abtId[i];
		}
		currentTime = current_timestamp();
		x = nfc_initiator_transceive_bytes(pndReader, cmdA, cmdA[0], abtTagRx, sizeof(abtTagRx), timeOut);
		printf("Sent command. \n");
		print_hex(cmdA, cmdA[0]);
		if (x > 0)
		{
			printf("%dBytes received from original tag \n", x);
			printf("Trans Res: \n");
			print_hex(abtTagRx, x);
			printf("Interval: %I64d\n", current_timestamp() - currentTime);
			printf("\n");
		}
		else
		{
			printf("Trans to tag error. Error code: %d\n\n", x);
			//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
		}
		*/
		//quitting = true;
		/*
		if (count > 5)
		{
		quitting = true;
		}
		count++;
		*/
	}

	//nfc_close(pndTag);
	nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_SUCCESS);
}
