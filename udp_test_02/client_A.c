#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "STUNExternalIP.h"

#define BUFFER_SIZE 1024

int main() {
    struct STUNServer server = {"stun.l.google.com", 19302};
    char public_ip[100];
    unsigned short public_port;
    
    // 공인 IP와 포트 얻기
    int ret = getPublicIPAddressAndPort(server, public_ip, &public_port);
    if (ret != 0) {
        printf("Failed to get public IP and port. Error: %d\n", ret);
        return 1;
    }
    
    printf("Client A Public IP: %s, Port: %d\n", public_ip, public_port);
    
    // UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // 로컬 주소 바인딩
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(public_port);
    
    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    // Client B의 정보 입력 받기
    char client_b_ip[100];
    int client_b_port;
    printf("Client B의 IP와 Port를 입력하시오.\n>>>");
    scanf("%s %d", client_b_ip, &client_b_port);
    
    struct sockaddr_in client_b_addr;
    memset(&client_b_addr, 0, sizeof(client_b_addr));
    client_b_addr.sin_family = AF_INET;
    client_b_addr.sin_addr.s_addr = inet_addr(client_b_ip);
    client_b_addr.sin_port = htons(client_b_port);
    
    char message[BUFFER_SIZE];
    printf("ClientB에게 보낼 TestMessage를 입력하세요.\n>>>");
    getchar(); // 버퍼 비우기
    fgets(message, BUFFER_SIZE, stdin);
    message[strlen(message)-1] = '\0'; // 개행문자 제거
    
    // Client B에게 메시지 전송
    sendto(sock, message, strlen(message), 0,
           (struct sockaddr*)&client_b_addr, sizeof(client_b_addr));
    
    // NAT 바인딩 유지를 위한 주기적인 메시지 전송
    while (1) {
        // 30초마다 keep-alive 메시지 전송
        sleep(30);
        sendto(sock, "keep-alive", 10, 0,
               (struct sockaddr*)&client_b_addr, sizeof(client_b_addr));
    }
    
    close(sock);
    return 0;
}
