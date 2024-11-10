#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void receiveMessageFromClientA(int socketd, struct sockaddr_in* clientA_address) {
    char buffer[512];
    socklen_t address_len = sizeof(*clientA_address);

    ssize_t length = recvfrom(socketd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)clientA_address, &address_len);
    if (length < 0) {
        perror("Receive failed");
        return;
    }

    buffer[length] = '\0'; // Null-terminate the received message
    printf("Client A가 보낸 '%s'를 받았습니다.\n", buffer);

    // Send confirmation back to Client A
    const char* confirmation_message = "Message received successfully by Client B!";
    if (sendto(socketd, confirmation_message, strlen(confirmation_message), 0, (struct sockaddr*)clientA_address, sizeof(*clientA_address)) == -1) {
        perror("Send confirmation failed");
    } else {
        printf("Confirmation sent back to Client A: '%s'\n", confirmation_message);
    }
}

int main() {
    char clientA_ip[100];
    unsigned short clientA_port;
    int socketd;
    struct sockaddr_in clientB_address;

    // Retrieve Client B's public IP and port (from previous STUN logic)
    
    printf("Client A의 IP와 Port를 입력하시오: ");
    scanf("%s %hu", clientA_ip, &clientA_port);  // Input Client A's IP and Port

    // Create a socket to receive messages
    socketd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Setup the local address for Client B
    bzero(&clientB_address, sizeof(clientB_address));
    clientB_address.sin_family = AF_INET;
    clientB_address.sin_port = htons(clientA_port);  // Use Client A's port to receive messages
    if (inet_pton(AF_INET, clientA_ip, &clientB_address.sin_addr) <= 0) {
        perror("Invalid address");
        close(socketd);
        return -1;
    }

    // Bind the socket to listen for messages from Client A
    if (bind(socketd, (struct sockaddr*)&clientB_address, sizeof(clientB_address)) < 0) {
        perror("Bind failed");
        close(socketd);
        return -1;
    }

    // Receive the message from Client A
    receiveMessageFromClientA(socketd, &clientB_address);

    // Close the socket
    close(socketd);
    return 0;
}

