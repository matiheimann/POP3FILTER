#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pop3FMP.h"
#include "buffer.h"

static char str[2000]; // TODO: check max
static int stringIndex; // automatically value is zero
static int state = START;


int receivePOP3FMPResponse(buffer *b, int count)
{

	while(count > 0)
	{
		count--;
		transitions((unsigned char) buffer_read(b));
		buffer_read_adv(b, 1);
		if((state == END && count > 0) || state == ERROR)
		{
			printf("SERVER ERROR\n");
			return 1;
		}
		if(state == END)
		{
			str[stringIndex] = 0;
			printf("%s\n", str);
			stringIndex = 0;
			return 1;
		}
	}
	return 0;
}


void transitions(unsigned char feed)
{
	switch(state)
	{
		case START: if(feed == 0x01 || feed == 0x2 || feed == 0x3 || feed == 0x4)
					{
						state = VERSION_METRIC;
					}
					else if(feed == 0x80)
					{
						state = VERSION_GET_SET;
					}
					else if(feed == 0x81)
					{
						state = VERSION_GET_SET;
					}
					else if(feed == 0x83 || feed == 0x84)
					{
						state = VERSION_AUTH;
					}
					else if(feed == 0x0)
					{

						state = END;
					}
					else if(feed == 0xFF)
					{
						strcpy(str , "BAD REQUEST");
						state = VERSION_BAD_REQUEST;
					}
					else if(feed == 0xFD)
					{
						stringIndex = strlen("VERSION NOT SUPPORTED");
						strcpy(str , "VERSION NOT SUPPORTED");
						state = VERSION_NOT_SUPPORTED;
					}
					else
					{
						state = ERROR;
					}
					break;
		case VERSION_METRIC: 		if(feed == 0x01)
									{
										state = STRING;
									}
									else
									{
										state = ERROR;
									}
									break;
		case VERSION_GET_SET:		if(feed == 0x01)
									{
										state = STRING;
									}
									else
									{
										state = ERROR;
									}
									break;
		case VERSION_NOT_SUPPORTED: if(feed == 0x01)
									{
										state = END;
									}
									else
									{
										state = ERROR;
									}
									break;
		case VERSION_BAD_REQUEST:   if(feed == 0x01)
									{
										state = END;
									}
									else
									{
										state = ERROR;
									}
									break;
		case STRING:				if(feed == '\0')
									{
										state = END;
									}
									else if(feed == ',' || isalnum(feed))
									{
										str[stringIndex] = feed;
										stringIndex++;
									}
									else
									{
										state = ERROR;
									}
									break;	
	}
}
