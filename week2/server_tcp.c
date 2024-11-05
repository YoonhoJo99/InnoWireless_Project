#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 소켓 바인딩 및 리스닝
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 3);

    printf("Server listening on port %d...\n", PORT);

    // 클라이언트 연결 수락
    client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    printf("Client connected!\n");

    // 데이터 수신 (Uplink)
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    printf("Received from client: %s\n", buffer);

    // 데이터 송신 (Downlink)
    char *message = "Hello from server!";
    send(client_socket, message, strlen(message), 0);
    printf("Sent to client: %s\n", message);

    // 종료
    close(client_socket);
    close(server_fd);
    return 0;
}

