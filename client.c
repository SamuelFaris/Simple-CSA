/*
CLIENT CODE FOR SIMPLE CSA PROGRAM SUPPORTING MESSAGING AND FILE TRANSFER

    -For file transfer, enter 'FILE [filename]'
        -file must be placed in client_files/
        -file will be copied to server_files/

    -To exit gracefully, enter 'QUIT'

    -Any other entry will be sent as a message

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/signal.h>

#define PORT 1234               //Port number
#define SERVER_IP "127.0.0.1"   //Localhost
#define BUFFER_SIZE 1024
int exit_flag = 0;              //Global flag for SIGINT

//Sets exit flag when SIGINT
void sigint_handler(int sig) {
    exit_flag = 1;
}

void cls(void) {
    printf("\033[2J");
    printf("\033[H");
}

int main() {

    cls();

    //Register SIGINT handler
    signal(SIGINT, sigint_handler);

    //Create client socket
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    FILE *ifp;
    int n;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = PORT;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Error in connection");
        exit(1);
    }
    printf("Established connection with server.\n");

    while (!exit_flag) {
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove the newline character

        //File transfer
        if (strncmp("FILE ", buffer, 5) == 0) {
            
            //Open file on client side
            char filepath[1024];
            char filename[256];
            strcpy(filepath, "client_files/");
            strcpy(filename, buffer + 5);
            strcat(filepath, filename);

            ifp = fopen(filepath, "r");
            if (ifp == NULL) {
                perror("Error in opening file");
                continue;
            }

            //Get file size
            int prev = ftell(ifp);          //save SOF
            fseek(ifp, 0L, SEEK_END);       //go to EOF
            size_t filesize = ftell(ifp);   //get offset of EOF
            fseek(ifp, prev, SEEK_SET);     //go back to SOF

            //Construct and send metadata payload (name and size of file being sent)
            char metadata[BUFFER_SIZE];
            snprintf(metadata, sizeof(metadata), "FILE %s %zu", filename, filesize);
            send(client_socket, metadata, BUFFER_SIZE, 0);

            //Send file
            while ((n = fread(buffer, 1, sizeof(buffer), ifp)) > 0) {
                send(client_socket, buffer, n, 0);
            }
            fclose(ifp);

            //Wait for ACK from server
            bzero(buffer, BUFFER_SIZE);
            n = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (n <= 0) {
                perror("No ACK from server");
                break;
            }

            if (strncmp("ACK", buffer, 3) == 0) {
                printf("File '%s' sent and received by server\n", filename);
            } else {
                printf("ERROR: File '%s' sent but not received by server\n", filename);
            }


        //Stopping connection
        } else if (strncmp("QUIT", buffer, 4) == 0) {
            send(client_socket, "QUIT", 4, 0);
            cls();
            break;

        //Sending messages
        } else {
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }

    close(client_socket);
    return 0;
}