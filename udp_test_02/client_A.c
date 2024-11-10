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
    printf("[LOG] Client A 시작...\n");
    
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
    
    printf("Client A Public IP: %s, Port: %d\n", public_ip, public_port);
    
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
    
    char client_b_ip[100];
    int client_b_port;
    printf("Client B의 IP와 Port를 입력하시오.\n>>>");
    scanf("%s %d", client_b_ip, &client_b_port);
    printf("[LOG] Client B 정보 입력 완료 - IP: %s, Port: %d\n", client_b_ip, client_b_port);
    
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
    
    printf("[LOG] 메시지 전송 시도 중... (메시지: %s)\n", message);
    ssize_t sent_bytes = sendto(sock, message, strlen(message), 0,
           (struct sockaddr*)&client_b_addr, sizeof(client_b_addr));
    
    if (sent_bytes < 0) {
        printf("[ERROR] 메시지 전송 실패: %s\n", strerror(errno));
    } else {
        printf("[LOG] 메시지 전송 성공! (%zd bytes)\n", sent_bytes);
    }
    
    printf("[LOG] 응답 대기 중...\n");
    
    // 응답 대기 추가 (재시도 로직 포함)
    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    int retry_count = 0;
    int max_retries = 5;
    
    while (retry_count < max_retries) {
        ssize_t recv_len = recvfrom(sock, recv_buffer, BUFFER_SIZE, 0,
                                    (struct sockaddr*)&from_addr, &from_len);
        
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[ERROR] 응답 수신 실패: 타임아웃. 재시도 중... (%d/%d)\n", retry_count + 1, max_retries);
                retry_count++;
                continue;
            } else {
                printf("[ERROR] 응답 수신 실패: %s\n", strerror(errno));
                break;
            }
        }
        
        recv_buffer[recv_len] = '\0';
        printf("[LOG] 응답 수신 성공! (%zd bytes): %s\n", recv_len, recv_buffer);
        break;
    }
    
    if (retry_count == max_retries) {
        printf("[ERROR] 응답 수신 실패: 재시도 횟수 초과\n");
    }
    
    printf("[LOG] NAT 바인딩 유지를 위한 루프 시작...\n");
    while (1) {
        printf("[LOG] Keep-alive 메시지 전송 중...\n");
        sleep(10); // Keep-alive 메시지 간격 10초로 변경
        sent_bytes = sendto(sock, "keep-alive", 10, 0,
                          (struct sockaddr*)&client_b_addr, sizeof(client_b_addr));
        if (sent_bytes < 0) {
            printf("[ERROR] Keep-alive 전송 실패: %s\n", strerror(errno));
        } else {
            printf("[LOG] Keep-alive 전송 성공 (%zd bytes)\n", sent_bytes);
        }
    }
    
    close(sock);
    return 0;
}

