#include <stdio.h> 

#include "metrics.h"

metrics_st * metrics;

void setMetrics()
{
	metrics->concurrentConnections = 0;
    metrics->historicConnections = 0;
    metrics->mailsFiltered = 0;
    metrics->bytesTransferred = 0;
}