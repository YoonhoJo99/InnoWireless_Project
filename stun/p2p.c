#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

void error_handling(const char *message) {
    perror(message);
    exit(1);
}

int main() {
    int sock;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t peer_addr_size;
    char buffer[BUFFER_SIZE];
    int str_len;
    char peer_ip[INET_ADDRSTRLEN];
    int peer_port;

    // UDP 소켓 생성
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 자신의 주소 설정
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(0);  // OS가 임의의 포트를 할당하도록 설정

    if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
        error_handling("bind() error");

    // 피어 정보 입력
    printf("Enter peer IP: ");
    scanf("%s", peer_ip);
    printf("Enter peer port: ");
    scanf("%d", &peer_port);

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    peer_addr.sin_port = htons(peer_port);

    peer_addr_size = sizeof(peer_addr);

    // 핑-퐁 메시지 전송 및 수신
    while (1) {
        printf("Enter message (q to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);

        if (!strcmp(buffer, "q\n")) break;

        // 메시지 전송
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&peer_addr, peer_addr_size);

        // 메시지 수신
        str_len = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&peer_addr, &peer_addr_size);
        if (str_len < 0) break;
        
        buffer[str_len] = 0;
        printf("Message from peer: %s", buffer);
    }

    close(sock);
    return 0;
}

