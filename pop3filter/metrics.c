#include <stdio.h> 
#include <stdlib.h> 

#include "metrics.h"

metrics_st * metrics;

void setMetrics()
{
	/* Initialize default values */
   	metrics = malloc(sizeof(*metrics));
	metrics->concurrentConnections = 0;
    metrics->historicConnections = 0;
    metrics->mailsFiltered = 0;
    metrics->bytesTransferred = 0;
}