#include <arpa/inet.h>
#include <string.h>

#include "iputils.h"

void ip_to_string(struct sockaddr_storage ip, char* string) {

	if(ip.ss_family == AF_INET) {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)&ip;
        struct in_addr ip_addr = ipv4_addr->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN );
     	memcpy(string, str, strlen(str));
    }
    else if(ip.ss_family == AF_INET6) {
        struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*)&ip;
        struct in6_addr ip_addr       = ipv6_addr->sin6_addr;
        char str[INET6_ADDRSTRLEN];
        inet_ntop( AF_INET6, &ip_addr, str, INET6_ADDRSTRLEN );
        memcpy(string, str, strlen(str));
    }
}