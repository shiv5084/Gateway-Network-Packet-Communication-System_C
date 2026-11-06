#include "unity/unity.h"
#include "common/gw_types.h"
#include "common/gw_config.h"
#include "common/platform.h"
#include "common/logger.h"
#include <stdio.h>
#include <string.h>

/* Global variables for thread test */
static int        s_thread_counter = 0;
static gw_mutex_t s_thread_mutex;

static void* thread_worker(void* arg) {
    int iterations = *(int*)arg;
    for (int i = 0; i < iterations; i++) {
        gw_mutex_lock(&s_thread_mutex);
        s_thread_counter++;
        gw_mutex_unlock(&s_thread_mutex);
    }
    return NULL;
}

void test_types_and_constants(void) {
    /* Verify failure codes specified in problem statement */
    TEST_ASSERT_EQUAL_HEX8(0x01, GW_FAIL_SRC_ADDR_INVALID);
    TEST_ASSERT_EQUAL_HEX8(0x02, GW_FAIL_TGT_ADDR_INVALID);
    TEST_ASSERT_EQUAL_HEX8(0x04, GW_FAIL_INTEGRITY);
    TEST_ASSERT_EQUAL_HEX8(0x08, GW_FAIL_LENGTH_MISMATCH);

    /* Verify addresses and port */
    TEST_ASSERT_EQUAL_UINT(13400, GW_DEFAULT_PORT);
    TEST_ASSERT_EQUAL_HEX16(0x0E80, GW_ADDR_NODE_1_REQ_SA);
    TEST_ASSERT_EQUAL_HEX16(0xE008, GW_ADDR_NODE_2_REQ_SA);
    TEST_ASSERT_EQUAL_HEX16(0x3108, GW_ADDR_GW_1_REQ_TA);
    TEST_ASSERT_EQUAL_HEX16(0x1380, GW_ADDR_GW_2_REQ_TA);

    /* Verify cryptographic XOR keys */
    TEST_ASSERT_EQUAL_HEX8(0xAA, GW_KEY_REQ_XOR);
    TEST_ASSERT_EQUAL_HEX8(0x55, GW_KEY_RESP_XOR);

    /* Verify CRC-8 polynomial */
    TEST_ASSERT_EQUAL_HEX8(0x07, GW_CRC8_POLYNOMIAL);
    TEST_ASSERT_EQUAL_HEX8(0x00, GW_CRC8_INITIAL_VAL);
}

void test_byte_order_conversions(void) {
    uint16_t val16 = 0x1234;
    uint16_t net16 = gw_htons(val16);
    uint16_t host16 = gw_ntohs(net16);
    TEST_ASSERT_EQUAL_HEX16(val16, host16);

    uint32_t val32 = 0x12345678;
    uint32_t net32 = gw_htonl(val32);
    uint32_t host32 = gw_ntohl(net32);
    TEST_ASSERT_EQUAL_HEX32(val32, host32);

    /* Test address conversion */
    uint16_t sa = GW_ADDR_NODE_1_REQ_SA; /* 0x0E80 */
    uint16_t sa_net = gw_htons(sa);
    uint8_t *p = (uint8_t*)&sa_net;
    /* In network byte order (Big Endian), 0x0E80 is stored as [0x0E, 0x80] */
    TEST_ASSERT_EQUAL_HEX8(0x0E, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, p[1]);
}

void test_time_and_sleep(void) {
    uint64_t t1 = gw_get_time_ms();
    gw_sleep_ms(25);
    uint64_t t2 = gw_get_time_ms();
    TEST_ASSERT_TRUE(t2 >= t1);
    TEST_ASSERT_TRUE((t2 - t1) >= 15); /* Slept at least 15 ms */
}

void test_mutex_and_threads(void) {
    TEST_ASSERT_EQUAL_INT(0, gw_mutex_init(&s_thread_mutex));
    s_thread_counter = 0;

    int count = 500;
    gw_thread_t th1, th2;

    TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&th1, thread_worker, &count));
    TEST_ASSERT_EQUAL_INT(0, gw_thread_create(&th2, thread_worker, &count));

    TEST_ASSERT_EQUAL_INT(0, gw_thread_join(th1));
    TEST_ASSERT_EQUAL_INT(0, gw_thread_join(th2));

    TEST_ASSERT_EQUAL_INT(1000, s_thread_counter);
    gw_mutex_destroy(&s_thread_mutex);
}

void test_socket_abstraction(void) {
    TEST_ASSERT_FALSE(gw_socket_is_valid(GW_INVALID_SOCKET));

    /* Test socket creation */
#if defined(_WIN32) || defined(_WIN64)
    gw_socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    gw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
    TEST_ASSERT_TRUE(gw_socket_is_valid(sock));

    TEST_ASSERT_EQUAL_INT(0, gw_socket_set_reuseaddr(sock, true));
    TEST_ASSERT_EQUAL_INT(0, gw_socket_set_timeouts(sock, 1000));

    TEST_ASSERT_EQUAL_INT(0, gw_socket_close(sock));
}

void test_logger(void) {
    const char *test_log_file = "test_phase1.log";

    TEST_ASSERT_EQUAL_INT(0, gw_log_init(GW_LOG_DEBUG, test_log_file));

    GW_LOG_D("TEST", "Debug message test: %d", 123);
    GW_LOG_I("TEST", "Info message test: %s", "hello");
    GW_LOG_W("TEST", "Warning message test");
    GW_LOG_E("TEST", "Error message test");

    uint8_t sample_packet[] = {0x0E, 0x80, 0x31, 0x08, 0x00, 0x00, 0x00, 0x02, 0xB8, 0x54};
    gw_log_hex_dump(GW_LOG_INFO, "TEST", "Sample Packet", sample_packet, sizeof(sample_packet));

    gw_log_close();

    /* Verify log file was written */
    FILE *f = fopen(test_log_file, "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) {
        char line[256];
        bool found = false;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "Sample Packet") != NULL) {
                found = true;
                break;
            }
        }
        fclose(f);
        TEST_ASSERT_TRUE(found);
    }
}
