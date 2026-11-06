#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UnityTestFunction)(void);

typedef struct {
    const char* TestFile;
    const char* CurrentTestName;
    int         CurrentTestLineNumber;
    int         NumberOfTests;
    int         TestFailures;
    int         TestIgnores;
    int         CurrentTestFailed;
} UnityStorage_t;

extern UnityStorage_t Unity;

void UnityAssertTrue(bool condition, int line, const char* msg);
void UnityAssertNull(const void* ptr, int line, const char* msg);
void UnityAssertNotNull(const void* ptr, int line, const char* msg);
void UnityAssertEqualInt(int expected, int actual, int line, const char* msg);
void UnityAssertEqualUInt(unsigned int expected, unsigned int actual, int line, const char* msg);
void UnityAssertEqualHex8(uint8_t expected, uint8_t actual, int line, const char* msg);
void UnityAssertEqualHex16(uint16_t expected, uint16_t actual, int line, const char* msg);
void UnityAssertEqualHex32(uint32_t expected, uint32_t actual, int line, const char* msg);
void UnityAssertEqualString(const char* expected, const char* actual, int line, const char* msg);
void UnityAssertEqualMemory(const void* expected, const void* actual, size_t length, int line, const char* msg);
void UnityAssertFail(int line, const char* msg);

#ifdef __cplusplus
}
#endif

#endif /* UNITY_INTERNALS_H */
