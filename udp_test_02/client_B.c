#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include "STUNExternalIP.h"

#define BUFFER_SIZE 1024

int main() {
    printf("[LOG] Client B 시작...\n");
    
    struct STUNServer server = {"stun.l.google.com", 19302};
    char public_ip[100];
    unsigned short public_port;
    
    printf("[LOG] STUN 서버에 요청 중...\n");
    int ret = getPublicIPAddressAndPort(server, public_ip, &public_port);
    if (ret != 0) {
        printf("[ERROR] STUN 서버 응답 실패. Error: %d\n", ret);
        return 1;
    }
    printf("[LOG] STUN 서버 응답 성공!\n");
    
    printf("Client B Public IP: %s, Port: %d\n", public_ip, public_port);
    
    printf("[LOG] UDP 소켓 생성 중...\n");
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[ERROR] 소켓 생성 실패: %s\n", strerror(errno));
        return 1;
    }
    printf("[LOG] 소켓 생성 성공. socket_fd: %d\n", sock);
    
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(public_port);
    
    printf("[LOG] 소켓 바인딩 중...\n");
    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        printf("[ERROR] 바인드 실패: %s\n", strerror(errno));
        return 1;
    }
    printf("[LOG] 소켓 바인딩 성공\n");
    
    char client_a_ip[100];
    int client_a_port;
    printf("Client A의 IP와 Port를 입력하시오.\n>>>");
    scanf("%s %d", client_a_ip, &client_a_port);
    printf("[LOG] Client A 정보 입력 완료 - IP: %s, Port: %d\n", client_a_ip, client_a_port);
    
    struct sockaddr_in client_a_addr;
    memset(&client_a_addr, 0, sizeof(client_a_addr));
    client_a_addr.sin_family = AF_INET;
    client_a_addr.sin_addr.s_addr = inet_addr(client_a_ip);
    client_a_addr.sin_port = htons(client_a_port);
    
    printf("[LOG] 메시지 수신 대기 중...\n");
    
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_a_addr);
    
    while (1) {
        printf("[LOG] 메시지를 기다리는 중...\n");
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&client_a_addr, &addr_len);
        
        if (recv_len < 0) {
            printf("[ERROR] 메시지 수신 실패: %s\n", strerror(errno));
            continue;
        }
        
        buffer[recv_len] = '\0';
        printf("[LOG] 메시지 수신 성공! (%d bytes): '%s'\n", recv_len, buffer);
        
        if (strcmp(buffer, "keep-alive") != 0) {
            printf("ClientA가 보낸 '%s'를 받았습니다.\n", buffer);
        }
        
        // Client A에게 응답
        printf("[LOG] Client A에게 응답 전송 중...\n");
        ssize_t sent_bytes = sendto(sock, "received", 8, 0,
                                  (struct sockaddr*)&client_a_addr, sizeof(client_a_addr));
        
        if (sent_bytes < 0) {
            printf("[ERROR] 응답 전송 실패: %s\n", strerror(errno));
        } else {
            printf("[LOG] 응답 전송 성공! (%zd bytes)\n", sent_bytes);
        }
    }
    
    close(sock);
    return 0;
}

