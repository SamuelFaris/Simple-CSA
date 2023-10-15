/*
SERVER CODE FOR SIMPLE CSA PROGRAM SUPPORTING MESSAGING AND FILE TRANSFER

    -Saves transmitted files to server_files/

*/
#include "csa.h"

int file_server_socket;
int file_client_socket;
char file_buf[BUFFER_SIZE];

int msg_server_socket;
int msg_client_socket;
char msg_buf[BUFFER_SIZE];

pthread_t msg_tid;
pthread_t file_tid;

int quit_flag = 0;
int server_exit = 0;

void sigint_handler(int sig) {
    printf("\nShutting down server\n");

    //Close sockets
    close(msg_client_socket);
    close(file_client_socket);
    close(msg_server_socket);
    close(file_server_socket);
    printf("Cleanup complete, exiting\n");
    exit(0);
}

//Function for handling messages in foreground. Returns 1 if client sent QUIT, 0 otherwise
void *handle_messages(void *socket){
    int client_socket = *(int *)socket;
    char ack[128] = "";

    //Make socket nonblocking
    fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);
    
    while(!server_exit) {
        bzero(msg_buf, BUFFER_SIZE);
        int n = recv(client_socket, msg_buf, BUFFER_SIZE, MSG_DONTWAIT);
        if (n < 0) {
            //No data case
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            //Error case
            } else {
                perror("Error in receiving message from server");
                break;
            }

        //Client quitting
        } else if (strncmp("QUIT", msg_buf, 4) == 0) {
            printf("Client requested to quit. Closing connection.\n");
            quit_flag = 1;
            pthread_exit(0);

        //Client sent message
        } else if (n > 0) {
            printf("Received message: %s\n", msg_buf);
            strcpy(ack, "ACK MSG");
            send(client_socket, ack, strlen(ack), 0);
        }
    }
    pthread_exit(0);
}

void *handle_file_transfer(void* socket) {
    int client_socket = *(int *)socket;

    while(1) {
        bzero(file_buf, BUFFER_SIZE);
        int n = recv(client_socket, file_buf, BUFFER_SIZE, 0);
        if (n < 0) {
            perror("Error in receiving message from client");

        //Client sending file
        } else if (strncmp("FILE ", file_buf, 5) == 0) {
            //Parse metadata
            strtok(file_buf, " ");                  //'FILE'
            char *filename = strtok(NULL, " ");     //filename
            char *sizestr  = strtok(NULL, " ");     //filesize as string
            size_t filesize = atol(sizestr);        //filesize as long

            //Create and open new file for incoming data
            char filepath[256];
            char *filename_cpy = (char *)malloc(strlen(filename));
            memcpy(filename_cpy, filename, strlen(filename));
            strcpy(filepath, "server_files/");
            strcat(filepath, filename);

        
            FILE *ofp = fopen(filepath, "w");
            if (ofp == NULL) {
                perror("Error in opening file");
                exit(1);
            }

            size_t received = 0;
            int iters = 0;

            //ACK the initial RTS            
            send_ack(client_socket);

            while (1) {
                size_t remaining = filesize - received;
                size_t chunk_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;

                //Done when received = filesize
                if (chunk_size <= 0) {
                    printf("File '%s' received from client\n", filename_cpy);
                    break;
                }

                //Else pull next [chunk_size] bytes from buffer
                n = recv(client_socket, file_buf, chunk_size, 0);
                if (n == -1) {
                    printf("File '%s' transfer unsuccessful\n", filename_cpy);
                    break;
                }

                //Read packet into ofp
                fprintf(ofp, "%s", file_buf);
                received += n;

                //ACK next packet
                send_ack(client_socket);
            }

            fclose(ofp);
            free(filename_cpy);

        //Unrecognizable input
        } else {
            continue;
        }
    }
}

int main() {

    signal(SIGINT, sigint_handler);
    if(access("server_files/", F_OK) == 0) {
        system("exec rm -r server_files/*");        //clean out folder
    }
    cls(stdout);

    struct sockaddr_in msg_server_addr, file_server_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    FILE *ofp;
    int n;
    pthread_t tid;

    //Init sockets
    msg_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (msg_server_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    file_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (file_server_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    //Server addr settings
    msg_server_addr.sin_family = AF_INET;
    msg_server_addr.sin_port = MSG_PORT;
    msg_server_addr.sin_addr.s_addr = INADDR_ANY;

    file_server_addr.sin_family = AF_INET;
    file_server_addr.sin_port = FILE_PORT;
    file_server_addr.sin_addr.s_addr = INADDR_ANY;

    //Bind server sockets
    if (bind(msg_server_socket, (struct sockaddr*)&msg_server_addr, sizeof(msg_server_addr)) < 0) {
        perror("Error in binding");
        exit(1);
    }

    if (bind(file_server_socket, (struct sockaddr*)&file_server_addr, sizeof(file_server_addr)) < 0) {
        perror("Error in binding");
        exit(1);
    }

    while (!server_exit) {

        //Listen on sockets
        cls(stdout);
        if (listen(msg_server_socket, 1) == 0) {
            printf("Listening on messaging port...\n");
        } else {
            printf("Listening error\n");
        }

        if (listen(file_server_socket, 1) == 0) {
            printf("Listening on file transfer port ...\n");
        } else {
            printf("Listening error\n");
        }

        //Accept requests on these ports
        msg_client_socket = accept(msg_server_socket, NULL, NULL);
        file_client_socket = accept(file_server_socket, NULL, NULL);

        cls(stdout);
        printf("Established connection on messaging port\n");
        printf("Established connection on file transfer port\n");

        while (!quit_flag) {    

            //Create separate threads for each port
            pthread_create(&msg_tid, NULL, handle_messages, &msg_client_socket);
            pthread_create(&file_tid, NULL, handle_file_transfer, &file_client_socket);

            //Wait for msg child to finish (user entered 'QUIT')
            pthread_join(msg_tid, NULL);
            pthread_cancel(file_tid);

            //Close sockets
            close(msg_client_socket);
            close(file_client_socket);
        }
        quit_flag = 0;
    }

    close(msg_server_socket);
    close(file_server_socket);
    return 0;
}