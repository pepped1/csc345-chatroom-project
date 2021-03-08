#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 10004

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	struct _USR* next;	// for linked list queue
	char userName[16];	// username goes in here
	char ipAddress[16];	// ip address string
	int roomNum;		// assigned room number
} USR;

USR *head = NULL;
USR *tail = NULL;
int roomCounter; //counts number of rooms
int roomCount[1001]; //array that keeps track of users in rooms

//prints each username and ip address that joined the server
//called when someone joins or leaves server
void print_list()
{
	USR *temp = head;
	while (temp != NULL){
		printf("%s (%s)\n", temp->userName, temp->ipAddress);
		temp = temp->next;
	}
	printf("\n");
}

//copies all information about a user into the node
void add_tail(int newclisockfd, char* user, char* ipAdd, int room)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		strncpy(head->userName, user, 16);
		strncpy(head->ipAddress, ipAdd, 16);
		head->roomNum = room;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		strncpy(tail->next->userName, user, 16);
		strncpy(tail->next->ipAddress, ipAdd, 16);
		tail->next->roomNum = room;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

//this function was added to deal with users leaving
void remove_node(int fromfd)
{
	USR *temp = head;
	if(head->clisockfd == fromfd){
		--roomCount[head->roomNum];
		head = head->next;
		free(temp);
	}
	else{
		while(temp->next->clisockfd != fromfd){
			temp = temp->next;
		}
		--roomCount[temp->next->roomNum];
		USR *temp2 = temp->next;
		temp->next = temp->next->next;
		free(temp2);
	}
}

void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	//finds node that is sending
	USR* temp = head;
	while (temp->clisockfd != fromfd){
		temp = temp->next;
	}

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd && cur->roomNum == temp->roomNum) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "[%s]:%s", temp->userName, message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	do{
		memset(buffer, 0, 256);
		// we send the message to everyone except the sender
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		else if (nrcv == 0) break;
		
		broadcast(clisockfd, buffer);

	} while(nrcv > 0);

	remove_node(clisockfd);
	close(clisockfd);
	print_list();
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	//setting up global variables
	roomCounter = 1;
	memset(roomCount, 0, 1001);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	//changed maximum number of connections from 5 to 50 (for fun)
	listen(sockfd, 50);

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");
		
		char roomBuffer[6]; //buffer used for sending and receiving data from clients
		memset(roomBuffer, 0, 6);
		int tempRoomHolder; //temp variable for holding the clients room number
		
		int rrcv = recv(newsockfd, roomBuffer, 6, 0);
		if (rrcv < 0) error("ERROR com recv() failed");

		if (strncmp(roomBuffer, "empt", strlen("empt")) == 0){
			//case if the user doesnt know what they want
			memset(roomBuffer, 0, 6);

			if (roomCounter == 1){
				//if no rooms were made yet
				snprintf(roomBuffer, 6, "%d", 1);
				int dsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (dsend < 0) error("ERROR writing data to socket");
				sleep(.2);
				goto emptyNew;
			} else {
				//if rooms exist
				snprintf(roomBuffer, 6, "%d", 2);
				int dsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (dsend < 0) error("ERROR writing data to socket");

				//sends data about number of users in each room
				for (int i=1; i<=roomCounter; ++i){
					sleep(.2);
					memset(roomBuffer, 0, 6);
					snprintf(roomBuffer, 6, "%d", roomCount[i]);
					int qsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
					if (qsend < 0) error("ERROR writing data to socket");
				}
				memset(roomBuffer, 0, 6);
				int brcv = recv(newsockfd, roomBuffer, 6, 0);
				if (brcv < 0) error("ERROR com recv() failed");
				if(strncmp(roomBuffer, "new", strlen("new")) == 0){
					//if user wants a new room
					goto emptyNew;
				}
				else{
					//if user wants to join available room
					goto emptyJoin;
				}

			}

		} else if (strncmp(roomBuffer, "new", strlen("new")) == 0){
			//case if new room wants to be made
		emptyNew:
			memset(roomBuffer, 0, 6);
			if(roomCounter < 1000){
				//if there isnt max number of rooms
				//and the max number of rooms is 999
				snprintf(roomBuffer, 6, "%d", roomCounter);
				int nsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (nsend < 0) error("ERROR writing room num to socket");
				tempRoomHolder = roomCounter;
				++roomCount[roomCounter];
				++roomCounter;
			} else {
				//no more rooms available
				snprintf(roomBuffer, 6, "%d", 0);
				int nsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (nsend < 0) error("ERROR writing room denial to socket");
				continue;
			}

		} else{
			//for server, else has to be join attempt
		emptyJoin:
			sleep(0);
			int tempRoom = atoi(roomBuffer);
			int dsend;
			memset(roomBuffer, 0, 6);
			if (tempRoom < roomCounter){
				//if the room has been created
				tempRoomHolder = tempRoom;
				++roomCount[tempRoom];
				snprintf(roomBuffer, 6, "%d", tempRoom);
				dsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (dsend < 0) error("ERROR writing data to socket");

			} else{
				//room was not created
				snprintf(roomBuffer, 6, "%d", 10000);
				dsend = send(newsockfd, roomBuffer, strlen(roomBuffer), 0);
				if (dsend < 0) error("ERROR writing data to socket");
				continue;
			}
		}

		//receives username
		char tempUser[16];
		memset(tempUser, 0, 16);
		int urcv = recv(newsockfd, tempUser, 16, 0);
		if (urcv < 0) error("ERROR: could not recv() username");

		add_tail(newsockfd, tempUser, inet_ntoa(cli_addr.sin_addr), tempRoomHolder); // add this new client to the client list
		print_list();

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

