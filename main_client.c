#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define PORT_NUM 10004

char userName[16]; //stores client's username
int roomNum = 0; //stores client's room number

void error(const char *msg)
{
	perror(msg);
	exit(-1);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[256];
	int n;

	//stuff for color
	int userNum = 0;
	int numUsers = 0;
	char tempUser[256];
	bool isInList = false;
	char userList[6][256];
	char colorList[6][10] = {"\033[0;31m", "\033[0;32m", "\033[0;33m", "\033[0;34m", "\033[0;35m", "\033[0;36m"};
	time_t t;
	srand((unsigned) time(&t));
	for (int i = 0; i < 6; i++) { // shuffle array for color randomization
	  int j = rand() % 6;
	  char temp[10];
	  strcpy(temp, colorList[j]);
	  strcpy(colorList[j], colorList[i]);
	  strcpy(colorList[i], temp);
        }

	do{
		memset(buffer, 0, 256);
		n = recv(sockfd, buffer, 256, 0);
		if (n < 0) error("ERROR recv() failed");

		strcpy(tempUser, buffer);                   // read buffer until ']'
		strtok(tempUser, "]");

		for (int i=0; i<6; i++) {
		  if (strcmp(userList[i], tempUser) == 0) { // check userList for that user
		    isInList = true;
		    userNum = i;
		  }
		  if (isInList) break;
		}
		if (!isInList) {                            // if not yet in list, add to list
		  strcpy(userList[numUsers], tempUser);
		  userNum = numUsers;
		  numUsers++;
		}		  
		isInList = false;
		
		printf("%s", colorList[userNum]);           // assign color
		printf("\n%s", buffer);                     // print buffer
		printf("\033[0m");                          // reset color
		printf("\n");
	} while(n > 0);

	return NULL;
}

void* thread_main_send(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[256];
	int n;

	printf("%s has entered room %d\n", userName, roomNum);

	while (1) {
		memset(buffer, 0, 256);
		fgets(buffer, 256, stdin);

		//removes the annoying newline or ends cycle
		if (strlen(buffer) == 1) buffer[0] = '\0';
		else buffer[strlen(buffer) - 1] == '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

		if (n == 0){
			printf("Disconnected from server\n");
			break; // we stop transmission when user type empty string
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please specify hostname");
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	int status = connect(sockfd, (struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");
	
	char coms[6]; //buffer used by client for sending and receiving from server
	memset(coms, 0, 6);

	if (argc < 3){
		//tells server its the empty case
		strncpy(coms, "empt", strlen("empt"));
		int rsend = send(sockfd, coms, strlen(coms), 0);
		if (rsend < 0) error("ERROR writing com to socket");
		memset(coms, 0, 6);

		int rrcv = recv(sockfd, coms, 6, 0);
		if (rrcv < 0) error("ERROR com recv() failed");

		//if server has no rooms, makes a new room
		if (atoi(coms) == 1){
			goto emptNew;
		}
		//else, prints number of people in a room
		else {
			printf("Server says the following options are available: \n");

			int rmCntr = 1;
			int tempNum, drcv;
			do{
				memset(coms, 0, 6);
				drcv = recv(sockfd, coms, 6, 0);
				if (drcv < 0) error("ERROR room data recv() failed");
				tempNum = atoi(coms);
				if (tempNum == 1){
					printf("\tRoom %d: 1 person\n", rmCntr);
				}
				else if (tempNum > 1){
					printf("\tRoom %d: %d people\n", rmCntr, tempNum);
				}

				++rmCntr;
			} while (tempNum != 0);
			printf("Choose the room number or type [new] to create a new room: ");
			char blankCheck[6];
			memset(blankCheck, 0, 6);
			fgets(blankCheck, 6, stdin);
			blankCheck[strlen(blankCheck) - 1] = '\0';
			
			if(strncmp(blankCheck, "new", strlen("new")) == 0){
				int bsend = send(sockfd, blankCheck, strlen(blankCheck), 0);
				if (bsend < 0) error("ERROR writing new ans to socket");
				goto emptNew;
			}
			else if (atoi(blankCheck) > 0){
				int bsend = send(sockfd, blankCheck, strlen(blankCheck), 0);
				if (bsend < 0) error("ERROR writing join ans to socket");
				goto emptJoin;
			}
			else {
				error("Invalid Entry");
			}
		}
	}
	else if (strcmp(argv[2], "new") == 0){
		//tells server a new room is wanted
		strncpy(coms, "new", strlen("new"));

		int rsend = send(sockfd, coms, strlen(coms), 0);
		if (rsend < 0) error("ERROR writing room to socket");
	emptNew:
		memset(coms, 0, 6);

		int rrcv = recv(sockfd, coms, 6, 0);
		if (rrcv < 0) error("ERROR com recv() failed");

		if(atoi(coms) == 0){
			//max number of rooms
			error("ERROR No new rooms could be made");
		} else{
			//properly connected to new room
			roomNum = atoi(coms);
			printf("Connected to %s with new room number %d\n", inet_ntoa(serv_addr.sin_addr), roomNum);

		}

	} else if (atoi(argv[2]) > 0 && atoi(argv[2]) < 1000){
		//join attempt
		strncpy(coms, argv[2], strlen(argv[2]));
		int rsend = send(sockfd, coms, strlen(coms), 0);
		if (rsend < 0) error("ERROR writing room to socket");
		memset(coms, 0, 6);

	emptJoin:
		sleep(0);
		int rrcv = recv(sockfd, coms, 6, 0);
		if (rrcv < 0) error("ERROR room recv() failed");

		if(atoi(coms) > 0 && atoi(coms) < 1000){
			//room is valid
			roomNum = atoi(coms);
			printf("Connected to %s with room number %d\n", inet_ntoa(serv_addr.sin_addr), roomNum);
		} else {
			//room was invalid
			error("ERROR Room does not exist");
		}

	} else{
		error("Invalid Input");
	}

	//sending username to server
	printf("What is your username? ");
	memset(userName, 0, 16);
	fgets(userName, 16, stdin);

	userName[strlen(userName) - 1] = '\0';

	int usend = send(sockfd, userName, strlen(userName), 0);
	if (usend < 0) error("ERROR writing username to socket");

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}

