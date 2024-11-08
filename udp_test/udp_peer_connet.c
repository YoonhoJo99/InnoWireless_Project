#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TARGET_IP "윈도우_PC의_공인_IP"
#define TARGET_PORT 윈도우_PC의_공인_포트
#define LOCAL_PORT 10001

int main() {
    int sockfd;
    struct sockaddr_in local_addr, target_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(target_addr);

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

    // 타겟 주소 설정 (윈도우 PC의 공인 IP와 포트)
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(TARGET_PORT);
    inet_pton(AF_INET, TARGET_IP, &target_addr.sin_addr);

    // 연결 테스트를 위해 패킷 전송
    strcpy(buffer, "Hello from Mac PC");
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

    // 응답 수신
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&target_addr, &addr_len) < 0) {
        perror("Failed to receive response");
    } else {
        printf("Received response: %s\n", buffer);
    }

    close(sockfd);
    return 0;
}

