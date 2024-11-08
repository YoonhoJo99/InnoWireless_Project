#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;
    int port;

    // 포트 번호 입력 확인
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    port = atoi(argv[1]);

    // 서버 소켓 생성
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 서버 주소 설정
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // 소켓 바인딩
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSocket);
        return 1;
    }

    // 연결 대기
    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        close(serverSocket);
        return 1;
    }
    printf("Listening on port %d...\n", port);

    // 클라이언트 연결 수락
    addr_size = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addr_size);
    if (clientSocket < 0) {
        perror("Accept failed");
        close(serverSocket);
        return 1;
    }
    printf("Client connected.\n");

    // 데이터 송수신 루프
    while (1) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            printf("Client disconnected or error occurred.\n");
            break;
        }
        buffer[bytesReceived] = '\0';
        printf("Received from client: %s\n", buffer);

        // 클라이언트에게 응답
        send(clientSocket, buffer, bytesReceived, 0);
    }

    // 소켓 종료
    close(clientSocket);
    close(serverSocket);
    return 0;
}

