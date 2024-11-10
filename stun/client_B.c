// client_B.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include "STUNExternalIP.h"
#include "client_common.h"

int main() {
    printf("[LOG] Client B 시작...\n");
    
    // STUN 서버에서 공인 IP, 포트 획득
    struct STUNServer server = {"stun.l.google.com", 19302};
    char public_ip[100];
    unsigned short public_port;
    
    if (getPublicIPAddressAndPort(server, public_ip, &public_port) != 0) {
        printf("[ERROR] STUN 서버 응답 실패\n");
        return 1;
    }
    printf("Client B Public IP: %s, Port: %d\n", public_ip, public_port);
    
    // 소켓 생성 및 바인딩
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[ERROR] 소켓 생성 실패\n");
        return 1;
    }
    
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(public_port);
    
    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        printf("[ERROR] 바인드 실패\n");
        return 1;
    }
    
    // Client A 정보 입력
    char client_a_ip[100];
    int client_a_port;
    printf("Client A의 IP와 Port를 입력하시오.\n>>>");
    scanf("%s %d", client_a_ip, &client_a_port);
    
    // 홀펀칭 정보 설정
    struct hole_punch_info info;
    strcpy(info.remote_ip, client_a_ip);
    info.remote_port = client_a_port;
    info.local_sock = sock;
    
    memset(&info.remote_addr, 0, sizeof(info.remote_addr));
    info.remote_addr.sin_family = AF_INET;
    info.remote_addr.sin_addr.s_addr = inet_addr(client_a_ip);
    info.remote_addr.sin_port = htons(client_a_port);
    
    // 홀펀칭 수행
    if (create_hole_punch(&info) != 0) {
        printf("[ERROR] 홀펀칭 실패\n");
        close(sock);
        return 1;
    }
    
    // 메시지 수신 및 NAT 바인딩 유지
    while (1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&from_addr, &from_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            
            if (strcmp(buffer, "keep-alive") != 0) {
                printf("ClientA가 보낸 '%s'를 받았습니다.\n", buffer);
                
                // 응답 전송
                sendto(sock, "메시지를 받았습니다", 20, 0,
                       (struct sockaddr*)&info.remote_addr, sizeof(info.remote_addr));
            }
        }
    }
    
    close(sock);
    return 0;
}
