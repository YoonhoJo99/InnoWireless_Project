#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <listen_port>\n", argv[0]);
        return -1;
    }

    int listen_port = atoi(argv[1]);

    // TCP 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port);

    // 소켓 바인딩 및 리스닝
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    listen(server_fd, 1);
    printf("Listening on port %d...\n", listen_port);

    // 클라이언트 연결 수락
    socklen_t addr_len = sizeof(client_addr);
    int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("Accept failed");
        close(server_fd);
        return -1;
    }

    printf("Connected to Peer 1!\n");

    // 데이터 수신
    char buffer[BUFFER_SIZE];
    int recv_len = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        printf("Received from Peer 1: %s\n", buffer);
    }

    // 응답 전송
    char *message = "Hello from Peer 2!";
    send(client_socket, message, strlen(message), 0);
    printf("Sent to Peer 1: %s\n", message);

    // 소켓 닫기
    close(client_socket);
    close(server_fd);
    return 0;
}

