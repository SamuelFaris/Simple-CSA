/*
CLIENT CODE FOR SIMPLE CSA PROGRAM SUPPORTING MESSAGING AND FILE TRANSFER

    -For file transfer, enter 'FILE [filename]'
        -file must be placed in client_files/
        -file will be copied to server_files/

    -To exit gracefully, enter 'QUIT'

    -Any other entry will be sent as a message

*/

#include "csa.h"

int msg_client_socket;
int file_client_socket;
char msg_buf[BUFFER_SIZE];
char file_buf[BUFFER_SIZE];

int polling_user = 0;
int exit_flag = 0;

struct send_file_args {
    char filepath[256];
    char filename[64];
    int client_socket;
};

void sigint_handler(int sig) {
    send(msg_client_socket, "QUIT", 4, 0);
    exit_flag = 1;
    fflush(stdin);
}

//Function for sending files to server
void *send_file(void *arg) {
    struct send_file_args args = *(struct send_file_args *)arg;
    char filepath[128], filename[64];
    strcpy(filepath, args.filepath);
    strcpy(filename, args.filename);
    int client_socket = args.client_socket;

    int n;
    bzero(file_buf, BUFFER_SIZE);

    FILE *ifp = fopen(filepath, "r");
    if (ifp == NULL) {
        perror("Error opening file");
        return NULL;
    }

    //Get file size
    int prev = ftell(ifp);                  //save SOF
    fseek(ifp, 0L, SEEK_END);               //go to EOF
    size_t filesize = (size_t)ftell(ifp);   //get offset of EOF
    fseek(ifp, prev, SEEK_SET);             //go back to SOF

    //Construct and send metadata payload (name and size of file being sent)
    char metadata[BUFFER_SIZE];
    snprintf(metadata, sizeof(metadata), "FILE %s %zu", filename, filesize);
    send(client_socket, metadata, BUFFER_SIZE, 0);

    //Wait for ack
    if(!wait_for_ack_nak(client_socket)) {
        free(arg);
        fclose(ifp);
        printf("Error: could not send first packet, ending transmission\n");
        return NULL;
    }

    int failed = 0;
    size_t sent = 0;
    char file_out[BUFFER_SIZE];
    while (sent < filesize) {
        //Calculate bytes to read + send
        size_t remaining = filesize - sent;
        size_t chunk_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;

        fread(file_out, 1, chunk_size, ifp);
        n = send(client_socket, file_out, chunk_size, 0);
        if (n < 0 || !wait_for_ack_nak(client_socket)) {
            printf("Error: could not transmit packet\n");
            failed = 1;
            break;
        }
        sent += n;
    }

    //Wait for user to enter prompt and output status of transmit
    char output[256];
    if (!failed) {
        sprintf(output, "File '%s' received by server\n", filename);
    } else 
        sprintf(output, "File '%s' not received by server\n", filename);
    while(polling_user) {};
    printf("%s", output);

    free(arg);
    fclose(ifp);

    return NULL;
}

int main() {

    signal(SIGINT, sigint_handler);
    cls(stdout);
    
    struct sockaddr_in msg_server_addr;
    struct sockaddr_in file_server_addr;
    char buffer[BUFFER_SIZE];
    FILE *ifp;
    pthread_t tid;
    int n;
    
    //Init sockets
    msg_client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (msg_client_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    file_client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (file_client_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    //Server addr settings
    msg_server_addr.sin_family = AF_INET;
    msg_server_addr.sin_port = MSG_PORT;
    msg_server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    file_server_addr.sin_family = AF_INET;
    file_server_addr.sin_port = FILE_PORT;
    file_server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    //Attempt connection to localhost
    if (connect(msg_client_socket, (struct sockaddr*)&msg_server_addr, sizeof(msg_server_addr)) != 0) {
        perror("Error in connection");
        exit(1);
    }
    printf("Established messaging connection with server.\n");

    if (connect(file_client_socket, (struct sockaddr*)&file_server_addr, sizeof(file_server_addr)) != 0) {
        perror("Error in connection");
        exit(1);
    }
    printf("Established file transfer connection with server.\n");

    //Loop for sending user files/messages
    while (!exit_flag) {
        //Get user command
        polling_user = 1;
        printf("Enter message: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';
        polling_user = 0;

        //File transfer
        if (strncmp("FILE ", buffer, 5) == 0) {
            memcpy(file_buf, buffer, BUFFER_SIZE);

            //Extract filepath info
            struct send_file_args *thread_args = (struct send_file_args *)malloc(sizeof(struct send_file_args));
            strncpy(thread_args->filename, buffer + 5, 64);
            strcpy(thread_args->filepath, "client_files/");
            strcat(thread_args->filepath, thread_args->filename);
            thread_args->client_socket = file_client_socket;

            //Create a thread to send file in background
            pthread_t ftid;
            pthread_create(&ftid, NULL, send_file, (void *)thread_args);

        //Stopping connection
        } else if (strncmp("QUIT", buffer, 4) == 0) {
            send(msg_client_socket, "QUIT", 4, 0);
            cls(stdout);
            break;

        //Sending messages
        } else {
            memcpy(msg_buf, buffer, BUFFER_SIZE);
            send(msg_client_socket, msg_buf, BUFFER_SIZE, 0);
        }
    }

    close(msg_client_socket);
    close(file_client_socket);
    return 0;
}