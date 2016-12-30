//chris willette
//christopher.willette@gmail.com

/* Icmp ping flood program in winsock */
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define ICMP_ECHOREPLY 0
#define ICMP_ECHOREQ 8
#include "stdio.h"
#include "winsock2.h"
#include "conio.h"
#include "stdint.h"
#include "windows.h"
#include <string>
#include <sstream>
#include <WS2tcpip.h>
#include <time.h>
#include <stdio.h>

#pragma comment(lib,"ws2_32.lib") //winsock 2.2 library

#define ICMP_ECHO       8   /* Echo Request         */

unsigned short in_cksum(unsigned short *ptr, int nbytes);

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;

struct icmphdr
{
	u_int8_t type;      /* message type */
	u_int8_t code;      /* type sub-code */
	u_int16_t checksum;
	union
	{
		struct
		{
			u_int16_t   id;
			u_int16_t   sequence;
		} echo;         /* echo datagram */
		u_int32_t   gateway;    /* gateway address */
		struct
		{
			u_int16_t   __unused;
			u_int16_t   mtu;
		} frag;         /* path mtu discovery */
	} un;
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("\n Usage: %s <hostname> \n", argv[0]);
		exit(1);
	}
	char *packet, *data = NULL;
	SOCKET s;
	int k = 1, packet_size, payload_size = 512, sent = 0;

	struct icmphdr *icmph = NULL;

	WSADATA wsock;
	DWORD dwError;
	int i = 0;

	struct hostent *remoteHost;
	char *host_name;
	struct in_addr addr;

	// Initialize Winsock
	printf("Initialising Winsock...\r\n");
	if (WSAStartup(MAKEWORD(2, 2), &wsock) != 0)
	{
		fprintf(stderr, "WSAStartup() failed");
		exit(EXIT_FAILURE);
	}
	printf("Done\r\n");

	host_name = argv[1];

	// If the user input is an alpha name for the host, use gethostbyname()
	// If not, get host by addr (assume IPv4)
	if (isalpha(host_name[0])) {        /* host address is a name */
		//printf("Calling gethostbyname with %s\n", host_name);
		remoteHost = gethostbyname(host_name);
	}
	else {
		//printf("Calling gethostbyaddr with %s\n", host_name);
		addr.s_addr = inet_addr(host_name);
		if (addr.s_addr == INADDR_NONE) {
			printf("The IPv4 address entered must be a legal address\n");
			return 1;
		}
		else
			remoteHost = gethostbyaddr((char *)&addr, 4, AF_INET);
	}

	if (remoteHost == NULL) {
		dwError = WSAGetLastError();
		if (dwError != 0) {
			if (dwError == WSAHOST_NOT_FOUND) {
				printf("Host not found\n");
				return 1;
			}
			else if (dwError == WSANO_DATA) {
				printf("No data record found\n");
				return 1;
			}
			else {
				printf("Function failed with error: %ld\n", dwError);
				return 1;
			}
		}
	}

	//most of the above comes from
	//https://msdn.microsoft.com/en-us/library/windows/desktop/ms738552(v=vs.85).aspx

	//Create Raw ICMP Packet
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == SOCKET_ERROR)
	{
		printf("Failed to create raw icmp packet");
		exit(EXIT_FAILURE);
	}
	/*
	dest.sin_family = AF_INET;	
	dest.sin_addr.s_addr = inet_addr("216.58.193.78");
	*/
	struct sockaddr_in dest;
	dest.sin_family = remoteHost->h_addrtype;
	dest.sin_port = 0;
	dest.sin_addr.s_addr = *(long*)remoteHost->h_addr;
	//the above socket casting courtesy of tyler vanderhoef

	packet_size = sizeof(struct icmphdr) + payload_size;
	packet = (char *)malloc(packet_size);

	//zero out the packet buffer
	memset(packet, 0, packet_size);

	icmph = (struct icmphdr*) packet;
	icmph->type = ICMP_ECHO;
	icmph->code = 0;
	icmph->un.echo.sequence = rand();
	icmph->un.echo.id = rand();

	// Initialize the payload to some rubbish
	data = packet + sizeof(struct icmphdr);
	memset(data, '^', payload_size);

	//checksum
	icmph->checksum = 0;
	icmph->checksum = in_cksum((unsigned short *)icmph, packet_size);

	printf("\nSending packets...\n");
	int times[3] = { 0 };
	while (1)
	{
		clock_t startTime = clock();
		clock_t delta;
		if (sendto(s, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) == SOCKET_ERROR)
		{
			printf("Error sending Packet : %d", WSAGetLastError());
			break;
		}
		
		//read the response
		char buf[1024] = {'\0'};
		struct sockaddr_storage hostAddr;
		socklen_t length = sizeof(hostAddr);
		int reads = recvfrom(s, buf, 1024, 0, (struct sockaddr *)&hostAddr, &length);

		//calculate the RTT
		delta = clock() - startTime;
		int rtt = delta * 1000 / CLOCKS_PER_SEC;

		//save the RTTs
		times[sent] = rtt;
		++sent;
		
		if (reads == -1) {
			printf("Error receiving packet: %d\r\n", WSAGetLastError());
			--sent;
			continue;
		}		
		printf("Received %ld bytes from %s after %dms\r\n", (long)reads, argv[1], rtt);
		//keep going if not reached 3 packet sends.
		if (sent == 3) {
			break;
		}
	}

	int min = 0, max = 0, avg = 0;
	for (int i = 0; i < 3; i++) {
		if (min == 0) {
			min = times[i];
		}
		if (max == 0) {
			max = times[i];
		}
		if (times[i] < min) min = times[i];
		if (times[i] > max) max = times[i];
		avg += times[i];
	}
	avg = avg / 3;
	printf("Min RTT: %dms, Max RTT: %dms, Average RTT: %dms\r\n", min, max, avg);
	return 0;
}

/* Function calculate checksum */
unsigned short in_cksum(unsigned short *ptr, int nbytes)
{
	register long sum;
	u_short oddbyte;
	register u_short answer;

	sum = 0;
	while (nbytes > 1) {
		sum += *ptr++;
		nbytes -= 2;
	}

	if (nbytes == 1) {
		oddbyte = 0;
		*((u_char *)& oddbyte) = *(u_char *)ptr;
		sum += oddbyte;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;

	return (answer);
}
