#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <errno.h>

#include <sys/wait.h>

int main(int argc, char *argv[]){
	srand(time(NULL));
	// Check args
	if(argc != 2){
		printf("Usage: ./route [Program]\n");
		return -1;
	}
	// Generate test data
	char ques[1024];
	char fileName[9];
	char method[5];
	memset(method, '\0', 5);
	if(rand()%2){
		strcpy(method, "GET");
	}else{
		strcpy(method, "POST");
	}
	int version = rand()%2;
	memset(fileName, '\0', 9);
	for(int i=0; i < (rand() % 8 + 1); ++i){
		fileName[i] = "0123456789abcdefghijklmnopqrstuvwxyz"[rand() % 36];
	}
	sprintf(ques, "%s /%s HTTP/1.%d\r\n", method, strcat(fileName, ".html"), version);
	printf("Question: %s", ques);
	// pipe
	int inp[2], oup[2];
	pipe(inp);
	pipe(oup);
	int pid = fork();
	if(pid == 0){
		// Child
		close(inp[1]);
		close(oup[0]);
		dup2(inp[0], STDIN_FILENO);
		dup2(oup[1], STDOUT_FILENO);
		execlp(argv[1], argv[1], NULL);
	}else{
		// Parent
		close(inp[0]);
		close(oup[1]);
		if(write(inp[1], ques, strlen(ques)) < 0){
			close(inp[1]);
			close(oup[0]);
			perror("write() error");
			return -1;
		}
		close(inp[1]);
		// Wait child
		waitpid(pid, NULL, 0);
		// Read child
		memset(ques, '\0', 1024);
		read(oup[0], ques, 1024);
		printf("Answer:\n%s\n", ques);
		char rmeth[5];
		char rname[9];
		int rv;
		sscanf(ques, "%s\n%s\nHTTP/1.%d", rmeth, rname, &rv);
		if(!strcmp(rmeth, method) && !strcmp(rname, fileName) && rv == version){
			printf("=== PASSED ===\n");
		}else{
			printf("=== WRONG ===\n");
		}
		close(oup[0]);
	}
	return 0;
}
