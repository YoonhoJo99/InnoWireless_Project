#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "STUNExternalIP.h" // STUNExternalIP 헤더 파일 포함

int main() {
    struct STUNServer server = {"stun.l.google.com", 19302}; // 사용할 STUN 서버
    char address[100];
    unsigned short port;

    // STUN 요청을 30초마다 반복하여 NAT 포트 유지
    // NAT는 일정 시간 동안 트래픽이 없으면 포트를 해제한다. 너무 길게하면 안 된다.
    while (1) {
        int retVal = getPublicIPAddressAndPort(server, address, &port);

        if (retVal == 0) {
            printf("Public IP: %s, Public Port: %d\n", address, port);
        } else {
            printf("Failed to retrieve public IP and port. Error: %d\n", retVal);
        }

        // 30초 대기 후 STUN 요청 반복
        sleep(30);
    }

    return 0;
}

