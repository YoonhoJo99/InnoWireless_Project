#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "STUNExternalIP.h"  // STUN 기능을 제공하는 헤더 파일

#define BUF_SIZE 512
#define STUN_SERVER_IP "stun.l.google.com"  // 사용할 STUN 서버 (예: 구글 STUN 서버)
#define STUN_SERVER_PORT 19302

int main() {
    struct STUNServer stun_server = {STUN_SERVER_IP, STUN_SERVER_PORT};
    char public_ip[20];
    unsigned short public_port;

    // STUN 서버로부터 공용 IP와 포트 가져오기
    if (getPublicIPAddressAndPort(stun_server, public_ip, &public_port) != 0) {
        fprintf(stderr, "Failed to get public IP and port\n");
        return 1;
    }
    
    printf("Your Public IP: %s, Port: %u\n", public_ip, public_port);

    // 상대방의 IP와 포트 수동 입력
    char peer_ip[20];
    unsigned short peer_port;
    printf("Enter peer's IP and port:\n>>> ");
    scanf("%s %hu", peer_ip, &peer_port);

    // UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 자신의 주소 설정 (랜덤 포트 사용)
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(public_port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
        perror("Binding failed");
        close(sock);
        return 1;
    }

    // 상대방 주소 설정
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    if (inet_aton(peer_ip, &peer_addr.sin_addr) == 0) {
        fprintf(stderr, "Invalid peer IP address\n");
        close(sock);
        return 1;
    }

    // 전송할 메시지 입력
    char message[BUF_SIZE];
    printf("Enter a message to send:\n>>> ");
    scanf(" %[^\n]", message); // 공백 포함 입력 받기

    // 메시지 전송
    if (sendto(sock, message, strlen(message), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Failed to send message");
        close(sock);
        return 1;
    }

    // 메시지 수신 대기
    char buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(peer_addr);
    if (recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&peer_addr, &addr_len) < 0) {
        perror("Failed to receive message");
        close(sock);
        return 1;
    }

    printf("Received message from peer: '%s'\n", buffer);

    close(sock);
    return 0;
}

