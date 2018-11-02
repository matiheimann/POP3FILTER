#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pop3FMP.h"





uint8_t* receivePOP3FMPRequest(buffer* b, int* size)
{
	char* string = (uint8_t*) calloc(MAX_RESPONSE, sizeof(uint8_t));
	uint8_t* response = (uint8_t*) calloc(MAX_RESPONSE, sizeof(uint8_t));
	int responseIndex = 2;
	int strIndex = 0;
	
	int state = START;
	
	unsigned char firstByte = (unsigned char) buffer_read(b);
	buffer_read_adv(b, 1);

	/** Version check */

	if(buffer_can_read(b))
	{
		if(buffer_read(b) != 0x1)
		{
			response[0] = 0xFD; //VERSION NOT SUPPORTED
			response[1] = 0x1; //SUPPORTED VERSION
			return response;
		}
		else
		{
			response[1] = 0x1; //SUPPORTED VERSION
			transitions(firstByte);
		}
		buffer_read_adv(b, 1);
	}
	else
	{
		state = VERSION_BAD_REQUEST;
	}

	/** POP3FMP request parsing */

	while(buffer_can_read(b))
	{

		transitions(buffer_read(b), &state, str, strIndex);
		buffer_read_adv(b, 1);
		if((state == END && buffer_can_read(b)) || state == VERSION_BAD_REQUEST)
		{
			response[0] = 0xFF;
			response[1] = 0x1;
		}
	}
	return response;
}


void transitions(uint8_t* feed, int* state, char* str, strIndex)
{
	switch(*state)
	{
		case START: if(feed == 0x01)
					{
						 response[0] = 0x01; //GET CONCURRENT CONNECTION
						 strcpy((char*)(response+responseIndex), "012345");
						 *state = END;
					}
					else if(feed == 0x02)
					{
						response[0] = 0x02; // HISTORICAL ACCESSES
						strcpy((char*)(response + responseIndex), "012345");
						*state = END;
					}
					else if(feed == 0x03)
					{
						response[0] = 0x03; // TRANSFERED BYTES
						strcpy((char*)(response + responseIndex), "012345");
						*state = END;
					}
					else if(feed == 0x04)
					{
						response[0] = 0x04; // FILTERED MAILS
						strcpy((char*)(response + responseIndex), "012345");
						*state = END;
					}
					else if(feed == 0x80)
					{
						response[0] = 0x80;
						*state = GET;
					}
					else if(feed == 0x81)
					{
						response[0] = 0X81;
						*state = SET;
					}
					else if(feed == 0x83)
					{
						response[0] = 0x83;
						*state = USER;
					}
					else if(feed == 0x84)
					{
						response[0] = 0x84;
						*state = PASS;
					}
					else if(feed == 0x0)
					{
						response[0] = 0x0;
						*state = CLOSE_CONNECTION;
					}
					else
					{
						*state = VERSION_BAD_REQUEST;
					}
					break;
		case GET: 	if(feed == 0x00)
					{
						strcpy((char*)(response + responseIndex), options->censoredMediaTypes);
						responseIndex += strlen(options->censoredMediaTypes) + 1;
						*state = END;
					}
					else if(feed == 0x01)
					{
						strcpy((char*)(response + responseIndex), options->replacementMessage);
						responseIndex += strlen(options->replacementMessage) + 1;
						*state = END;
					}
					else if(feed == 0x02)
					{
						strcpy(response + responseIndex, options->command);
						responseIndex += strlen(options->command) + 1;
						*state = END;
					}
					else
					{
						*state = VERSION_BAD_REQUEST;
					}
					break;
		case SET_MEDIATYPES: 
								if(feed != '\0')
								{isValidMediaType(char* mediatype)
									str[stringIndex] = feed;
									stringIndex++;
								}
								break;
		case SET_MESSAGE:	if()
							{

							}
							break;
		case SET_FILTER:	if

		case default: break;





	}
}



