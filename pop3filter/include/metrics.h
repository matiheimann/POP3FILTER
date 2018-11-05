#ifndef METRICS_H
#define METRICS_H

typedef struct {
    unsigned int concurrentConnections;
    unsigned int historicConnections;
    unsigned int mailsFiltered;
    unsigned int bytesTransferred;
} metrics_st;

extern metrics_st * metrics;

void setMetrics();

#endif