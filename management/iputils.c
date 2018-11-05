#include <arpa/inet.h>
#include <string.h>

#include "iputils.h"

void ip_to_string(struct sockaddr *sa, char *string) {
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    string, INET_ADDRSTRLEN);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    string, INET6_ADDRSTRLEN);
            break;
        }
}

int string_to_ip(char *string, struct sockaddr *sa) {
    if (1 == inet_pton(AF_INET, string, &(((struct sockaddr_in *)sa)->sin_addr)))
        return AF_INET;
    if (1 == inet_pton(AF_INET6, string, &(((struct sockaddr_in6 *)sa)->sin6_addr)))
        return AF_INET6;
    return AF_UNSPEC;
}

int ip_type_max_length(struct sockaddr_storage ip) {
    return ip.ss_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN; 
}
