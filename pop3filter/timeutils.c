#include <time.h>
#include <stdio.h>

#include "timeutils.h"

void print_time() {
	time_t t = time(NULL);
    struct tm time_manager = *localtime(&t);
    printf("%d-%d-%d %d:%d:%d", time_manager.tm_year + 1900, time_manager.tm_mon + 1, 
                time_manager.tm_mday, time_manager.tm_hour, time_manager.tm_min, time_manager.tm_sec);
}