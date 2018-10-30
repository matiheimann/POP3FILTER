#ifndef LOGGING_LIB_H
#define LOGGING_LIB_H

void print_error_message_with_client_ip(struct sockaddr_storage client_ip, char* error_message);
void print_error_message(char* message);
void print_login_info_message(struct sockaddr_storage client_ip, char* logged_in_username);
void print_info_starting_message();
void print_logout_info_message(struct sockaddr_storage client_ip, char* logged_in_username, int top_commands, 
	int retr_commands, int dele_commands);

#endif