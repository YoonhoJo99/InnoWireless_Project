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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>

#define GOOGLE_STUN_SERVER "stun.l.google.com"
#define GOOGLE_STUN_PORT 19302
#define BUFFER_SIZE 1024
#define PUNCH_INTERVAL_MS 500
#define PUNCH_TIMEOUT_SEC 60
#define KEEPALIVE_INTERVAL 5

// STUN Message Types
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN Attribute Types
#define MAPPED_ADDRESS 0x0001
#define XOR_MAPPED_ADDRESS 0x0020

// Iperf style definitions
#define IPERF_PACKET_SIZE 8192  // Default packet size
#define TEST_DURATION 10        // Default test duration (seconds)
#define REPORT_INTERVAL 1       // Status report interval (seconds)

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

struct iperf_packet {
    uint32_t packet_id;         // Packet sequence number
    uint32_t tv_sec;           // Timestamp seconds
    uint32_t tv_usec;          // Timestamp microseconds
    char data[IPERF_PACKET_SIZE - 12];  // Actual data
};
#pragma pack(pop)

typedef struct {
    char public_ip[INET_ADDRSTRLEN];
    uint16_t public_port;
} PublicEndpoint;

// Test statistics structure
struct test_stats {
    uint64_t total_bytes;
    uint32_t total_packets;
    struct timeval start_time;
    struct timeval last_report;
};


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

void init_test_stats(struct test_stats *stats) {
    stats->total_bytes = 0;
    stats->total_packets = 0;
    gettimeofday(&stats->start_time, NULL);
    stats->last_report = stats->start_time;
}

void print_stats(struct test_stats *stats, struct timeval *current_time) {
    double elapsed = (current_time->tv_sec - stats->last_report.tv_sec) +
                    (current_time->tv_usec - stats->last_report.tv_usec) / 1000000.0;
    double total_elapsed = (current_time->tv_sec - stats->start_time.tv_sec) +
                          (current_time->tv_usec - stats->start_time.tv_usec) / 1000000.0;

    double bits_per_second = (stats->total_bytes * 8.0) / elapsed;

    printf("[%.1f-%.1f sec] %lu bytes %.2f Mbits/sec\n",
           total_elapsed - 1.0, total_elapsed,
           stats->total_bytes,
           bits_per_second / 1000000.0);

    stats->total_bytes = 0;
    stats->last_report = *current_time;
}

void run_iperf_server(int sock, struct sockaddr_in *peer_addr) {
    struct iperf_packet packet;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    struct test_stats stats;
    struct timeval current_time;

    printf("Running in server mode...\n");
    init_test_stats(&stats);

    while (1) {
        ssize_t recv_len = recvfrom(sock, &packet, sizeof(packet), 0,
                                  (struct sockaddr*)&client_addr, &addr_len);

        if (recv_len > 0) {
            gettimeofday(&current_time, NULL);
            stats.total_bytes += recv_len;
            stats.total_packets++;

            if (current_time.tv_sec >= stats.last_report.tv_sec + REPORT_INTERVAL) {
                print_stats(&stats, &current_time);
            }
        }
    }
}

void run_iperf_client(int sock, struct sockaddr_in *peer_addr) {
    struct iperf_packet packet;
    struct test_stats stats;
    struct timeval current_time;
    uint32_t packet_id = 0;

    printf("Running in client mode...\n");
    printf("Sending %d byte packets for %d seconds\n", IPERF_PACKET_SIZE, TEST_DURATION);

    init_test_stats(&stats);
    memset(&packet, 'a', sizeof(packet));

    while (1) {
        gettimeofday(&current_time, NULL);

        if (current_time.tv_sec >= stats.start_time.tv_sec + TEST_DURATION) {
            break;
        }

        packet.packet_id = htonl(packet_id++);
        packet.tv_sec = htonl(current_time.tv_sec);
        packet.tv_usec = htonl(current_time.tv_usec);

        ssize_t sent_len = sendto(sock, &packet, sizeof(packet), 0,
                                (struct sockaddr*)peer_addr, sizeof(*peer_addr));

        if (sent_len > 0) {
            stats.total_bytes += sent_len;
            stats.total_packets++;

            if (current_time.tv_sec >= stats.last_report.tv_sec + REPORT_INTERVAL) {
                print_stats(&stats, &current_time);
            }
        }

        usleep(1000);  // 1ms delay
    }

    gettimeofday(&current_time, NULL);
    print_stats(&stats, &current_time);
    printf("\nTest completed. Sent %u packets\n", stats.total_packets);
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
    printf("Commands:\n");
    printf("  /iperf server - Start iperf server mode\n");
    printf("  /iperf client - Start iperf client mode\n");
    printf("  /quit - Exit program\n");

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

            if (strncmp(buffer, "/iperf server", 12) == 0) {
                printf("Starting iperf server mode...\n");
                run_iperf_server(sock, &peer_addr);
            }
            else if (strncmp(buffer, "/iperf client", 12) == 0) {
                printf("Starting iperf client mode...\n");
                run_iperf_client(sock, &peer_addr);
            }
            else if (strncmp(buffer, "/quit", 5) == 0) {
                break;
            }
            else {
                if (sendto(sock, buffer, strlen(buffer), 0,
                          (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                    perror("Send failed");
                }
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

            if (strcmp(buffer, "KEEPALIVE") != 0) {
                printf("Peer: %s", buffer);
                last_activity = current_time;
            }
        }

        if (current_time - last_activity >= KEEPALIVE_INTERVAL) {
            send_keepalive(sock, &peer_addr);
            last_activity = current_time;
        }
    }

    close(sock);
    return 0;
}

