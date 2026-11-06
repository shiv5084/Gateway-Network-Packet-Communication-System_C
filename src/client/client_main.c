/**
 * @file client_main.c
 * @brief Entry point for the Gateway (GW) Diagnostic Client executable (Phase 5).
 */
#include "client/tcp_client.h"
#include "client/request_builder.h"
#include "client/response_parser.h"
#include "common/platform.h"
#include "common/logger.h"
#include "common/gw_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GW_CLIENT_VERSION "1.0.0"

static void print_usage(const char *prog) {
    printf("Gateway Diagnostic Client v%s\n", GW_CLIENT_VERSION);
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --ip <ip>           Server IP address (default: 127.0.0.1)\n");
    printf("  --port <port>       Server port (default: %u)\n", GW_DEFAULT_PORT);
    printf("  --test              Run the 5 protocol conformance test scenarios\n");
    printf("  --count <N>         Number of iterations to run (default: 1)\n");
    printf("  --client-id <ID>    Client identifier for log differentiation\n");
    printf("  --interactive       Launch interactive diagnostic command loop\n");
    printf("  --verbose           Enable debug logging and byte tracing\n");
    printf("  --help              Display this help message\n");
}

static int run_conformance_tests(gw_client_t *client) {
    printf("============================================================\n");
    printf(" Running Gateway Protocol Conformance Test Scenarios\n");
    printf("============================================================\n\n");

    int passes = 0;
    int total = 5;

    /* Scenario 1: Valid Single-Byte Request (0x12 -> 0x21) */
    {
        printf("[Scenario 1] Valid Request (SA=0x0E80, TA=0x3108, Payload=0x12) ... ");
        uint8_t payload[] = {0x12};
        uint8_t req[64];
        size_t req_len = 0;
        gw_client_build_request(0x0E80, 0x3108, payload, 1, req, sizeof(req), &req_len);

        gw_client_response_t resp;
        if (gw_client_send_request(client, req, req_len, &resp) == 0 &&
            resp.type == GW_RESP_POSITIVE &&
            resp.payload_len == 1 &&
            resp.decrypted_payload[0] == 0x21 &&
            resp.source_addr == 0x3108 &&
            resp.target_addr == 0x0E80) {
            printf("PASS (Received 0x21)\n");
            passes++;
        } else {
            printf("FAIL\n");
        }
    }

    /* Scenario 2: Invalid Source Address (0xAABB -> Fail 0x01) */
    {
        printf("[Scenario 2] Invalid SA (SA=0xAABB, TA=0x3108) ... ");
        uint8_t payload[] = {0x12};
        uint8_t req[64];
        size_t req_len = 0;
        gw_client_build_request(0xAABB, 0x3108, payload, 1, req, sizeof(req), &req_len);

        gw_client_response_t resp;
        if (gw_client_send_request(client, req, req_len, &resp) == 0 &&
            resp.type == GW_RESP_NEGATIVE &&
            resp.failure_code == GW_FAIL_SRC_ADDR_INVALID) {
            printf("PASS (FailCode: 0x01)\n");
            passes++;
        } else {
            printf("FAIL\n");
        }
    }

    /* Scenario 3: Invalid Target Address (0x9999 -> Fail 0x02) */
    {
        printf("[Scenario 3] Invalid TA (SA=0x0E80, TA=0x9999) ... ");
        uint8_t payload[] = {0x12};
        uint8_t req[64];
        size_t req_len = 0;
        gw_client_build_request(0x0E80, 0x9999, payload, 1, req, sizeof(req), &req_len);

        gw_client_response_t resp;
        if (gw_client_send_request(client, req, req_len, &resp) == 0 &&
            resp.type == GW_RESP_NEGATIVE &&
            resp.failure_code == GW_FAIL_TGT_ADDR_INVALID) {
            printf("PASS (FailCode: 0x02)\n");
            passes++;
        } else {
            printf("FAIL\n");
        }
    }

    /* Scenario 4: Corrupted Integrity Check (Bad CRC -> Fail 0x04) */
    {
        printf("[Scenario 4] Corrupted Integrity Check (Bad CRC) ... ");
        uint8_t payload[] = {0x12};
        uint8_t req[64];
        size_t req_len = 0;
        gw_client_build_corrupt_request(0x0E80, 0x3108, payload, 1,
                                       true, 0, req, sizeof(req), &req_len);

        gw_client_response_t resp;
        if (gw_client_send_request(client, req, req_len, &resp) == 0 &&
            resp.type == GW_RESP_NEGATIVE &&
            resp.failure_code == GW_FAIL_INTEGRITY) {
            printf("PASS (FailCode: 0x04)\n");
            passes++;
        } else {
            printf("FAIL\n");
        }
    }

    /* Scenario 5: Length Mismatch (length=0 in header -> Fail 0x08) */
    {
        printf("[Scenario 5] Length Mismatch (length=0 in header) ... ");
        uint8_t req[8] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x00};
        size_t req_len = sizeof(req);

        gw_client_response_t resp;
        if (gw_client_send_request(client, req, req_len, &resp) == 0 &&
            resp.type == GW_RESP_NEGATIVE &&
            resp.failure_code == GW_FAIL_LENGTH_MISMATCH) {
            printf("PASS (FailCode: 0x08)\n");
            passes++;
        } else {
            printf("FAIL\n");
        }
    }

    printf("\n============================================================\n");
    printf(" Results: %d/%d tests passed (%s)\n", passes, total,
           passes == total ? "ALL PASSED" : "FAILED");
    printf("============================================================\n");

    return (passes == total) ? 0 : 1;
}

