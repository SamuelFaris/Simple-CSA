
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <pthread.h>


#define MSG_PORT 1000           //Port for messaging
#define FILE_PORT 1001          //Port for file transfer
#define SERVER_IP "127.0.0.1"   //Localhost
#define BUFFER_SIZE 1024

//Helper functions
void cls(FILE *of);
int send_ack(int socket);
int send_nak(int socket);
int wait_for_ack_nak(int socket);