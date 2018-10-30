#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "logging_lib.h"
#include "timeutils.h"
#include "iputils.h"

void print_error_message(char* message) {
	printf("ERROR: On ");
    print_time();
    printf(" %s\n", message);
}

void print_error_message_with_client_ip(struct sockaddr_storage client_ip, char* error_message) {
	int ip_length = ip_type_max_length(client_ip);
    char* ip = malloc(ip_length+1);
	ip_to_string((struct sockaddr*)&client_ip, ip);
	printf("ERROR: On ");
	print_time();
	printf(" %s, connected client ip=%s\n", error_message, ip);
	free(ip);
}

void print_login_info_message(struct sockaddr_storage client_ip, char* logged_in_username) {
    int ip_length = ip_type_max_length(client_ip);
    char* ip = malloc(ip_length+1);
    ip_to_string((struct sockaddr*)&client_ip, ip);
    print_info_starting_message();
    printf("client logged in: ip=%s, user=%s\n", ip, logged_in_username);
    free(ip);
}

void print_logout_info_message(struct sockaddr_storage client_ip, char* logged_in_username, int top_commands, 
	int retr_commands, int dele_commands) {

    int ip_length = ip_type_max_length(client_ip);
    char* ip = malloc(ip_length+1);
    ip_to_string((struct sockaddr*)&client_ip, ip);
    print_info_starting_message();
    printf("client logged out: ip=%s, user=%s, top=%d, retr=%d, dele=%d\n", ip, logged_in_username, 
    	top_commands, retr_commands, dele_commands);
    free(ip);
}

void print_info_starting_message() {
	printf("INFO: On ");
    print_time();
    printf(" ");
}