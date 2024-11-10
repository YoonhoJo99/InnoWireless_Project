// client_common.h
#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#define BUFFER_SIZE 1024
#define HOLE_PUNCH_MSG "hole_punch"
#define ACK_MSG "ack"

// 홀펀칭 시도 구조체
struct hole_punch_info {
    char remote_ip[100];
    int remote_port;
    int local_sock;
    struct sockaddr_in remote_addr;
};

// 홀펀칭 함수
int create_hole_punch(struct hole_punch_info *info) {
    int max_attempts = 5;
    int attempt = 0;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(info->remote_addr);
    
    // 타임아웃 설정 (1초)
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(info->local_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    while (attempt < max_attempts) {
        printf("[LOG] 홀펀칭 시도 #%d...\n", attempt + 1);
        
        // 홀펀칭 메시지 전송
        if (sendto(info->local_sock, HOLE_PUNCH_MSG, strlen(HOLE_PUNCH_MSG), 0,
                  (struct sockaddr*)&info->remote_addr, sizeof(info->remote_addr)) < 0) {
            printf("[ERROR] 홀펀칭 메시지 전송 실패: %s\n", strerror(errno));
            return -1;
        }
        
        // ACK 수신 대기
        int recv_len = recvfrom(info->local_sock, buffer, BUFFER_SIZE, 0,
                              (struct sockaddr*)&info->remote_addr, &addr_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            if (strcmp(buffer, ACK_MSG) == 0) {
                printf("[LOG] 홀펀칭 성공!\n");
                
                // ACK에 대한 응답 전송
                sendto(info->local_sock, ACK_MSG, strlen(ACK_MSG), 0,
                      (struct sockaddr*)&info->remote_addr, sizeof(info->remote_addr));
                return 0;
            }
        }
        
        attempt++;
        usleep(500000); // 0.5초 대기
    }
    
    printf("[ERROR] 홀펀칭 실패: 최대 시도 횟수 초과\n");
    return -1;
}

#endif
