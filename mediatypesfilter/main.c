#include <stdlib.h>
#include <stdio.h>

int main(int argc, char const *argv[])
{
	char* filterMessage = getenv("FILTER_MESSAGE");
	char* censoredMediaTypes = getenv("FILTER_MEDIA");
	printf("%s\n", filterMessage);
	printf("%s\n", censoredMediaTypes);
	return 0;
}