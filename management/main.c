#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/sctp.h> 
#include <string.h>
#include <ctype.h> 
#include <unistd.h>
#include <sys/types.h>

#include "options.h"
#include "buffer.h"
#include "managementParser.h"
#include "pop3FMP.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

static bool done = false;
static buffer* b;
static uint8_t raw_buff[1024];

int main(int argc, char* const argv[])
{
	setConfiguration(argc, argv);

	int connection;
	int conn;

	int send;

	const char* err_msg = NULL;
	uint8_t *ptr; //buffer pointer
	size_t  count;
	ssize_t  n;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family 		= AF_INET;
	addr.sin_addr.s_addr 	= htonl(options->managementDirection); // TODO:
	addr.sin_port 			= htons(options->managementPort); // TODO:

	printf("Welcome to the management tool.\n");
	printf("You can use it to get metrics or change the proxy pop3 filter settings.\n");

	connection = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if(connection == -1)
	{
		 err_msg = "unable to create socket";
		 goto finally;
	}

	conn = connect(connection, (struct sockaddr *) &addr, sizeof(addr));
	if(conn == -1)
	{
		err_msg = "unable to connect";
		goto finally;
	}
	printf("Please enter a command and press enter.\n-help to ask for help\n");
	buffer_init(b,  N(raw_buff), raw_buff);
	while(!done)
	{
		int dataSize;
		unsigned char* request = readCommand(&dataSize);
		if(request != NULL)
		{
			send = sctp_sendmsg(connection, (void*) request, dataSize,
								NULL, 0, 0, 0, 0, 0, 0);
			if(send == -1)
			{
				err_msg = "unable to send SCTP msg";
				goto finally;
			}
			ptr = buffer_write_ptr(b, &count);
			n = sctp_recvmsg(connection, ptr, count, NULL, 0 , 0, 0);
			
			if(n > 0)
			{
				buffer_write_adv(b, n);
				receivePOP3FMPResponse(b, n);
			}
			else
			{
				err_msg = "unable to receive SCTP msg";
				goto finally;
			}
		}
	}

	int ret;

finally:
	if(err_msg != NULL)
	{
		perror(err_msg);
		ret = 1;
	} 
	else 
	{
		ret = 0;
		printf("Management client closing safely...\n");
	}
	if(connection >= 0)
		close(connection);
	return ret;
}
