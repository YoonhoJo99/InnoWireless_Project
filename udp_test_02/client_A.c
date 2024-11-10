#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void sendMessageToClientB(const char* clientB_ip, unsigned short clientB_port, const char* message) {
    int socketd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketd < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in clientB_address;
    bzero(&clientB_address, sizeof(clientB_address));
    clientB_address.sin_family = AF_INET;
    clientB_address.sin_port = htons(clientB_port);
    if (inet_pton(AF_INET, clientB_ip, &clientB_address.sin_addr) <= 0) {
        perror("Invalid address");
        close(socketd);
        return;
    }

    if (sendto(socketd, message, strlen(message), 0, (struct sockaddr*)&clientB_address, sizeof(clientB_address)) == -1) {
        perror("Send failed");
        close(socketd);
        return;
    }

    printf("Message sent to Client B: %s\n", message);
}

void receiveConfirmationFromClientB(int socketd) {
    char buffer[512];
    struct sockaddr_in clientB_address;
    socklen_t address_len = sizeof(clientB_address);

    ssize_t length = recvfrom(socketd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientB_address, &address_len);
    if (length < 0) {
        perror("Receive failed");
        return;
    }

    buffer[length] = '\0'; // Null-terminate the received message
    printf("Client B received the message: '%s'\n", buffer);
}

int main() {
    char clientB_ip[100];
    unsigned short clientB_port;
    char message[512];

    // Retrieve Client A's public IP and port (from previous STUN logic)

    printf("Client B's IP and Port를 입력하시오: ");
    scanf("%s %hu", clientB_ip, &clientB_port);  // Input Client B's IP and Port

    printf("ClientB에게 보낼 TestMessage를 입력하세요: ");
    scanf(" %[^\n]%*c", message);  // Input the message to send to Client B

    // Send the message to Client B
    sendMessageToClientB(clientB_ip, clientB_port, message);

    // Now wait for Client B's response to confirm message receipt
    int socketd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in clientA_address;
    bzero(&clientA_address, sizeof(clientA_address));
    clientA_address.sin_family = AF_INET;
    clientA_address.sin_port = htons(0);  // Random available port
    clientA_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketd, (struct sockaddr*)&clientA_address, sizeof(clientA_address)) < 0) {
        perror("Bind failed");
        close(socketd);
        return -1;
    }

    // Receive confirmation from Client B
    receiveConfirmationFromClientB(socketd);

    printf("NAT Hole Punching 성공!!\n");

    close(socketd);
    return 0;
}

