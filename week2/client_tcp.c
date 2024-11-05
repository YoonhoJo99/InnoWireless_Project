#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1" // 서버 IP 주소 (테스트 환경에 따라 변경)

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[1024] = "Hello from client!";

    // 소켓 생성
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 서버에 연결
    connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // 데이터 송신 (Uplink)
    send(client_fd, buffer, strlen(buffer), 0);
    printf("Sent to server: %s\n", buffer);

    // 데이터 수신 (Downlink)
    recv(client_fd, buffer, 1024, 0);
    printf("Received from server: %s\n", buffer);

    // 종료
    close(client_fd);
    return 0;
}

