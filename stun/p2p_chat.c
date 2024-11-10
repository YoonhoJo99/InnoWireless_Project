#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define PORT 12345

int create_udp_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    return sock;
}

void bind_socket(int sock, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
}

void send_message(int sock, const char* msg, const char* ip, int port) {
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &peer_addr.sin_addr);

    sendto(sock, msg, strlen(msg), 0, 
           (struct sockaddr*)&peer_addr, sizeof(peer_addr));
}

void start_p2p_communication(const char* peer_ip, int peer_port) {
    int sock = create_udp_socket();
    bind_socket(sock, PORT);

    printf("Starting P2P communication with peer %s:%d\n", peer_ip, peer_port);
    
    // NAT 홀펀칭을 위한 초기 메시지 전송
    send_message(sock, "NAT_PROBE", peer_ip, peer_port);
    
    char buffer[BUFFER_SIZE];
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    
    fd_set readfds;
    struct timeval tv;
    int retry_count = 0;
    const int MAX_RETRIES = 5;

    while (retry_count < MAX_RETRIES) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
        
        if (ready < 0) {
            perror("Select failed");
            break;
        } else if (ready == 0) {
            printf("Timeout, retrying... (%d/%d)\n", retry_count + 1, MAX_RETRIES);
            send_message(sock, "NAT_PROBE", peer_ip, peer_port);
            retry_count++;
            continue;
        }

        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&peer_addr, &addr_len);
        
        if (recv_len < 0) {
            perror("Receive failed");
            continue;
        }

        buffer[recv_len] = '\0';
        char peer_addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_addr_str, INET_ADDRSTRLEN);
        
        printf("Received message from %s:%d: %s\n", 
               peer_addr_str, ntohs(peer_addr.sin_port), buffer);

        // NAT 홀펀칭이 성공하면 실제 통신 시작
        if (strcmp(buffer, "NAT_PROBE") == 0) {
            printf("NAT hole punching successful!\n");
            // 연결 성공 메시지 전송
            send_message(sock, "CONNECTION_ESTABLISHED", peer_addr_str, ntohs(peer_addr.sin_port));
            break;
        }
    }

    if (retry_count >= MAX_RETRIES) {
        printf("Failed to establish P2P connection after %d attempts\n", MAX_RETRIES);
        close(sock);
        return;
    }

    // 실제 P2P 통신 루프
    printf("Enter message (or 'quit' to exit):\n");
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        select(sock + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "quit") == 0) {
                break;
            }

            send_message(sock, buffer, peer_ip, peer_port);
        }

        if (FD_ISSET(sock, &readfds)) {
            int recv_len = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                  (struct sockaddr*)&peer_addr, &addr_len);
            if (recv_len > 0) {
                buffer[recv_len] = '\0';
                printf("Peer: %s\n", buffer);
            }
        }
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <peer_ip> <peer_port>\n", argv[0]);
        return 1;
    }

    const char* peer_ip = argv[1];
    int peer_port = atoi(argv[2]);

    start_p2p_communication(peer_ip, peer_port);
    return 0;
}
