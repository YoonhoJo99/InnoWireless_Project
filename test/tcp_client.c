#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    int port;

    // IP와 포트 번호 입력 확인
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }
    char *serverIP = argv[1];
    port = atoi(argv[2]);

    // 소켓 생성
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 서버 주소 설정
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(clientSocket);
        return 1;
    }

    // 서버에 연결 시도
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        close(clientSocket);
        return 1;
    }

    printf("Connected to the server: %s:%d\n", serverIP, port);

    // 서버로부터 데이터 수신 (테스트용)
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';  // 문자열 끝에 NULL 추가
        printf("Received from server: %s\n", buffer);
    } else {
        printf("No message received from the server.\n");
    }

    // 소켓 종료
    close(clientSocket);
    return 0;
}

