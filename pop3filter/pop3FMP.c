#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "pop3FMP.h"
#include "mediatypes.h"
#include "options.h"

uint8_t* receivePOP3FMPRequest(buffer* b, int* size)
{
	char* str = (char*) calloc(MAX_RESPONSE, sizeof(char));
	uint8_t* response = (uint8_t*) calloc(MAX_RESPONSE, sizeof(uint8_t));
	int strIndex = 0;
	
	int state = START;
	
	uint8_t firstByte = buffer_read(b);
	buffer_read_adv(b, 1);

	/** Version check */

	if(buffer_can_read(b))
	{
		*size =2; // Set size of response
		if(buffer_read(b) != 0x1)
		{
			response[0] = 0xFD; // VERSION NOT SUPPORTED
			response[1] = 0x1; // SUPPORTED VERSION
			free(str);
			return response;
		}
		else
		{
			response[1] = 0x1; //SUPPORTED VERSION
			transitions(firstByte, &state, response, size, str, &strIndex);
		}
		buffer_read_adv(b, 1);
	}
	else
	{
		state = END_BAD_REQUEST;
	}

	/** POP3FMP request parsing */

	while(buffer_can_read(b))
	{

		transitions(buffer_read(b), &state, response, size, str, &strIndex);
		buffer_read_adv(b, 1);
		if(((state == END  || 
			state == CLOSE_CONNECTION || 
			END_SET_MEDIATYPES || 
			END_SET_MESSAGE || 
			END_SET_FILTER) && buffer_can_read(b)) 
			|| state == END_BAD_REQUEST)
		{
			response[0] = 0xFF;
			response[1] = 0x1;
			*size = 2;
			break;
		}
		else if(state == CLOSE_CONNECTION)
		{
			//  TODO: close connection?
		}
		else if(state == END_SET_MEDIATYPES)
		{
			options->censoredMediaTypes = str;
		}
		else if(state == END_SET_MESSAGE)
		{
			setReplacementMessage(str);
		}
		else if(state == END_SET_FILTER)
		{
			options->command = str;
		}
	}
	free(str);
	return response;
}


int transitions(uint8_t feed, int* state, uint8_t* response, int * size, char* str, int* strIndex)
{
	switch(*state)
	{
		case START: if(feed == 0x01)
					{
						 response[0] = 0x01; //GET CONCURRENT CONNECTION
						 strcpy((char*)(response + *size), "012345");
						 *size += strlen("012345") + 1;
						 *state = END;
					}
					else if(feed == 0x02)
					{
						response[0] = 0x02; // HISTORICAL ACCESSES
						strcpy((char*)(response + *size), "012345");
						*size += strlen("012345") + 1;
						*state = END;
					}
					else if(feed == 0x03)
					{
						response[0] = 0x03; // TRANSFERED BYTES
						strcpy((char*)(response + *size), "012345");
						*size += strlen("012345") + 1;
						*state = END;
					}
					else if(feed == 0x04)
					{
						response[0] = 0x04; // FILTERED MAILS
						strcpy((char*)(response + *size), "012345");
						*size += strlen("012345") + 1;
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
						*state = END_BAD_REQUEST;
					}
					break;
		case GET: 	if(feed == 0x00)
					{
						strcpy((char*)(response + *size), options->censoredMediaTypes);
						*size += strlen(options->censoredMediaTypes) + 1;
						*state = END;
					}
					else if(feed == 0x01)
					{
						strcpy((char*)(response + *size), options->replacementMessage);
						*size += strlen(options->replacementMessage) + 1;
						*state = END;
					}
					else if(feed == 0x02)
					{
						strcpy((char*)(response + *size), options->command);
						*size += strlen(options->command) + 1;
						*state = END;
					}
					else
					{
						*state = END_BAD_REQUEST;
					}
					break;
		case SET_MEDIATYPES: 	if(feed == '\0')
								{
									str[*strIndex] = feed;
									if(checkMediaTypes(str))
									{
										strcpy((char*)(response + *size), "+OK");
										*size += strlen("+OK") + 1;
										*state = END_SET_MEDIATYPES;
										break;
									}
									else
									{
										*state = END_BAD_REQUEST;
									}
								}
								else
								{
									str[*strIndex] = feed;
									(*strIndex)++;
								}
								break;
		case SET_MESSAGE:	if(feed != '\0')
							{
								str[*strIndex] = feed;
								strcpy((char*)(response + *size), "+OK");
								*size += strlen("+OK") + 1;
								*state = END_SET_MESSAGE;
							}
							else
							{
								str[*strIndex] = feed;
								(*strIndex)++;
							}
							break;
		case SET_FILTER:	if(feed != '\0')
							{
								str[*strIndex] = feed;
								if(isValidFile(str))
								{
									strcpy((char*)(response + *size), "+OK");
									*size += strlen("+OK") + 1;
									*state = END_SET_FILTER;
								}
								else
								{
									*state = END_BAD_REQUEST;
								}
							}
							else
							{
								str[*strIndex] = feed;
								(*strIndex)++;
							}
							break;
		case USER: 	if(*strIndex < MAX_USER)
					{
						if(feed == '\0')
						{
							str[*strIndex] = feed;
							strcpy((char*)(response + *size), "+OK");
							*size += strlen("+OK") + 1;
							*state = END;
						}
						else if(feed > 0 && feed <= 128)
						{
							str[*strIndex] = feed;
							(*strIndex)++;
						}
						else
						{
							*state = END_BAD_REQUEST;
						}
					}
					else
					{
						*state = END_BAD_REQUEST;
					}
					break;
		case PASS:	if(*strIndex < MAX_USER)
					{
						if(feed == '\0')
						{
							str[*strIndex] = feed;
							strcpy((char*)(response + *size), "+OK");
							*size += strlen("+OK") + 1;
							*state = END;
						}
						else if(feed > 0 && feed <= 128)
						{
							str[*strIndex] = feed;
							(*strIndex)++;
						}
						else
							*state = END_BAD_REQUEST;
					}
					else
					{
						*state = END_BAD_REQUEST;
					}
					break;
		default: return 0;
	}
	return 1;
}



