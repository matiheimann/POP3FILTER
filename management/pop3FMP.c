#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pop3FMP.h"
#include "buffer.h"

int receivePOP3FMPResponse(buffer *b)
{
	char str[2000]; // TODO: check max
	int stringIndex = 0; // automatically value is zero
	int state = START;

	while(buffer_can_read(b))
	{
		transitions(buffer_read(b), &state, str, &stringIndex);
		if((state == END && buffer_can_read(b)) || state == ERROR)
		{
			consumeBuffer(b);
			printf("SERVER ERROR\n");
			return 1;
		}
		if(state == END)
		{
			str[stringIndex] = '\0';
			if(strlen(str) == 0)
				printf("The field is empty\n");
			else
				printf("%s\n", str);
			stringIndex = 0;
			return 1;
		}
	}
	return 0;
}


void transitions(uint8_t feed, int* state, char* str, int* stringIndex)
{
	switch(*state)
	{
		case START: if(feed == 0x01 || feed == 0x2 || feed == 0x3 || feed == 0x4)
					{
						*state = VERSION_METRIC;
					}
					else if(feed == 0x80)
					{
						*state = VERSION_GET_SET;
					}
					else if(feed == 0x81)
					{
						*state = VERSION_GET_SET;
					}
					else if(feed == 0x83 || feed == 0x84)
					{
						*state = VERSION_AUTH;
					}
					else if(feed == 0x0)
					{

						*state = END;
					}
					else if(feed == 0xFF)
					{
						strcpy(str , "BAD REQUEST");
						*state = VERSION_BAD_REQUEST;
					}
					else if(feed == 0xFD)
					{
						*stringIndex = strlen("VERSION NOT SUPPORTED");
						strcpy(str , "VERSION NOT SUPPORTED");
						*state = VERSION_NOT_SUPPORTED;
					}
					else
					{
						*state = ERROR;
					}
					break;
		case VERSION_METRIC: 		if(feed == 0x01)
									{
										*state = STRING;
									}
									else
									{
										*state = ERROR;
									}
									break;
		case VERSION_GET_SET:		if(feed == 0x01)
									{
										*state = STRING;
									}
									else
									{
										*state = ERROR;
									}
									break;
		case VERSION_NOT_SUPPORTED: if(feed == 0x01)
									{
										*state = END;
									}
									else
									{
										*state = ERROR;
									}
									break;
		case VERSION_BAD_REQUEST:   if(feed == 0x01)
									{
										*state = END;
									}
									else
									{
										*state = ERROR;
									}
									break;
		case STRING:				if(feed == '\0')
									{
										*state = END;
									}
									else if(feed >= 32 && feed <= 126)
									{
										str[*stringIndex] = feed;
										(*stringIndex)++;
									}
									else
									{
										*state = ERROR;
									}
									break;	
	}
}


void consumeBuffer(buffer* b)
{
	while(buffer_can_read(b))
	{
		buffer_read_adv(b,1);
	}
}