#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>

#define GOOGLE_STUN_SERVER "stun.l.google.com"
#define GOOGLE_STUN_PORT 19302
#define BUFFER_SIZE 1024
#define PUNCH_INTERVAL_MS 500
#define PUNCH_TIMEOUT_SEC 60  // 타임아웃 시간 증가
#define KEEP_ALIVE_INTERVAL 15
#define PORT_RANGE 1000      // 포트 범위 크게 증가

// STUN Message Types
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN Attribute Types
#define MAPPED_ADDRESS 0x0001
#define XOR_MAPPED_ADDRESS 0x0020

#pragma pack(push, 1)
struct stun_header {
    uint16_t type;
    uint16_t length;
    uint32_t magic_cookie;
    uint8_t transaction_id[12];
};

struct stun_attr_header {
    uint16_t type;
    uint16_t length;
};

struct xor_mapped_address {
    uint8_t reserved;
    uint8_t family;
    uint16_t port;
    uint32_t address;
};
#pragma pack(pop)

typedef struct {
    char public_ip[INET_ADDRSTRLEN];
    uint16_t public_port;
} PublicEndpoint;

typedef struct {
    int sock;
    struct sockaddr_in peer_addr;
    int is_connected;
    pthread_mutex_t mutex;
    uint16_t last_working_port;  // 마지막으로 응답을 받은 포트 저장
} PunchingContext;

// 랜덤 트랜잭션 ID 생성
void generate_transaction_id(uint8_t *transaction_id) {
    for (int i = 0; i < 12; i++) {
        transaction_id[i] = rand() % 256;
    }
}

// STUN 바인딩 요청 메시지 생성
int create_stun_binding_request(uint8_t *buffer) {
    struct stun_header *header = (struct stun_header *)buffer;
    
    header->type = htons(STUN_BINDING_REQUEST);
    header->length = htons(0);
    header->magic_cookie = htonl(STUN_MAGIC_COOKIE);
    generate_transaction_id(header->transaction_id);
    
    return sizeof(struct stun_header);
}

// STUN 응답에서 XOR-MAPPED-ADDRESS 파싱
int parse_stun_response(uint8_t *response, int response_len, PublicEndpoint *endpoint) {
    struct stun_header *header = (struct stun_header *)response;
    uint8_t *ptr = response + sizeof(struct stun_header);
    uint16_t msg_type = ntohs(header->type);
    
    if (msg_type != STUN_BINDING_RESPONSE) {
        return -1;
    }

    while (ptr < response + response_len) {
        struct stun_attr_header *attr = (struct stun_attr_header *)ptr;
        uint16_t attr_type = ntohs(attr->type);
        uint16_t attr_length = ntohs(attr->length);
        
        if (attr_type == XOR_MAPPED_ADDRESS) {
            struct xor_mapped_address *addr = (struct xor_mapped_address *)(ptr + sizeof(struct stun_attr_header));
            uint32_t xor_addr = ntohl(addr->address) ^ STUN_MAGIC_COOKIE;
            uint16_t xor_port = ntohs(addr->port) ^ (STUN_MAGIC_COOKIE >> 16);
            
            struct in_addr ip_addr;
            ip_addr.s_addr = htonl(xor_addr);
            inet_ntop(AF_INET, &ip_addr, endpoint->public_ip, INET_ADDRSTRLEN);
            endpoint->public_port = xor_port;
            return 0;
        }
        
        ptr += sizeof(struct stun_attr_header) + attr_length;
        if (attr_length % 4 != 0) {
            ptr += 4 - (attr_length % 4);
        }
    }
    
    return -1;
}

// STUN 서버로부터 공인 IP와 포트 획득
int get_public_endpoint(int sock, PublicEndpoint *endpoint) {
    struct hostent *host = gethostbyname(GOOGLE_STUN_SERVER);
    if (!host) {
        perror("Failed to resolve STUN server");
        return -1;
    }

    struct sockaddr_in stun_addr;
    memset(&stun_addr, 0, sizeof(stun_addr));
    stun_addr.sin_family = AF_INET;
    memcpy(&stun_addr.sin_addr, host->h_addr_list[0], host->h_length);
    stun_addr.sin_port = htons(GOOGLE_STUN_PORT);

    uint8_t request[BUFFER_SIZE];
    int request_len = create_stun_binding_request(request);

    if (sendto(sock, request, request_len, 0, 
               (struct sockaddr*)&stun_addr, sizeof(stun_addr)) < 0) {
        perror("Failed to send STUN request");
        return -1;
    }

    uint8_t response[BUFFER_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    
    int recv_len = recvfrom(sock, response, BUFFER_SIZE, 0,
                           (struct sockaddr*)&recv_addr, &addr_len);
    if (recv_len < 0) {
        perror("Failed to receive STUN response");
        return -1;
    }

    return parse_stun_response(response, recv_len, endpoint);
}

// Keep-alive 메시지 전송
void send_keepalive(int sock, struct sockaddr_in *peer_addr) {
    const char *keepalive_msg = "KEEPALIVE";
    sendto(sock, keepalive_msg, strlen(keepalive_msg), 0,
           (struct sockaddr*)peer_addr, sizeof(*peer_addr));
}

// 연결 유지 스레드
void* connection_maintenance_thread(void *arg) {
    PunchingContext *ctx = (PunchingContext*)arg;
    time_t last_keepalive = time(NULL);

    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->is_connected) {
            time_t now = time(NULL);
            if (now - last_keepalive >= KEEP_ALIVE_INTERVAL) {
                send_keepalive(ctx->sock, &ctx->peer_addr);
                last_keepalive = now;
            }
        }
        pthread_mutex_unlock(&ctx->mutex);
        usleep(100000);
    }
    return NULL;
}

