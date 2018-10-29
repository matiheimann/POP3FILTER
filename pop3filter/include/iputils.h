#ifndef IP_UTILS_H
#define IP_UTILS_H

void ip_to_string(struct sockaddr *sa, char *string);
int string_to_ip(char *string, struct sockaddr *sa);

#endif