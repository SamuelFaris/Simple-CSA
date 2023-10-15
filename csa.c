
#include "csa.h"

//Clears screen
void cls(FILE *of) {
    fprintf(of, "\033[2J");
    fprintf(of, "\033[H");
}

int send_ack(int socket) {
    return (send(socket, "ACK", 3, 0) > 0);
}

int send_nak(int socket) {
    return (send(socket, "NAK", 3, 0) > 0);
}

int wait_for_ack_nak(int socket) {
    char buffer[BUFFER_SIZE];
    int n;

    //timeout period
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 5000; //5ms timeout

    //Set socket with timeout
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //Wait
    n = recv(socket, buffer, BUFFER_SIZE, 0);

    //Failed due to timeout or error
    if (n < 0) {
        perror("Error receiving ACK/NAK");
        return 0;
    }

    //ACK'ed
    if (strncmp("ACK", buffer, 3) == 0) {
        return 1;

    //NAK'ed
    } else if (strncmp("NAK", buffer, 3) == 0) {
        return 0;
    } else {
        return 0;
    }
}