#include "common/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <mmsystem.h>
    #if defined(_MSC_VER)
        #pragma comment(lib, "ws2_32.lib")
        #pragma comment(lib, "winmm.lib")
    #endif
#else
    #include <signal.h>
#endif

int gw_platform_init(void) {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa_data;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (rc != 0) {
        return -1;
    }
    return 0;
#else
    /* Ignore SIGPIPE so writing to closed socket returns EPIPE instead of terminating process */
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

void gw_platform_cleanup(void) {
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#else
    /* No cleanup needed on POSIX */
#endif
}

bool gw_socket_is_valid(gw_socket_t sock) {
#if defined(_WIN32) || defined(_WIN64)
    return (sock != INVALID_SOCKET);
#else
    return (sock >= 0);
#endif
}

int gw_socket_close(gw_socket_t sock) {
    if (!gw_socket_is_valid(sock)) {
        return 0;
    }
#if defined(_WIN32) || defined(_WIN64)
    return closesocket(sock);
#else
    return close(sock);
#endif
}

int gw_socket_get_last_error(void) {
#if defined(_WIN32) || defined(_WIN64)
    return WSAGetLastError();
#else
    return errno;
#endif
}

const char* gw_socket_strerror(int err_code, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return "";
    }
#if defined(_WIN32) || defined(_WIN64)
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD res = FormatMessageA(flags, NULL, (DWORD)err_code, 0, buf, (DWORD)buf_size, NULL);
    if (res == 0) {
        snprintf(buf, buf_size, "Winsock error %d", err_code);
    } else {
        /* Strip trailing newlines */
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
            buf[--len] = '\0';
        }
    }
    return buf;
#else
    const char *msg = strerror(err_code);
    if (msg) {
        snprintf(buf, buf_size, "%s", msg);
    } else {
        snprintf(buf, buf_size, "Socket error %d", err_code);
    }
    return buf;
#endif
}

int gw_socket_set_reuseaddr(gw_socket_t sock, bool enable) {
    if (!gw_socket_is_valid(sock)) {
        return -1;
    }
#if defined(_WIN32) || defined(_WIN64)
    BOOL opt = enable ? TRUE : FALSE;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    int opt = enable ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
}

int gw_socket_set_timeouts(gw_socket_t sock, uint32_t timeout_ms) {
    if (!gw_socket_is_valid(sock)) {
        return -1;
    }
#if defined(_WIN32) || defined(_WIN64)
    DWORD tv = (DWORD)timeout_ms;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) != 0) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) != 0) {
        return -1;
    }
    return 0;
#else
    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000) * 1000);
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
        return -1;
    }
    return 0;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
typedef struct {
    gw_thread_fn_t func;
    void *arg;
} gw_win_thread_arg_t;

static DWORD WINAPI gw_win_thread_wrapper(LPVOID param) {
    gw_win_thread_arg_t *targ = (gw_win_thread_arg_t*)param;
    gw_thread_fn_t fn = targ->func;
    void *arg = targ->arg;
    free(targ);
    fn(arg);
    return 0;
}
#endif

int gw_thread_create(gw_thread_t *thread, gw_thread_fn_t func, void *arg) {
    if (!thread || !func) {
        return -1;
    }
#if defined(_WIN32) || defined(_WIN64)
    gw_win_thread_arg_t *targ = (gw_win_thread_arg_t*)malloc(sizeof(gw_win_thread_arg_t));
    if (!targ) {
        return -1;
    }
    targ->func = func;
    targ->arg = arg;

    HANDLE h = CreateThread(NULL, 0, gw_win_thread_wrapper, targ, 0, NULL);
    if (!h) {
        free(targ);
        return -1;
    }
    *thread = h;
    return 0;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

int gw_thread_join(gw_thread_t thread) {
#if defined(_WIN32) || defined(_WIN64)
    if (thread == NULL || thread == INVALID_HANDLE_VALUE) {
        return -1;
    }
    DWORD res = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (res == WAIT_OBJECT_0) ? 0 : -1;
#else
    return pthread_join(thread, NULL);
#endif
}

int gw_mutex_init(gw_mutex_t *mutex) {
    if (!mutex) return -1;
#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

int gw_mutex_lock(gw_mutex_t *mutex) {
    if (!mutex) return -1;
#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

int gw_mutex_unlock(gw_mutex_t *mutex) {
    if (!mutex) return -1;
#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

void gw_mutex_destroy(gw_mutex_t *mutex) {
    if (!mutex) return;
#if defined(_WIN32) || defined(_WIN64)
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void gw_sleep_ms(uint32_t ms) {
#if defined(_WIN32) || defined(_WIN64)
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

uint64_t gw_get_time_ms(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (uint64_t)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
#endif
}

uint16_t gw_htons(uint16_t val) {
    return htons(val);
}

uint16_t gw_ntohs(uint16_t val) {
    return ntohs(val);
}

uint32_t gw_htonl(uint32_t val) {
    return htonl(val);
}

uint32_t gw_ntohl(uint32_t val) {
    return ntohl(val);
}
