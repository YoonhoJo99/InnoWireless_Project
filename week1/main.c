#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "STUNExternalIP.h"

int main(int argc, const char * argv[]) {
    struct STUNServer servers[14] = {
        {"stun.l.google.com", 19302}, {"stun.l.google.com", 19305},
        {"stun1.l.google.com", 19302}, {"stun1.l.google.com", 19305},
        {"stun2.l.google.com", 19302}, {"stun2.l.google.com", 19305},
        {"stun3.l.google.com", 19302}, {"stun3.l.google.com", 19305},
        {"stun4.l.google.com", 19302}, {"stun4.l.google.com", 19305},
        {"stun.wtfismyip.com", 3478}, {"stun.bcs2005.net", 3478},
        {"numb.viagenie.ca", 3478}, {"173.194.202.127", 19302}
    };

    for (int index = 0; index < 14; index++) {
        char address[100];
        unsigned short port;
        struct STUNServer server = servers[index];

        int retVal = getPublicIPAddressAndPort(server, address, &port);

        if (retVal != 0) {
            printf("%s: Failed. Error: %d\n", server.address, retVal);
        } else {
            printf("%s: Public IP: %s, Public Port: %d\n", server.address, address, port);
        }
    }
    return 0;
}
