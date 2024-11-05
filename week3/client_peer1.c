#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <peer2_public_ip> <peer2_public_port>\n", argv[0]);
        return -1;
    }

    const char *peer_ip = argv[1];
    int peer_port = atoi(argv[2]);

    // TCP 소켓 생성
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr);

    // 연결 시도
    if (connect(sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        return -1;
    }

    printf("Connected to Peer 2!\n");

    // 데이터 전송
    char *message = "Hello from Peer 1!";
    send(sockfd, message, strlen(message), 0);
    printf("Sent to Peer 2: %s\n", message);

    // 데이터 수신
    char buffer[BUFFER_SIZE];
    int recv_len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        printf("Received from Peer 2: %s\n", buffer);
    }

    // 소켓 닫기
    close(sockfd);
    return 0;
}
