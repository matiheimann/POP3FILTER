#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>
#include "selector.h"
#include "filter.h"

struct executeFilterArguments {
	char* mailToFilter;
	char* commandToExecute;
	int childToParentPipeRead;
	int childToParentPipeWrite;
	int parentToChildPipeRead;
	int parentToChildPipeWrite;
};

//Handler para leer el mail filtrado por la aplicacion externa
const struct fd_handler mailFilterHandler = {
        .handle_read       = readFilteredMail,
        .handle_write      = NULL,
        .handle_close      = NULL, 
    };

//Handler que se llama al leer el pipe por donde esta el mail filtrado
void readFilteredMail(struct selector_key *key)
{
	printf("EN EL HANDLER\n");
	char buff[500] = {0};
	read(key->fd, buff, 500);
	printf("Mail filtrado: %s\n", buff);
	selector_set_interest_key(key, OP_NOOP);
}

unsigned filterMail(fd_selector selector, char* mail, char* command)
{
	pthread_t threadID;
	selector_status selectorStatus = SELECTOR_SUCCESS;

	int input_pipe[2];
	int output_pipe[2];

	if(pipe(input_pipe) == -1)
	{
		//Informar error en los logs
		return 0;
	}

	if(pipe(output_pipe) == -1)
	{
		//Informar error en los logs
		return 0;
	}

	if(selector_fd_set_nio(output_pipe[0]) == -1) //Extremo de lectura de pipe no bloqueante
	{
		//Informar error en los logs
		return selectorStatus;
	}

	selectorStatus = selector_register(selector, output_pipe[0], &mailFilterHandler, OP_READ, NULL);

	if(selectorStatus != SELECTOR_SUCCESS) {
        //Informar error en los logs
        return selectorStatus;
    }

    //Argumentos a pasarle al thread
	struct executeFilterArguments* args = (struct executeFilterArguments*)malloc(sizeof(struct executeFilterArguments));
	args->mailToFilter = mail;
	args->commandToExecute = command;
	args->parentToChildPipeRead = input_pipe[0];
	args->parentToChildPipeWrite = input_pipe[1];
	args->childToParentPipeRead = output_pipe[0];
	args->childToParentPipeWrite = output_pipe[1];

	pthread_create(&threadID, NULL, &executeFilter, (void *)args);
	return selectorStatus;
}

void* executeFilter(void* arguments) 
{

	struct executeFilterArguments* args = (struct executeFilterArguments*)arguments;
	char* mail = args->mailToFilter;
	char* command = args->commandToExecute;

	int pid = fork();
	if(pid == -1) 
	{
		//Informar error en los logs
		return NULL;
	}

	if(pid == 0) //Child process
	{
		close(args->childToParentPipeRead);
		close(args->parentToChildPipeWrite);

		dup2(args->parentToChildPipeRead, STDIN_FILENO);
		dup2(args->childToParentPipeWrite, STDOUT_FILENO);
		
		execl(command, command, (char*)NULL);

        exit(EXIT_SUCCESS);
	} 

	write(args->parentToChildPipeWrite, mail, strlen(mail));
	close(args->parentToChildPipeWrite);

	close(args->parentToChildPipeRead);

	wait(NULL);
	//close(args->childToParentPipeWrite); Este fd hay que cerrarlo en un estado del selector

	pthread_exit(NULL);
}