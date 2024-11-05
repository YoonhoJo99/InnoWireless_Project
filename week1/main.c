#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "STUNExternalIP.h" // STUNExternalIP 헤더 파일 포함

int main() {
    // 사용할 STUN 서버 설정 (하나의 서버만 사용)
    struct STUNServer server = {"stun.l.google.com", 19302};
    
    // 공인 IP 주소와 포트를 저장할 변수
    char address[100];
    unsigned short port;
    
    // 공인 IP 주소 및 포트 요청
    int retVal = getPublicIPAddressAndPort(server, address, &port);
    
    // 결과 출력
    if (retVal != 0) {
        // 요청이 실패한 경우 에러 메시지 출력
        printf("Failed to retrieve public IP and port from %s. Error: %d\n", server.address, retVal);
    } else {
        // 성공적으로 공인 IP와 포트를 가져온 경우 출력
        printf("Public IP: %s, Public Port: %d\n", address, port);
    }
    
    return 0; // 프로그램 종료
}
