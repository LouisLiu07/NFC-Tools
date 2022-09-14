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

	nfc_target nt =
	{
		.nm =
	{
		.nmt = NMT_ISO14443A,
		.nbr = NBR_UNDEFINED
	},
		.nti =
	{
		.nai =
	{
		.abtAtqa = { 0x00, 0x04 },
		.abtUid = { 0x08, 0xab, 0xcd, 0xef },
		.btSak = 0x09,
		.szUidLen = 4,
		.szAtsLen = 0,
	},
	},
	};
	/*
	printf("Card info: \n");
	print_hex(nt.nti.nfi.abtId, 8);
	print_hex(nt.nti.nfi.abtPad, 8);
	print_hex(nt.nti.nfi.abtSysCode, 2);
	*/
	printf("%s", "Configuring emulator settings...\n");

	printf("Initializing NFC emulator\n");
	if ((szReaderRx = (nfc_target_init(pndTag, &nt, abtReaderRx, sizeof(abtReaderRx), 0))) < 0) {
		printf("%s", "Initialization of NFC emulator failed\n");
		printf("Error code: %d\n", szReaderRx);
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
			int s = nfc_target_send_bytes(pndTag, abtReaderRx, szReaderRx, 0);
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
