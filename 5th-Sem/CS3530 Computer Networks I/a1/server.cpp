#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <iostream>
#include <array>

#define BUFSIZE 2048

static const int MAXPENDING = 10; // Maximum outstanding connection requests
struct client{
	int index;
	int clntSock;
	struct sockaddr_in clntAddr;
	socklen_t clntAddrLen;
};

pthread_t thread[1024];
char name[1024][100];
struct client Client[1024];
int clientCount = 0, nameCount = 0;

// Receive Data from client
void recieveData(char * buffer, struct client* cInfo){
	memset(buffer, 0, BUFSIZE);
	ssize_t recvLen = recv(cInfo->clntSock, buffer, BUFSIZE - 1, 0);
	if (recvLen < 0) {
		perror("recv() failed");
		exit(-1);
	}
	buffer[recvLen] = '\0';
}

// Send Data to client
void sendData(char * buffer, int clntSock){
	ssize_t sentLen = send(clntSock, buffer, BUFSIZE - 1, 0);
	if (sentLen < 0) {
		perror("send() failed");
		exit(-1);
	}
}

// Execute a command on server and return the output to the client
void exec(const char* cmd,  struct client* cInfo) {
	printf("Entering Command mode\n");
	char buffer[BUFSIZE];
	char result[BUFSIZE];
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
	int l = 0;
    while (fgets(buffer, 128, pipe.get()) != nullptr) {
		if(l >= 1024)break;
		l += snprintf(result + l, 127, "%s", buffer);
    }
	sendData(result, cInfo -> clntSock);
	memset(result, 0, BUFSIZE);
    memset(buffer, 0, BUFSIZE);
	printf("Exiting Command mode\n");
}

// Host and manage message passing
void * host(void * c){
	struct client* cInfo = (struct client*) c;
	printf("Client %d connected.\n",cInfo -> index + 1);
	while(1){
		char buffer[BUFSIZE];
		recieveData(buffer, cInfo);

		if(strcmp(buffer,"User_List") == 0){
			int l = 0;
			for(int i = 0 ; i < clientCount ; i ++)
				if(i != cInfo -> index)
					l += snprintf(buffer + l, BUFSIZE - 1, "Client %d: %s\n", i + 1, name[i]);
			sendData(buffer, cInfo -> clntSock);
		}
		else if(strcmp(buffer,"Send") == 0){
			recieveData(buffer, cInfo);
			int temp = atoi(buffer) - 1;
			recieveData(buffer, cInfo);
			printf("Message received from %s\n",name[cInfo -> index]);
            char tmp[BUFSIZE];
            int l = 0;
            l = snprintf(tmp + l, BUFSIZE - 1, "Msg from Client %d: %s -> ", cInfo -> index + 1, name[cInfo -> index]);
            l = snprintf(tmp + l, BUFSIZE - 1, "%s\n", buffer);
			printf("Message sent to %s\n",name[temp]);
			sendData(tmp, Client[temp].clntSock);
            memset(tmp, 0, BUFSIZE);
            memset(buffer, 0, BUFSIZE);
		}
		else if(strcmp(buffer,"User@name") == 0){
			recieveData(buffer, cInfo);
			strcpy(name[cInfo -> index],buffer);
			printf("Username Registered: %s\n",name[cInfo -> index]);
	        fflush(stdout);
		}
		else if(strcmp(buffer,"Command") == 0){
			recieveData(buffer, cInfo);
			exec(buffer, cInfo);
			memset(buffer, 0, BUFSIZE);
		}

	}
	return NULL;
}

int main(){
	
    char servIP[100] = "127.0.0.1"; 
    in_port_t servPort = 8002;
	printf("Enter: <Server Address>\n");
    scanf("%s", servIP);
    printf("Enter: <Port>\n");
    scanf("%hu", &servPort);

	// create socket for incoming connections
	int servSock;
	if ((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket() failed");
		exit(-1);
	}

	// Set local parameters
	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(servPort);


	// Bind to the local address
	if (bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
		perror("bind() failed");
		exit(-1);
	}

	// Listen to the client
	if (listen(servSock, MAXPENDING) < 0) {
		perror("listen() failed");
		exit(-1);
	}
    printf("Server hosted on IP: %s\nListening on Port: %hu ..........\n", servIP, servPort);

    // Accept Clients
	while(1){

		Client[clientCount].clntSock = accept(servSock, (struct sockaddr*) &Client[clientCount].clntAddr, &Client[clientCount].clntAddrLen);
		if(Client[clientCount].clntSock < 0){
			perror("accept() failed");
			exit(-1);
		}
		Client[clientCount].index = clientCount;
		pthread_create(&thread[clientCount], NULL, host, (void *) &Client[clientCount]);
		clientCount ++;
	}

    // Wait for all thread to join
	for(int i = 0 ; i < clientCount ; i ++)
		pthread_join(thread[i],NULL);
}