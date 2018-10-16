#include <string.h>
#include <ctype.h>

#include "mediatypes.h"

int isValidMediaType(char* mediatype)
{
	int i = 0;
	int slash = 0;
	int dotComma = 0;
	int equals = 0;

	while(mediatype[i] != 0)
	{
		if(!isalnum(mediatype[i]))
		{
			if(slash)
			{
				if(mediatype[i] == '*')
				{
					return (mediatype[i+1] == 0 && mediatype[i-1] == '/') ? 1 : 0;
				}
				else if(mediatype[i] == ';')
				{
					if(mediatype[i-1] != '/')
					{
						dotComma = 1;
					}
					else
					{
						return 0;
					}
				}
				else if(mediatype[i] == '=')
				{
					if(dotComma != 0 &&  mediatype[i-1] != ';')
					{
						equals = 1;
					}
					else
					{
						return 0;
					}
				}
				else
				{
					return 0;
				}
			}
			else
			{
				if(mediatype[i] == '/' && i != 0)
				{
					slash = 1;
				}
				else if(mediatype[i] == '*' && i == 0)
				{
					return (strcmp("*/*", mediatype) == 0) ? 1 : 0;
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

	if(dotComma != 0 && equals == 0)
	{
		return 0;
	}

	if(equals != 0 && mediatype[i-1] == '=')
	{
		return 0;
	}

	return 1;
}

