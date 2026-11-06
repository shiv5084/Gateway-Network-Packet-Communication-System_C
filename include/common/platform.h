#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Platform-Specific Headers & Types
 * ========================================================================= */
#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    typedef SOCKET            gw_socket_t;
    typedef HANDLE            gw_thread_t;
    typedef CRITICAL_SECTION  gw_mutex_t;

    #define GW_INVALID_SOCKET INVALID_SOCKET
    #define GW_SOCKET_ERROR   SOCKET_ERROR

#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
    #include <time.h>

    typedef int               gw_socket_t;
    typedef pthread_t         gw_thread_t;
    typedef pthread_mutex_t   gw_mutex_t;

    #define GW_INVALID_SOCKET (-1)
    #define GW_SOCKET_ERROR   (-1)

#endif

/* Thread entry point signature */
typedef void* (*gw_thread_fn_t)(void *arg);

/* =========================================================================
 * Subsystem Lifecycle API
 * ========================================================================= */

/**
 * @brief Initializes the platform networking subsystem (e.g., WSAStartup).
 * @return 0 on success, negative error code on failure.
 */
int gw_platform_init(void);

/**
 * @brief Cleans up the platform networking subsystem (e.g., WSACleanup).
 */
void gw_platform_cleanup(void);

/* =========================================================================
 * Socket API Abstraction
 * ========================================================================= */

/**
 * @brief Checks if a socket descriptor is valid.
 */
bool gw_socket_is_valid(gw_socket_t sock);

/**
 * @brief Closes an active socket safely across platforms.
 * @return 0 on success, -1 on failure.
 */
int gw_socket_close(gw_socket_t sock);

/**
 * @brief Retrieves the last platform socket error code.
 */
int gw_socket_get_last_error(void);

/**
 * @brief Formats the last socket error code into a human-readable string.
 */
const char* gw_socket_strerror(int err_code, char *buf, size_t buf_size);

/**
 * @brief Sets SO_REUSEADDR on the specified socket.
 */
int gw_socket_set_reuseaddr(gw_socket_t sock, bool enable);

/**
 * @brief Sets send and receive timeouts in milliseconds.
 */
int gw_socket_set_timeouts(gw_socket_t sock, uint32_t timeout_ms);

/* =========================================================================
 * Threading & Synchronization API
 * ========================================================================= */

/**
 * @brief Creates and starts a worker thread.
 * @param thread Output pointer to thread handle
 * @param func Thread entry function
 * @param arg User argument passed to func
 * @return 0 on success, non-zero on failure.
 */
int gw_thread_create(gw_thread_t *thread, gw_thread_fn_t func, void *arg);

/**
 * @brief Waits for a worker thread to terminate and frees handle.
 */
int gw_thread_join(gw_thread_t thread);

/**
 * @brief Initializes a cross-platform mutex.
 */
int gw_mutex_init(gw_mutex_t *mutex);

/**
 * @brief Locks a mutex.
 */
int gw_mutex_lock(gw_mutex_t *mutex);

/**
 * @brief Unlocks a mutex.
 */
int gw_mutex_unlock(gw_mutex_t *mutex);

/**
 * @brief Destroys a mutex.
 */
void gw_mutex_destroy(gw_mutex_t *mutex);

/* =========================================================================
 * Time & Byte Order Utilities
 * ========================================================================= */

/**
 * @brief Suspends execution for the specified milliseconds.
 */
void gw_sleep_ms(uint32_t ms);

/**
 * @brief Returns current monotonic timestamp in milliseconds.
 */
uint64_t gw_get_time_ms(void);

/**
 * @brief Converts 16-bit uint from host to network byte order.
 */
uint16_t gw_htons(uint16_t val);

/**
 * @brief Converts 16-bit uint from network to host byte order.
 */
uint16_t gw_ntohs(uint16_t val);

/**
 * @brief Converts 32-bit uint from host to network byte order.
 */
uint32_t gw_htonl(uint32_t val);

/**
 * @brief Converts 32-bit uint from network to host byte order.
 */
uint32_t gw_ntohl(uint32_t val);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