// NAT 홀펀칭 시도를 위한 단일 메시지 전송
void send_punch_attempt(int sock, const char* message, struct sockaddr_in *addr) {
    sendto(sock, message, strlen(message), 0,
           (struct sockaddr*)addr, sizeof(*addr));
}

// NAT 홀펀칭 수행 (개선된 버전)
int perform_nat_punching(int sock, struct sockaddr_in *peer_addr) {
    PunchingContext ctx = {
        .sock = sock,
        .peer_addr = *peer_addr,
        .is_connected = 0,
        .last_working_port = 0
    };
    pthread_mutex_init(&ctx.mutex, NULL);

    pthread_t maintenance_thread;
    pthread_create(&maintenance_thread, NULL, connection_maintenance_thread, &ctx);

    const char *punch_messages[] = {
        "PUNCH_REQ",
        "PUNCH_ACK"
    };

    time_t start_time = time(NULL);
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    char buffer[BUFFER_SIZE];
    
    // 논블로킹 모드 설정
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // 초기 포트 범위 설정
    uint16_t base_port = ntohs(peer_addr->sin_port);
    uint16_t port_min = (base_port > PORT_RANGE) ? (base_port - PORT_RANGE) : 1024;
    uint16_t port_max = base_port + PORT_RANGE;

    printf("Starting enhanced NAT hole punching...\n");
    printf("Target IP: %s\n", inet_ntoa(peer_addr->sin_addr));
    printf("Initial port range: %d - %d\n", port_min, port_max);

    int received_response = 0;
    int punch_count = 0;
    struct sockaddr_in working_addr = *peer_addr;

    while (time(NULL) - start_time < PUNCH_TIMEOUT_SEC) {
        // 기본 포트로 시도
        send_punch_attempt(sock, punch_messages[0], peer_addr);
        printf("Attempt %d: Trying base port %d\n", ++punch_count, ntohs(peer_addr->sin_port));

        // 마지막으로 작동한 포트로 시도
        if (ctx.last_working_port != 0) {
            working_addr.sin_port = htons(ctx.last_working_port);
            send_punch_attempt(sock, punch_messages[0], &working_addr);
            printf("Trying last working port: %d\n", ctx.last_working_port);
        }

        // 포트 범위 순회
        for (uint16_t port = port_min; port <= port_max; port += 10) {  // 10포트 간격으로 시도
            struct sockaddr_in alt_addr = *peer_addr;
            alt_addr.sin_port = htons(port);
            
            send_punch_attempt(sock, punch_messages[0], &alt_addr);
            printf("Trying port: %d\n", port);

            // 응답 확인 (각 전송 후 짧은 대기)
            for (int i = 0; i < 5; i++) {
                memset(buffer, 0, BUFFER_SIZE);
                int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr*)&recv_addr, &addr_len);

                if (recv_len > 0) {
                    ctx.last_working_port = ntohs(recv_addr.sin_port);
                    printf("Received from %s:%d - %s\n", 
                           inet_ntoa(recv_addr.sin_addr),
                           ctx.last_working_port,
                           buffer);

                    if (strcmp(buffer, punch_messages[0]) == 0) {
                        // 응답 전송
                        send_punch_attempt(sock, punch_messages[1], &recv_addr);
                        received_response = 1;
                        peer_addr->sin_port = recv_addr.sin_port;  // 작동하는 포트로 업데이트
                    }
                    else if (strcmp(buffer, punch_messages[1]) == 0) {
                        printf("NAT hole punching successful!\n");
                        printf("Final working port: %d\n", ctx.last_working_port);
                        ctx.is_connected = 1;
                        return 0;
                    }
                }
                usleep(10000);  // 10ms 대기
            }
        }

        if (received_response) {
            printf("Response received, focusing on port %d\n", ctx.last_working_port);
        }

        usleep(PUNCH_INTERVAL_MS * 1000);
    }

    printf("NAT hole punching failed after %d attempts\n", punch_count);
    printf("Total time elapsed: %ld seconds\n", time(NULL) - start_time);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <local_port>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    int local_port = atoi(argv[1]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    PublicEndpoint public_endpoint;
    if (get_public_endpoint(sock, &public_endpoint) < 0) {
        printf("Failed to get public endpoint\n");
        return 1;
    }

    printf("Public IP: %s\n", public_endpoint.public_ip);
    printf("Public Port: %d\n", public_endpoint.public_port);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    
    printf("Enter peer's public IP and port (format: IP:PORT): ");
    char peer_info[64];
    fgets(peer_info, sizeof(peer_info), stdin);
    
    char peer_ip[INET_ADDRSTRLEN];
    int peer_port;
    sscanf(peer_info, "%[^:]:%d", peer_ip, &peer_port);

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    peer_addr.sin_port = htons(peer_port);

    if (perform_nat_punching(sock, &peer_addr) < 0) {
        printf("Failed to establish P2P connection\n");
        close(sock);
        return 1;
    }

    printf("P2P connection established. Starting chat...\n");

    fd_set readfds;
    struct timeval tv;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(sock + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            fgets(buffer, BUFFER_SIZE, stdin);

            if (sendto(sock, buffer, strlen(buffer), 0,
                      (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                perror("Send failed");
            }
        }

        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                  (struct sockaddr*)&peer_addr, &peer_addr_len);

            if (recv_len < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("Receive failed");
                }
                continue;
            }

            if (strcmp(buffer, "KEEPALIVE") != 0) {
                printf("Peer: %s", buffer);
            }
        }
    }

    close(sock);
    return 0;
}
