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
    
    printf("Client B Public IP: %s, Port: %d\n", public_ip, public_port);
    
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
    
    // Client A의 정보 입력 받기
    char client_a_ip[100];
    int client_a_port;
    printf("Client A의 IP와 Port를 입력하시오.\n>>>");
    scanf("%s %d", client_a_ip, &client_a_port);
    
    struct sockaddr_in client_a_addr;
    memset(&client_a_addr, 0, sizeof(client_a_addr));
    client_a_addr.sin_family = AF_INET;
    client_a_addr.sin_addr.s_addr = inet_addr(client_a_ip);
    client_a_addr.sin_port = htons(client_a_port);
    
    // Client A로부터 메시지 수신
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_a_addr);
    
    while (1) {
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&client_a_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            if (strcmp(buffer, "keep-alive") != 0) {
                printf("ClientA가 보낸 '%s'를 받았습니다.\n", buffer);
            }
            // Client A에게 응답 (NAT 바인딩 유지를 위해)
            sendto(sock, "received", 8, 0,
                   (struct sockaddr*)&client_a_addr, sizeof(client_a_addr));
        }
    }
    
    close(sock);
    return 0;
}
