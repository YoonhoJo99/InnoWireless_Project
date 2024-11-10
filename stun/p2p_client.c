#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define MAX_RETRY 5

typedef struct {
    int sock;
    struct sockaddr_in peer_addr;
    int is_connected;
} PeerConnection;

PeerConnection *connection;

void handle_signal(int sig) {
    if (connection && connection->sock) {
        close(connection->sock);
    }
    exit(0);
}

void *receive_messages(void *arg) {
    PeerConnection *conn = (PeerConnection *)arg;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recvfrom(conn->sock, buffer, BUFFER_SIZE, 0, 
                              (struct sockaddr *)&sender_addr, &addr_len);
        
        if (recv_len < 0) {
            perror("recvfrom failed");
            continue;
        }

        // 홀펀칭 메시지 처리
        if (strcmp(buffer, "punch") == 0) {
            printf("홀펀칭 요청 수신 from %s:%d\n", 
                   inet_ntoa(sender_addr.sin_addr), 
                   ntohs(sender_addr.sin_port));
            
            conn->peer_addr = sender_addr;
            const char *response = "punch-response";
            sendto(conn->sock, response, strlen(response), 0,
                   (struct sockaddr *)&sender_addr, sizeof(sender_addr));
            conn->is_connected = 1;
        }
        else if (strcmp(buffer, "punch-response") == 0) {
            printf("홀펀칭 성공 - peer: %s:%d\n",
                   inet_ntoa(sender_addr.sin_addr),
                   ntohs(sender_addr.sin_port));
            conn->is_connected = 1;
        }
        else {
            printf("\n상대방: %s\n", buffer);
        }
    }
    return NULL;
}

int try_connection(PeerConnection *conn) {
    const char *punch_msg = "punch";
    int retry_count = 0;

    while (!conn->is_connected && retry_count < MAX_RETRY) {
        printf("홀펀칭 시도 %d/%d...\n", retry_count + 1, MAX_RETRY);
        
        sendto(conn->sock, punch_msg, strlen(punch_msg), 0,
               (struct sockaddr *)&conn->peer_addr, sizeof(conn->peer_addr));
        
        sleep(1);
        retry_count++;
    }

    return conn->is_connected;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("사용법: %s <local_port>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);

    // 소켓 생성 및 초기화
    connection = malloc(sizeof(PeerConnection));
    connection->sock = socket(AF_INET, SOCK_DGRAM, 0);
    connection->is_connected = 0;

    if (connection->sock < 0) {
        perror("소켓 생성 실패");
        return 1;
    }

    // 로컬 주소 바인딩
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(atoi(argv[1]));

    if (bind(connection->sock, (struct sockaddr *)&local_addr, 
             sizeof(local_addr)) < 0) {
        perror("바인드 실패");
        close(connection->sock);
        return 1;
    }

    printf("로컬 포트 %s에서 시작됨\n", argv[1]);

    // 수신 스레드 시작
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, connection) != 0) {
        perror("스레드 생성 실패");
        close(connection->sock);
        return 1;
    }

    // 메인 루프
    char input_buffer[BUFFER_SIZE];
    while (1) {
        if (!connection->is_connected) {
            char peer_ip[16];
            int peer_port;

            printf("상대방 IP 입력: ");
            scanf("%s", peer_ip);
            printf("상대방 포트 입력: ");
            scanf("%d", &peer_port);
            getchar(); // 버퍼 비우기

            // 피어 주소 설정
            memset(&connection->peer_addr, 0, sizeof(connection->peer_addr));
            connection->peer_addr.sin_family = AF_INET;
            connection->peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
            connection->peer_addr.sin_port = htons(peer_port);

            if (!try_connection(connection)) {
                printf("연결 실패\n");
                continue;
            }
            printf("연결 성공!\n");
        }

        printf("메시지 입력 (종료는 'quit'): ");
        fgets(input_buffer, BUFFER_SIZE, stdin);
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        if (strcmp(input_buffer, "quit") == 0) {
            break;
        }

        if (sendto(connection->sock, input_buffer, strlen(input_buffer), 0,
                  (struct sockaddr *)&connection->peer_addr, 
                  sizeof(connection->peer_addr)) < 0) {
            perror("메시지 전송 실패");
        }
    }

    // 정리
    close(connection->sock);
    free(connection);
    return 0;
}
