#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>

// Constants for STUN and NAT traversal
#define GOOGLE_STUN_SERVER "stun.l.google.com"
#define GOOGLE_STUN_PORT 19302
#define BUFFER_SIZE 1024
#define PUNCH_INTERVAL_MS 500
#define PUNCH_TIMEOUT_SEC 60
#define KEEPALIVE_INTERVAL 5

// Constants for speed test
#define SPEED_TEST_PORT_OFFSET 1
#define CHUNK_SIZE (64 * 1024)
#define TEST_DURATION 10
#define SPEED_TEST_COMMAND "SPEED_TEST"
#define TCP_BUFFER_SIZE (256 * 1024)

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
    double duration;
    double bytes;
    double speed_mbps;
} SpeedTestResult;

// Function declarations
void generate_transaction_id(uint8_t *transaction_id);
int create_stun_binding_request(uint8_t *buffer);
int parse_stun_response(uint8_t *response, int response_len, PublicEndpoint *endpoint);
int get_public_endpoint(int sock, PublicEndpoint *endpoint);
int perform_nat_punching(int sock, struct sockaddr_in *peer_addr);
void send_keepalive(int sock, struct sockaddr_in *peer_addr);
int create_tcp_socket(int port, int is_server);
void* run_speed_test_server(void* arg);
SpeedTestResult run_speed_test_client(const char* server_ip, int port);

// Implementation of functions
void generate_transaction_id(uint8_t *transaction_id) {
    for (int i = 0; i < 12; i++) {
        transaction_id[i] = rand() % 256;
    }
}

int create_stun_binding_request(uint8_t *buffer) {
    struct stun_header *header = (struct stun_header *)buffer;
    header->type = htons(STUN_BINDING_REQUEST);
    header->length = htons(0);
    header->magic_cookie = htonl(STUN_MAGIC_COOKIE);
    generate_transaction_id(header->transaction_id);
    return sizeof(struct stun_header);
}

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

void send_keepalive(int sock, struct sockaddr_in *peer_addr) {
    const char *keepalive_msg = "KEEPALIVE";
    sendto(sock, keepalive_msg, strlen(keepalive_msg), 0,
           (struct sockaddr*)peer_addr, sizeof(*peer_addr));
}

int perform_nat_punching(int sock, struct sockaddr_in *peer_addr) {
    const char *punch_messages[] = {
        "PUNCH_REQ",
        "PUNCH_ACK"
    };

    time_t start_time = time(NULL);
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    char buffer[BUFFER_SIZE];
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    printf("Starting NAT hole punching...\n");
    printf("Target IP: %s, Port: %d\n", inet_ntoa(peer_addr->sin_addr), ntohs(peer_addr->sin_port));

    int punch_count = 0;

    while (time(NULL) - start_time < PUNCH_TIMEOUT_SEC) {
        sendto(sock, punch_messages[0], strlen(punch_messages[0]), 0,
               (struct sockaddr*)peer_addr, sizeof(*peer_addr));
        printf("Attempt %d: Trying port %d\n", ++punch_count, ntohs(peer_addr->sin_port));

        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&recv_addr, &addr_len);

        if (recv_len > 0) {
            printf("Received from %s:%d - %s\n", 
                   inet_ntoa(recv_addr.sin_addr),
                   ntohs(recv_addr.sin_port),
                   buffer);

            if (strcmp(buffer, punch_messages[0]) == 0) {
                sendto(sock, punch_messages[1], strlen(punch_messages[1]), 0,
                      (struct sockaddr*)&recv_addr, sizeof(recv_addr));
            }
            else if (strcmp(buffer, punch_messages[1]) == 0) {
                printf("NAT hole punching successful!\n");
                return 0;
            }
        }

        usleep(PUNCH_INTERVAL_MS * 1000);
    }

    printf("NAT hole punching failed after %d attempts\n", punch_count);
    printf("Total time elapsed: %ld seconds\n", time(NULL) - start_time);
    return -1;
}

int create_tcp_socket(int port, int is_server) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("TCP socket creation failed");
        return -1;
    }

    int buffer_size = TCP_BUFFER_SIZE;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    if (is_server) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("TCP bind failed");
            close(sock);
            return -1;
        }

        if (listen(sock, 1) < 0) {
            perror("Listen failed");
            close(sock);
            return -1;
        }
    }

    return sock;
}

