#ifndef MAIN_H
#define MAIN_H

void setConfiguration(int argc, char* const argv[]);
void setErrorFile(char* file);
void printHelp();
int isANumericArgument(char* dir, char param);
void setPop3Direction(char* dir);
void setManagmentDirection(char* dir);
void setReplacementMessage(char* message);
void addCensoredMediaType(char* mediaType);
void setManagementPort(char* port);
void setLocalPort(char* port);
void setOriginPort(char* port);
void setCommand(char* cmd);
void printVersion();

#endif
