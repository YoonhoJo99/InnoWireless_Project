#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    int port;

    // Ensure IP and port are provided
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }
    char *serverIP = argv[1];
    port = atoi(argv[2]);

    // Create UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up the server address struct
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(clientSocket);
        return 1;
    }

    // Send message to the server
    strcpy(buffer, "Hello from client");
    if (sendto(clientSocket, buffer, strlen(buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Send failed");
        close(clientSocket);
        return 1;
    }
    printf("Message sent to %s:%d\n", serverIP, port);

    // Receive server response
    int bytesReceived = recvfrom(clientSocket, buffer, BUFFER_SIZE, 0, NULL, NULL);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        printf("Received from server: %s\n", buffer);
    } else {
        printf("No response received from server.\n");
    }

    // Close the socket
    close(clientSocket);
    return 0;
}

