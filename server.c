#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/signal.h>

#define PORT 1234               //Port number
#define BUFFER_SIZE 1024

void cls(void) {
    printf("\033[2J");
    printf("\033[H");
}

int main() {

    cls();

    //Create server socket and requester socket
    int server_socket, new_socket;
    struct sockaddr_in server_addr, new_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    FILE *ofp;
    int n;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = PORT;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //Bind server socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in binding");
        exit(1);
    }

    //Listen for connection requests and handle communications with client
    while (1) {

        //Listen on socket, queue 1 connection request max
        if (listen(server_socket, 1) == 0) {
            printf("Listening...\n");
        } else {
            printf("Error in listening\n");
        }

        //Wait for connection
        addr_size = sizeof(new_addr);
        new_socket = accept(server_socket, (struct sockaddr*)&new_addr, &addr_size);
        cls();
        printf("Established connection with client\n");

        while (1) {
            bzero(buffer, BUFFER_SIZE);
            n = recv(new_socket, buffer, BUFFER_SIZE, 0);
            if (n <= 0) {
                perror("Error in receiving");
                break;
            }

            //Handling files
            if (strncmp("FILE ", buffer, 5) == 0) {
                
                //Parse metadata
                strtok(buffer, " ");                    //'FILE'
                char *filename = strtok(NULL, " ");     //filename
                char *sizestr  = strtok(NULL, " ");     //filesize as string
                size_t filesize = atol(sizestr);        //filesize as long

                //Create and open new file for incoming data
                char filepath[1024];
                strcpy(filepath, "server_files/");
                strcat(filepath, filename);

                ofp = fopen(filepath, "w");
                if (ofp == NULL) {
                    perror("Error in opening file");
                    exit(1);
                }

                //Read into file from buffer while data is present
                size_t received = 0;
                while (received < filesize) {
                    n = recv(new_socket, buffer, BUFFER_SIZE, 0);
                    if (n <= 0) {
                        break;
                    }
                    fprintf(ofp, "%s", buffer);
                    received += n;
                }
                fclose(ofp);
                printf("File received from client and written to '%s'\n", filepath);

                //Send ACK to client
                send(new_socket, "ACK", 3, 0);

            //Handling messages
            } else if (strncmp("QUIT", buffer, 4) == 0) {
                printf("Client requested to quit. Closing connection.\n");
                break;
            } else {
                printf("Received message: %s\n", buffer);
            }
        }
        cls();
    }

    close(server_socket);
    return 0;
}