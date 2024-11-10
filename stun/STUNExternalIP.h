#ifndef STUNExternalIP_h
#define STUNExternalIP_h

// STUN 서버 구조체 정의
struct STUNServer {
    char* address;         // STUN 서버의 주소
    unsigned short port;   // STUN 서버의 포트
};

///
/// Get the external IPv4 address and port
///
/// @param server A valid STUN server
/// @param address A non-null buffer to store the public IPv4 address
/// @param port A pointer to store the public port number
/// @return 0 on success.
/// @warning This function returns
///          -1 if failed to bind the socket;        // 소켓 바인딩 실패
///          -2 if failed to resolve the given STUN server; // STUN 서버 해석 실패
///          -3 if failed to send the STUN request; // STUN 요청 전송 실패
///          -4 if failed to read from the socket (and timed out; default = 5s); // 소켓 읽기 실패 (타임아웃)
///          -5 if failed to get the external address. // 외부 주소 가져오기 실패
///
int getPublicIPAddressAndPort(struct STUNServer server, char* address, unsigned short* port);

#endif /* STUNExternalIP_h */
