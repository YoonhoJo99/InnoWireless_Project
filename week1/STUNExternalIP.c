#include "STUNExternalIP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// STUN 메시지 헤더 구조체 (RFC 5389 섹션 6)
struct STUNMessageHeader {
    unsigned short type;
    unsigned short length;
    unsigned int cookie;
    unsigned int identifier[3];
};

#define XOR_MAPPED_ADDRESS_TYPE 0x0020

struct STUNAttributeHeader {
    unsigned short type;
    unsigned short length;
};

struct STUNXORMappedIPv4Address {
    unsigned char reserved;
    unsigned char family;
    unsigned short port;
    unsigned int address;
};

int getPublicIPAddressAndPort(struct STUNServer server, char* address, unsigned short* port) {
    int socketd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in localAddress;
    memset(&localAddress, 0, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = INADDR_ANY;
    localAddress.sin_port = htons(10000); // 포트를 10000번으로 고정

    if (bind(socketd, (struct sockaddr*) &localAddress, sizeof(localAddress)) < 0) {
        close(socketd);
        return -1;
    }

    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(server.address, NULL, &hints, &results) != 0) {
        close(socketd);
        return -2;
    }

    struct sockaddr_in remoteAddress;
    memset(&remoteAddress, 0, sizeof(remoteAddress));
    remoteAddress.sin_family = AF_INET;
    remoteAddress.sin_addr = ((struct sockaddr_in*) results->ai_addr)->sin_addr;
    remoteAddress.sin_port = htons(server.port);

    struct STUNMessageHeader request;
    request.type = htons(0x0001);
    request.length = htons(0x0000);
    request.cookie = htonl(0x2112A442);
    for (int i = 0; i < 3; i++) {
        srand((unsigned int) time(0) + i);
        request.identifier[i] = rand();
    }

    if (sendto(socketd, &request, sizeof(request), 0, (struct sockaddr*) &remoteAddress, sizeof(remoteAddress)) == -1) {
        freeaddrinfo(results);
        close(socketd);
        return -3;
    }

    struct timeval timeout = {5, 0};
    setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char buffer[512];
    memset(buffer, 0, sizeof(buffer));
    ssize_t length = recv(socketd, buffer, sizeof(buffer), 0);

    if (length < 0) {
        freeaddrinfo(results);
        close(socketd);
        return -4;
    }

    char* pointer = buffer;
    struct STUNMessageHeader* response = (struct STUNMessageHeader*) buffer;

    if (response->type == htons(0x0101)) {  // Binding Response 체크
        for (int index = 0; index < 3; index++) {
            if (request.identifier[index] != response->identifier[index]) {
                freeaddrinfo(results);
                close(socketd);
                return -1; // 식별자 불일치
            }
        }

        pointer += sizeof(struct STUNMessageHeader);

        // 응답 속성 확인
        while (pointer < buffer + length) {
            struct STUNAttributeHeader* header = (struct STUNAttributeHeader*) pointer;

            if (header->type == htons(XOR_MAPPED_ADDRESS_TYPE)) {
                pointer += sizeof(struct STUNAttributeHeader);
                struct STUNXORMappedIPv4Address* xorAddress = (struct STUNXORMappedIPv4Address*) pointer;

                *port = ntohs(xorAddress->port) ^ 0x2112;
                unsigned int numAddress = xorAddress->address ^ htonl(0x2112A442);
                snprintf(address, 20, "%d.%d.%d.%d",
                         (numAddress >> 24) & 0xFF,
                         (numAddress >> 16) & 0xFF,
                         (numAddress >> 8)  & 0xFF,
                         numAddress & 0xFF);

                freeaddrinfo(results);
                close(socketd);
                return 0;
            }

            pointer += sizeof(struct STUNAttributeHeader) + ntohs(header->length);
        }
    }

    freeaddrinfo(results);
    close(socketd);
    return -5; // 외부 주소 획득 실패
}

