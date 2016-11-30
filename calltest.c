#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/poll.h>

#define PORT 8000

typedef struct{
	int inp[2], oup[2];
	pid_t pid;
}Worker;

int main(int argc, char const *argv[]) {
	// Set true value
	int yes = 1;
	// Create server socket
	int serverSock;
	if((serverSock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) < 0){
		perror("Server socket() error: ");
		exit(-1);
	}
	// Set socket option
	if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0){
		perror("Server setsockopt() error: ");
		close(serverSock);
		exit(-1);
	}
	// Construct sockaddr_in structure
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton("0.0.0.0", &serverAddr.sin_addr);
	// Bind server
	if(bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
		perror("bind() error: ");
		close(serverSock);
		exit(-1);
	}
	// Listen
	if(listen(serverSock, 10) < 0){
		perror("listen() error: ");
		close(serverSock);
		exit(-1);
	}
	// Add stdin and listen fds
	struct pollfd fds[102];
	Worker workers[100];
	memset(workers, 0, sizeof(workers));
	memset(fds, 0, sizeof(fds));
	fds[0].fd = STDIN_FILENO; // stdin
	fds[0].events = POLLIN;
	fds[1].fd = serverSock; // server
	fds[1].events = POLLIN;
	for (int i = 2; i < 102; i++) {
		fds[i].fd = -1;
	}
	// Poll event loop
	int prc;
	int nsock = 0;
	int clientSock = 0;
	while((prc = poll(fds, 102, -1)) > 0){
		if(fds[0].revents){ // stdio
			if(fds[0].revents != POLLIN){
				break;
			}else{
				char cmd[200];
				if(fgets(cmd, 200, stdin)){
					if(!strcmp(cmd, "stop\n")){
						break;
					}else{
						printf("Type \"stop\" to close!\n");
					}
				}
			}
		}else if(fds[1].revents){ // server
			if(fds[1].revents != POLLIN){
				perror("server error");
				break;
			}else{
				do {
					struct sockaddr_in clientAddr;
					socklen_t clientSize = sizeof(clientAddr);
					memset(&clientAddr, 0, sizeof(clientAddr));
					clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &clientSize);
					if(clientSock < 0){
						if(errno != EWOULDBLOCK){
							perror("accept() error");
							prc = 0;
						}
						break;
					}else{
						if(nsock < 100){ // add to poll list
							int sockid;
							for(sockid = 2; sockid < 102; ++sockid){
								if(fds[sockid].fd < 0){
									break;
								}
							}
							fds[sockid].fd = clientSock;
							fds[sockid].events = POLLIN;
							nsock++;
						}else{ // server full
							write(clientSock, "HTTP/1.1 503 Service Unavailable\r\n\r\n\r\n", 38);
							close(clientSock);
						}
					}
				} while(clientSock != -1);
			}
		}else{ // client message
			for (int i = 2; i < 102; i++) {
				if(fds[i].fd > 0){
					if(fds[i].revents){
						if(fds[i].revents != POLLIN){
							perror("Socket error");
							printf("Error socket: %d\n", fds[i].fd);
							prc = 0;
							break;
						}else{
							char buf[1024];
							do{
								memset(buf, '\0', sizeof(buf));
								int crc = recv(fds[i].fd, buf, sizeof(buf), MSG_DONTWAIT);
								if(crc < 0){
									if (errno != EWOULDBLOCK){
										perror("recv() failed");
										printf("Error socket: %d\n", fds[i].fd);
										close(fds[i].fd);
										fds[i].fd = -1;
						            }
						            break;
								}else if(crc == 0){
									close(fds[i].fd);
									fds[i].fd = -1;
									break;
								}else{
									printf("%s", buf);
									if((buf[crc-4] == '\r')&&(buf[crc-3] == '\n')&&(buf[crc-2] == '\r')&&(buf[crc-1] == '\n')){
										// Read result
										memset(buf, '\0', 1024);
										strcpy(buf, "HTTP/1.1 200 OK\r\n\r\n\r\n");
										if(write(fds[i].fd, buf, strlen(buf)) < 0){
											perror("responce write() error");
											printf("Error socket: %d\n", fds[i].fd);
										}
										close(fds[i].fd);
										fds[i].fd = -1;
										break;
									}
								}
							}while(1);
						}
					}
				}
			}
		}
	}
	if(prc < 0){
		perror("poll() error");
		for (int i = 0; i < 102; i++) {
			if(fds[i].fd > 0){
				close(fds[i].fd);
			}
		}
		return -1;
	}
	for (int i = 0; i < 102; i++) {
		if(fds[i].fd > 0){
			close(fds[i].fd);
		}
	}
	return 0;
}