void* run_speed_test_server(void* arg) {
    int port = *(int*)arg;
    int server_sock = create_tcp_socket(port, 1);
    if (server_sock < 0) return NULL;

    printf("Speed test server listening on port %d\n", port);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_sock < 0) {
        perror("Accept failed");
        close(server_sock);
        return NULL;
    }

    char buffer[CHUNK_SIZE];
    size_t total_received = 0;
    time_t start_time = time(NULL);
    
    while (1) {
        ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        total_received += received;
        
        if (time(NULL) - start_time >= TEST_DURATION) break;
    }

    double duration = difftime(time(NULL), start_time);
    double speed_mbps = (total_received * 8.0) / (duration * 1000000.0);
    
    SpeedTestResult result = {
        .duration = duration,
        .bytes = total_received,
        .speed_mbps = speed_mbps
    };
    send(client_sock, &result, sizeof(result), 0);

    printf("Speed test completed. Received %.2f MB at %.2f Mbps\n",
           total_received / (1024.0 * 1024.0), speed_mbps);

    close(client_sock);
    close(server_sock);
    return NULL;
}

SpeedTestResult run_speed_test_client(const char* server_ip, int port) {
    SpeedTestResult result = {0};
    int sock = create_tcp_socket(0, 0);
    if (sock < 0) return result;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return result;
    }

    char buffer[CHUNK_SIZE];
    memset(buffer, 'X', sizeof(buffer));
    
    size_t total_sent = 0;
    time_t start_time = time(NULL);
    
    while (difftime(time(NULL), start_time) < TEST_DURATION) {
        ssize_t sent = send(sock, buffer, sizeof(buffer), 0);
        if (sent < 0) break;
        total_sent += sent;
    }

    recv(sock, &result, sizeof(result), 0);
    close(sock);
    return result;
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

    printf("\nP2P connection established. Commands:\n");
    printf("1. Start chat\n");
    printf("2. Run speed test\n");
    printf("Choice: ");

    int choice;
    scanf("%d", &choice);
    getchar(); // 버퍼 비우기

    if (choice == 2) {
        // 상대방에게 속도 테스트 시작을 알림
        sendto(sock, SPEED_TEST_COMMAND, strlen(SPEED_TEST_COMMAND), 0,
               (struct sockaddr*)&peer_addr, sizeof(peer_addr));

        // TCP 포트는 UDP 포트 + 1 사용
        int tcp_port = ntohs(local_addr.sin_port) + SPEED_TEST_PORT_OFFSET;
        
        // 서버 스레드 시작
        pthread_t server_thread;
        pthread_create(&server_thread, NULL, run_speed_test_server, &tcp_port);

        // 잠시 대기 후 클라이언트 시작
        sleep(1);
        
        // 클라이언트로 속도 테스트 실행
        printf("Starting speed test...\n");
        SpeedTestResult result = run_speed_test_client(peer_ip, 
                                                     ntohs(peer_addr.sin_port) + SPEED_TEST_PORT_OFFSET);
        
        printf("\nSpeed Test Results:\n");
        printf("Duration: %.2f seconds\n", result.duration);
        printf("Total Data: %.2f MB\n", result.bytes / (1024.0 * 1024.0));
        printf("Speed: %.2f Mbps\n", result.speed_mbps);

        pthread_join(server_thread, NULL);
        return 0;
    }

    // 채팅 모드
    printf("P2P connection established. Starting chat...\n");

    fd_set readfds;
    struct timeval tv;
    time_t last_activity = time(NULL);
    
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

        time_t current_time = time(NULL);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            fgets(buffer, BUFFER_SIZE, stdin);

            if (sendto(sock, buffer, strlen(buffer), 0,
                      (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                perror("Send failed");
            }
            last_activity = current_time;
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

            // 속도 테스트 명령 확인
            if (strcmp(buffer, SPEED_TEST_COMMAND) == 0) {
                printf("Received speed test request\n");
                int tcp_port = ntohs(local_addr.sin_port) + SPEED_TEST_PORT_OFFSET;
                pthread_t server_thread;
                pthread_create(&server_thread, NULL, run_speed_test_server, &tcp_port);
                pthread_detach(server_thread);
                continue;
            }

            if (strcmp(buffer, "KEEPALIVE") != 0) {
                printf("Peer: %s", buffer);
                last_activity = current_time;
            }
        }

        // Keepalive 전송
        if (current_time - last_activity >= KEEPALIVE_INTERVAL) {
            send_keepalive(sock, &peer_addr);
            last_activity = current_time;
        }
    }

    close(sock);
    return 0;
}