/**
 * @brief Parses space-separated hex bytes from user input.
 *        Example: "12 34 AB cd" -> {0x12, 0x34, 0xAB, 0xCD}
 */
static size_t parse_hex_string(const char *input, uint8_t *out_buf, size_t max_out) {
    size_t count = 0;
    const char *p = input;

    while (*p && count < max_out) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char byte_hex[3] = {0};
        if (isxdigit((unsigned char)*p)) {
            byte_hex[0] = *p++;
            if (isxdigit((unsigned char)*p)) {
                byte_hex[1] = *p++;
            }
            out_buf[count++] = (uint8_t)strtoul(byte_hex, NULL, 16);
        } else {
            p++;
        }
    }
    return count;
}

static void run_interactive_mode(gw_client_t *client) {
    printf("============================================================\n");
    printf(" Gateway Diagnostic Client — Interactive CLI Mode\n");
    printf(" Enter 'q' or 'exit' at any prompt to quit.\n");
    printf(" Default Addresses:\n");
    printf("   SA: 0x0E80 (Node 1) or 0xE008 (Node 2)\n");
    printf("   TA: 0x3108 (Gateway 1) or 0x1380 (Gateway 2)\n");
    printf("============================================================\n\n");

    char line[256];
    while (1) {
        printf("gw-client> Enter SA (hex, default 0E80): ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q") == 0 || strcmp(line, "exit") == 0) break;

        uint16_t sa = 0x0E80;
        if (strlen(line) > 0) {
            sa = (uint16_t)strtoul(line, NULL, 16);
        }

        printf("gw-client> Enter TA (hex, default 3108): ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q") == 0 || strcmp(line, "exit") == 0) break;

        uint16_t ta = 0x3108;
        if (strlen(line) > 0) {
            ta = (uint16_t)strtoul(line, NULL, 16);
        }

        printf("gw-client> Enter Payload Hex Bytes (e.g. 12 34 AB, default 12): ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q") == 0 || strcmp(line, "exit") == 0) break;

        uint8_t payload[256];
        size_t payload_len = 0;
        if (strlen(line) > 0) {
            payload_len = parse_hex_string(line, payload, sizeof(payload));
        } else {
            payload[0] = 0x12;
            payload_len = 1;
        }

        printf("Packing request: SA=0x%04X, TA=0x%04X, PayloadLen=%zu\n",
               sa, ta, payload_len);

        uint8_t req_buf[GW_MAX_FRAME_SIZE + 16];
        size_t req_len = 0;
        if (gw_client_build_request(sa, ta, payload, (uint32_t)payload_len,
                                    req_buf, sizeof(req_buf), &req_len) != 0) {
            printf("Error building request frame!\n");
            continue;
        }

        printf("TX Frame (%zu bytes):", req_len);
        for (size_t i = 0; i < req_len; i++) {
            printf(" %02X", req_buf[i]);
        }
        printf("\n");

        gw_client_response_t resp;
        if (gw_client_send_request(client, req_buf, req_len, &resp) == 0) {
            gw_client_print_response(&resp);
        } else {
            printf("Error: Failed to receive response from server.\n");
        }
        printf("\n");
    }

    printf("Exiting interactive mode.\n");
}

