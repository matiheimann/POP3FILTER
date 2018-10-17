#ifndef FILTER_H
#define FILTER_H

unsigned filterMail(fd_selector selector, char* mail, char* command);
void* executeFilter(void* arguments);
void readFilteredMail(struct selector_key *key);

#endif