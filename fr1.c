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
static uint8_t pbtRxPar[MAX_FRAME_LEN];
static uint8_t abtReaderRx[MAX_FRAME_LEN];
//static uint8_t abtReaderRxPar[MAX_FRAME_LEN];
static int szReaderRx;
static uint8_t abtTagRx[MAX_FRAME_LEN];
//static uint8_t abtTagRxPar[MAX_FRAME_LEN];
//static int szTagRxBits;
static nfc_device *pndReader;
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
print_hex(const uint8_t *pbtData, const size_t szBytes)
{
	size_t  szPos;

	for (szPos = 0; szPos < szBytes; szPos++) {
		printf("%02x  ", pbtData[szPos]);
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
	nm.nmt = NMT_FELICA;
	nm.nbr = NBR_212;
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

	uint8_t cmdRead[] = { 0x10, 0x06, 0x01, 0x01,  0x07,  0x01,  0xf4,  0x15,  0x87,  0x0f, 0x01, 0x17, 0x01, 0x01, 0x80, 0x00 };

	int x = nfc_initiator_transceive_bytes(pndReader, cmdRead, cmdRead[0], abtTagRx, sizeof(abtTagRx), 0);
	printf("Sent command. \n");
	//print_hex(cmdRead, cmdRead[0]);
	if (x > 0)
	{
		printf("%dBytes received from original tag \n", x);
		printf("Trans Res: \n");
		print_hex(abtTagRx, x);
		printf("\n");
	}
	else
	{
		printf("Trans to tag error. Error code: %d\n\n", x);
		//uint8_t data[] = { 0x0f,0x0f, 0x01, 0x01 };
	}

	if ((nfc_device_set_property_bool(pndReader, NP_HANDLE_CRC, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_HANDLE_PARITY, false) < 0) ||
		(nfc_device_set_property_bool(pndReader, NP_EASY_FRAMING, false) < 0)) {
		//nfc_perror(pndReader, "nfc_device_set_property_bool");
		//nfc_close(pndTag);
		nfc_close(pndReader);
		nfc_exit(context);
		exit(EXIT_FAILURE);
	}

	uint8_t buffer[] = { 0x11, 0x00, 0x80, 0x08, 0x00, 0x01 };

	int s = nfc_initiator_transceive_bits(pndReader, buffer, 48, pbtRxPar, abtReaderRx, szReaderRx, abtTagRx);

	if (s > 0)
	{
		printf("Success. ");
		print_hex(abtReaderRx, s / 8);
	}
	else
	{
		printf("Failed. Error code: %d\n", s);
	}
	//nfc_close(pndTag);
	nfc_close(pndReader);
	nfc_exit(context);
	exit(EXIT_SUCCESS);
}