int main(int argc, char *argv[]) {
    const char *ip = "127.0.0.1";
    uint16_t port = GW_DEFAULT_PORT;
    bool run_test = false;
    bool interactive = false;
    bool verbose = false;
    int count = 1;
    const char *client_id = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
            ip = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--test") == 0) {
            run_test = true;
        } else if (strcmp(argv[i], "--interactive") == 0) {
            interactive = true;
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count < 1) count = 1;
        } else if (strcmp(argv[i], "--client-id") == 0 && i + 1 < argc) {
            client_id = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (gw_platform_init() != 0) {
        fprintf(stderr, "Failed to initialize platform network layer.\n");
        return 1;
    }

    gw_log_init(verbose ? GW_LOG_DEBUG : GW_LOG_INFO, NULL);

    gw_client_t client;
    gw_client_init(&client, verbose);

    if (gw_client_connect(&client, ip, port) != 0) {
        if (client_id) {
            fprintf(stderr, "[Client %s] Could not connect to server at %s:%u\n", client_id, ip, port);
        } else {
            fprintf(stderr, "Could not connect to server at %s:%u\n", ip, port);
        }
        gw_log_close();
        gw_platform_cleanup();
        return 1;
    }

    int rc = 0;
    if (run_test) {
        int total_passed = 0;
        for (int iter = 0; iter < count; iter++) {
            if (count > 1 && !client_id) {
                printf("\n--- Test Iteration %d/%d ---\n", iter + 1, count);
            }
            if (run_conformance_tests(&client) == 0) {
                total_passed++;
            }
        }
        rc = (total_passed == count) ? 0 : 1;
    } else if (interactive) {
        run_interactive_mode(&client);
    } else {
        /* Send 'count' requests and track packet transfer & loss */
        int sent_count = 0;
        int recv_count = 0;
        int loss_count = 0;

        uint8_t payload[] = {0x12};
        uint8_t req[64];
        size_t req_len = 0;
        gw_client_build_request(0x0E80, 0x3108, payload, 1, req, sizeof(req), &req_len);

        const char *cid = client_id ? client_id : "1";
        printf("[Client %s] Streaming %d request packets to %s:%u...\n", cid, count, ip, port);

        for (int i = 0; i < count; i++) {
            sent_count++;
            gw_client_response_t resp;
            if (gw_client_send_request(&client, req, req_len, &resp) == 0) {
                recv_count++;
                if (count == 1) {
                    gw_client_print_response(&resp);
                }
            } else {
                loss_count++;
            }
        }

        double loss_rate = sent_count > 0 ? ((double)loss_count / sent_count) * 100.0 : 0.0;
        printf("[Client %s] Summary: Sent=%d | Received=%d | Lost=%d (Loss Rate: %.2f%%)\n",
               cid, sent_count, recv_count, loss_count, loss_rate);
        rc = (loss_count == 0) ? 0 : 1;
    }

    gw_client_disconnect(&client);
    gw_log_close();
    gw_platform_cleanup();

    return rc;
}
