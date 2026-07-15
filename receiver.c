/* RECEIVER (C)
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender (optional, not used)
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
#define WINDOW_SIZE 256
#define FEC_A 1
#define FEC_B 2

void forward_packet(uint32_t seq, uint8_t *payload, int fd, struct sockaddr_in *addr) {
    uint8_t buf[4 + PAYLOAD_SIZE];
    uint32_t net_seq = htonl(seq);
    memcpy(buf, &net_seq, 4);
    memcpy(buf + 4, payload, PAYLOAD_SIZE);
    sendto(fd, buf, sizeof(buf), 0, (struct sockaddr *)addr, sizeof(*addr));
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket in_fd");
        return 1;
    }
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr)) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket out_fd");
        return 1;
    }
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    bool seen[WINDOW_SIZE];
    bool has_fec[WINDOW_SIZE];
    uint8_t payloads[WINDOW_SIZE][PAYLOAD_SIZE];
    uint8_t fecs[WINDOW_SIZE][PAYLOAD_SIZE];
    
    memset(seen, 0, sizeof(seen));
    memset(has_fec, 0, sizeof(has_fec));

    uint32_t max_seq = 0;
    bool first_packet = true;
    uint8_t in_buf[2048];

    for (;;) {
        ssize_t n = recvfrom(in_fd, in_buf, sizeof(in_buf), 0, NULL, NULL);
        if (n <= 0) continue;
        
        // Accept only our two specific packet formats
        if (n != 4 + PAYLOAD_SIZE && n != 4 + 2 * PAYLOAD_SIZE) {
            continue; // Malformed
        }

        uint32_t seq;
        memcpy(&seq, in_buf, 4);
        seq = ntohl(seq);

        uint8_t *payload = in_buf + 4;
        
        if (first_packet) {
            max_seq = seq;
            first_packet = false;
        }

        // Slide the window forward if we receive a newer packet
        if (seq > max_seq) {
            if (seq > max_seq + WINDOW_SIZE) {
                // Huge gap, just reset the window arrays
                memset(seen, 0, sizeof(seen));
                memset(has_fec, 0, sizeof(has_fec));
            } else {
                for (uint32_t i = max_seq + 1; i <= seq; i++) {
                    seen[i % WINDOW_SIZE] = false;
                    has_fec[i % WINDOW_SIZE] = false;
                }
            }
            max_seq = seq;
        }

        // Check if the packet is too old for our window
        if (max_seq >= WINDOW_SIZE && seq < max_seq - WINDOW_SIZE + 1) {
            // It's outside our FEC window. Forward it directly just in case it's useful.
            // The harness player ignores duplicates automatically.
            forward_packet(seq, payload, out_fd, &player);
            continue;
        }

        // Duplicate check within window
        if (seen[seq % WINDOW_SIZE]) {
            // Already received and forwarded
            continue;
        }

        // Mark as seen, store payload, and forward immediately
        seen[seq % WINDOW_SIZE] = true;
        memcpy(payloads[seq % WINDOW_SIZE], payload, PAYLOAD_SIZE);
        forward_packet(seq, payload, out_fd, &player);

        // Store FEC if present
        if (n == 4 + 2 * PAYLOAD_SIZE) {
            has_fec[seq % WINDOW_SIZE] = true;
            memcpy(fecs[seq % WINDOW_SIZE], in_buf + 4 + PAYLOAD_SIZE, PAYLOAD_SIZE);
        }

        // Iterative Recovery Loop
        bool changed;
        do {
            changed = false;
            uint32_t start = (max_seq >= WINDOW_SIZE) ? (max_seq - WINDOW_SIZE + 1) : 0;
            
            for (uint32_t k = start; k <= max_seq; k++) {
                if (!has_fec[k % WINDOW_SIZE]) continue;
                if (k < FEC_A || k < FEC_B) continue; // Prevent underflow
                
                uint32_t a = k - FEC_A;
                uint32_t b = k - FEC_B;
                
                // Only recover if a and b are within the valid window
                if (a < start || b < start) continue;
                
                bool seen_a = seen[a % WINDOW_SIZE];
                bool seen_b = seen[b % WINDOW_SIZE];
                
                if (seen_a && !seen_b) {
                    for (int i = 0; i < PAYLOAD_SIZE; i++) {
                        payloads[b % WINDOW_SIZE][i] = fecs[k % WINDOW_SIZE][i] ^ payloads[a % WINDOW_SIZE][i];
                    }
                    seen[b % WINDOW_SIZE] = true;
                    forward_packet(b, payloads[b % WINDOW_SIZE], out_fd, &player);
                    changed = true;
                } else if (seen_b && !seen_a) {
                    for (int i = 0; i < PAYLOAD_SIZE; i++) {
                        payloads[a % WINDOW_SIZE][i] = fecs[k % WINDOW_SIZE][i] ^ payloads[b % WINDOW_SIZE][i];
                    }
                    seen[a % WINDOW_SIZE] = true;
                    forward_packet(a, payloads[a % WINDOW_SIZE], out_fd, &player);
                    changed = true;
                }
            }
        } while (changed);
    }
    return 0;
}
