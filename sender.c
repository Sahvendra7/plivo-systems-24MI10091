/* SENDER (C)
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver
 *   bind 47004  <- feedback from your receiver (optional, not used)
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#define PAYLOAD_SIZE 160
#define HISTORY_SIZE 16
#define FEC_A 1
#define FEC_B 2

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket in_fd");
        return 1;
    }
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr)) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket out_fd");
        return 1;
    }
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t history[HISTORY_SIZE][PAYLOAD_SIZE];
    memset(history, 0, sizeof(history));
    
    uint8_t in_buf[2048];
    uint8_t out_buf[512];

    for (;;) {
        ssize_t n = recvfrom(in_fd, in_buf, sizeof(in_buf), 0, NULL, NULL);
        if (n <= 0) continue;
        
        if (n != 4 + PAYLOAD_SIZE) {
            // Unexpected frame size from harness
            continue;
        }

        // Parse sequence number (4-byte big-endian)
        uint32_t seq;
        memcpy(&seq, in_buf, 4);
        uint32_t host_seq = ntohl(seq);

        // Store payload in history
        uint8_t *payload = in_buf + 4;
        memcpy(history[host_seq % HISTORY_SIZE], payload, PAYLOAD_SIZE);

        // Construct packet
        // First 4 bytes: sequence number (preserve big-endian from harness)
        memcpy(out_buf, in_buf, 4 + PAYLOAD_SIZE);

        size_t send_len = 4 + PAYLOAD_SIZE; // 164 bytes

        // Append FEC block with 95% probability (skip every 20th packet)
        if ((host_seq % 20) != 0 && host_seq >= FEC_B) {
            uint8_t *fec_dest = out_buf + 4 + PAYLOAD_SIZE;
            uint8_t *payload_a = history[(host_seq - FEC_A) % HISTORY_SIZE];
            uint8_t *payload_b = history[(host_seq - FEC_B) % HISTORY_SIZE];
            
            for (int i = 0; i < PAYLOAD_SIZE; i++) {
                fec_dest[i] = payload_a[i] ^ payload_b[i];
            }
            send_len += PAYLOAD_SIZE; // 324 bytes
        }

        sendto(out_fd, out_buf, send_len, 0, (struct sockaddr *)&relay, sizeof(relay));
    }
    return 0;
}
