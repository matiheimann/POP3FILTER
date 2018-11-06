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

void getMetricAsString(char* metric, unsigned int count)
{
	int strIndex = 0;
	unsigned int transformed = count;
	do{
		strIndex++;
		transformed = transformed/10;
	}while(transformed);
	metric[strIndex] = '\0';
	do{
		strIndex--;
		metric[strIndex] = (count%10) + '0';
		count = count / 10;
	}while(count);
}
