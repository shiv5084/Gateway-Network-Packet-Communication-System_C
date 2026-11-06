#ifndef GW_TYPES_H
#define GW_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Protocol Constants & Addresses
 * ========================================================================= */
#define GW_DEFAULT_PORT             13400
#define GW_DEFAULT_IP               "127.0.0.1"

#define GW_HEADER_SIZE              8
#define GW_CRC_SIZE                 1
#define GW_MIN_PACKET_SIZE          (GW_HEADER_SIZE + GW_CRC_SIZE) /* 9 bytes minimum */
#define GW_MAX_PAYLOAD_SIZE         65535
#define GW_MAX_FRAME_SIZE           (GW_HEADER_SIZE + GW_MAX_PAYLOAD_SIZE + GW_CRC_SIZE)

/* Valid Source Addresses (SA) for Requests */
#define GW_ADDR_NODE_1_REQ_SA       0x0E80
#define GW_ADDR_NODE_2_REQ_SA       0xE008

/* Valid Target Addresses (TA) for Requests */
#define GW_ADDR_GW_1_REQ_TA         0x3108
#define GW_ADDR_GW_2_REQ_TA         0x1380

/* Cryptographic XOR Keys */
#define GW_KEY_REQ_XOR              0xAA
#define GW_KEY_RESP_XOR             0x55

/* CRC-8 Parameters (Standard CRC-8 / SMBus) */
#define GW_CRC8_POLYNOMIAL          0x07
#define GW_CRC8_INITIAL_VAL         0x00

/* =========================================================================
 * Enumerations
 * ========================================================================= */

/**
 * @brief Gateway Protocol Failure Codes (Negative Response)
 */
typedef enum {
    GW_FAIL_NONE              = 0x00, /**< No failure / Request valid */
    GW_FAIL_SRC_ADDR_INVALID  = 0x01, /**< Source Address invalid */
    GW_FAIL_TGT_ADDR_INVALID  = 0x02, /**< Target Address invalid */
    GW_FAIL_INTEGRITY         = 0x04, /**< CRC-8 integrity verification failed */
    GW_FAIL_LENGTH_MISMATCH   = 0x08  /**< Payload length mismatch / buffer truncated */
} gw_failure_code_t;

/**
 * @brief Response frame classification
 */
typedef enum {
    GW_RESP_POSITIVE          = 0,    /**< Positive Response frame */
    GW_RESP_NEGATIVE          = 1     /**< Negative Response frame */
} gw_response_type_t;

/* =========================================================================
 * Data Structures
 * ========================================================================= */

/**
 * @brief Parsed Gateway Protocol Packet representation
 */
typedef struct {
    uint16_t source_addr;       /**< Source address (host byte order) */
    uint16_t target_addr;       /**< Target address (host byte order) */
    uint32_t length;            /**< Declared length: payload bytes + CRC byte */
    uint32_t payload_len;       /**< Payload byte count (length - 1) */
    const uint8_t *payload;     /**< Pointer to payload in raw frame buffer */
    uint8_t  crc;               /**< Received CRC-8 byte */
} gw_packet_t;

#ifdef __cplusplus
}
#endif

#endif /* GW_TYPES_H */
