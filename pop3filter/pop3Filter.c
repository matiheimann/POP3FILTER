#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 


static char* errorFile = "/dev/null";
static int helpFlag = 0;
static uint32_t pop3Direction = INADDR_ANY;
static uint32_t managmentDirection = INADDR_LOOPBACK;
static char* replacementMessage = "Parte reemplazada";
static char** mediaTypesCensurable = NULL;
static int managmentPort = 9090;
static int localPort = 1110;
static int originPort = 110;
static char* command = "";
static int printInformation = 0;

int main(int argc, char* const argv[])
{
	char c;
	while((c = getopt(argc, argv, "ehlLmMopPtv:")) != -1)
	{
		switch(c)
		{
			case 'e':
				break;
			case 'h':
				helpFlag = 1;
				break;
			case 'l':
				break;
			case 'L':
				break;
			case 'm':
				break;
			case 'M':
				break;
			case 'o':
				break;
			case 'p':
				break;
			case 'P':
				break;
			case 't':
				break;
			case 'v':
				printInformation = 1;
				break;
			default:
				break;
		}
	}
	return 0;
}