#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define STUN_SERVER "stun.l.google.com"
#define STUN_PORT 19302
#define LOCAL_PORT 10000

int main() {
    int sockfd;
    struct sockaddr_in local_addr, stun_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(stun_addr);

    // 소켓 생성
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 로컬 주소 바인딩
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(LOCAL_PORT);

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // STUN 서버 주소 설정
    memset(&stun_addr, 0, sizeof(stun_addr));
    stun_addr.sin_family = AF_INET;
    stun_addr.sin_port = htons(STUN_PORT);
    inet_pton(AF_INET, STUN_SERVER, &stun_addr.sin_addr);

    // STUN 요청 전송
    strcpy(buffer, "STUN Request");
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&stun_addr, sizeof(stun_addr));

    // STUN 응답 수신
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&stun_addr, &addr_len) < 0) {
        perror("STUN response failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 수신한 공인 IP와 포트 출력
    printf("Public IP: %s, Public Port: %d\n", inet_ntoa(stun_addr.sin_addr), ntohs(stun_addr.sin_port));

    // 윈도우 PC의 공인 IP와 포트를 복사하여 메모장이나 다른 클라이언트와 공유

    close(sockfd);
    return 0;
}

