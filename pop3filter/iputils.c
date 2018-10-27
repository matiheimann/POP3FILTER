#include <arpa/inet.h>
#include <string.h>

#include "iputils.h"

void ip_to_string(struct sockaddr_storage ip, char* string) {

	if(ip.ss_family == AF_INET) {
        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&ip;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
     	memcpy(string, str, strlen(str));
    }
    else if(ip.ss_family == AF_INET6) {
        struct sockaddr_in6* pV6Addr = (struct sockaddr_in6*)&ip;
        struct in6_addr ipAddr       = pV6Addr->sin6_addr;
        char str[INET6_ADDRSTRLEN];
        inet_ntop( AF_INET6, &ipAddr, str, INET6_ADDRSTRLEN );
        memcpy(string, str, strlen(str));
    }
}