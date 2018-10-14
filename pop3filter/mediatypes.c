#include <string.h>
#include <ctype.h>

#include "mediatypes.h"

int isValidMediaType(char* mediatype)
{
	int i = 0;
	int slash = 0;

	while(mediatype[i] != 0)
	{
		if(!isalpha(mediatype[i]))
		{
			if(slash)
			{
				if(mediatype[i] == '*')
				{
					return (mediatype[i+1] == 0 && mediatype[i-1] == '/') ? 1 : 0;
				}
				return 0;
			}
			else
			{
				if(mediatype[i] == '/')
				{
					slash = 1;
				}
				else
				{
					return 0;
				}
			}
		}
		i++;
	}

	if(slash == 0)
	{
		return 0;
	}

	if(mediatype[i-1] == '/')
	{
		return 0;
	}

	return 1;
}