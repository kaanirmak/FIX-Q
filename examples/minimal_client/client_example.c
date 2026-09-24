#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <finora/finora.h>

int main(int argc, char* argv[]) {
    const char* server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t port = (argc > 2) ? (uint16_t)atoi(argv[2]) : 5002;

    printf("[Client] Connecting to Finora Gateway at %s:%u...\n", server_ip, port);

    // 1. Establish Post-Quantum Cryptographic Session
    finora_ctx_t* ctx = finora_client_connect(server_ip, port, NULL);
    if (!ctx) {
        fprintf(stderr, "[Client Error] Failed to establish PQC session with gateway.\n");
        return 1;
    }
    printf("[Client] Handshake Complete! Quantum-safe session established.\n");

    // 2. Prepare sample FIX 4.2 NewOrderSingle (35=D) message
    const char* fix_msg = "8=FIX.4.2\x01" "9=74\x01" "35=D\x01" "49=CLIENT\x01" "56=EXCHANGE\x01"
                          "11=ORD-9901\x01" "55=THYAO.E\x01" "54=1\x01" "38=500\x01" "44=298.50\x01" "10=142\x01";

    // 3. Send encrypted message (0x01 = FIX)
    printf("[Client] Transmitting encrypted order: THYAO.E BUY 500 @ 298.50\n");
    int rc = finora_send(ctx, (const uint8_t*)fix_msg, strlen(fix_msg), 0x01);
    if (rc != 0) {
        fprintf(stderr, "[Client Error] Failed to send message: error code %d\n", rc);
    } else {
        printf("[Client] Successfully transmitted encrypted frame via wire.\n");
    }

    // 4. Clean teardown and secure memory wipe
    finora_disconnect(ctx);
    printf("[Client] Session closed cleanly. Keying material purged.\n");

    return 0;
}
