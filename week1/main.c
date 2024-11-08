#include <stdio.h>
#include <stdlib.h>
#include "STUNExternalIP.h" // STUNExternalIP 헤더 파일 포함

int main() {
    // 사용할 STUN 서버 설정 (첫 번째 서버만 사용)
    struct STUNServer server = {"stun.l.google.com", 19302};
    char address[100];
    unsigned short port;

    // 공인 IP와 포트를 가져오는 함수 호출
    int retVal = getPublicIPAddressAndPort(server, address, &port);

    // 결과 출력
    if (retVal == 0) {
        printf("Public IP: %s, Public Port: %d\n", address, port);
    } else {
        printf("Failed to retrieve public IP and port. Error: %d\n", retVal);
    }

    return 0;
}

