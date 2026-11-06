#include "unity.h"
#include <stdio.h>
#include <string.h>

UnityStorage_t Unity;

void UnityBegin(const char* filename) {
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    printf("======================================================================\n");
    printf("Running Unit Tests: %s\n", filename);
    printf("======================================================================\n");
}

int UnityEnd(void) {
    printf("----------------------------------------------------------------------\n");
    printf("Tests Run: %d | Failures: %d | Ignored: %d\n",
           Unity.NumberOfTests, Unity.TestFailures, Unity.TestIgnores);
    if (Unity.TestFailures == 0) {
        printf("RESULT: ALL TESTS PASSED (OK)\n");
    } else {
        printf("RESULT: %d TEST(S) FAILED (FAIL)\n", Unity.TestFailures);
    }
    printf("======================================================================\n");
    return Unity.TestFailures;
}

void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum) {
    Unity.NumberOfTests++;
    Unity.CurrentTestName = FuncName;
    Unity.CurrentTestLineNumber = FuncLineNum;
    Unity.CurrentTestFailed = 0;

    printf("  [RUN ] %s", FuncName);
    fflush(stdout);

    Func();

    if (Unity.CurrentTestFailed == 0) {
        printf("\r  [PASS] %s\n", FuncName);
    } else {
        printf("\r  [FAIL] %s\n", FuncName);
    }
    fflush(stdout);
}

void UnityAssertTrue(bool condition, int line, const char* msg) {
    if (!condition) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: %s", Unity.TestFile, line, msg ? msg : "");
    }
}

void UnityAssertNull(const void* ptr, int line, const char* msg) {
    if (ptr != NULL) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected NULL, got %p. %s", Unity.TestFile, line, ptr, msg ? msg : "");
    }
}

void UnityAssertNotNull(const void* ptr, int line, const char* msg) {
    if (ptr == NULL) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected Non-NULL, got NULL. %s", Unity.TestFile, line, msg ? msg : "");
    }
}

void UnityAssertEqualInt(int expected, int actual, int line, const char* msg) {
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected %d, got %d. %s", Unity.TestFile, line, expected, actual, msg ? msg : "");
    }
}

void UnityAssertEqualUInt(unsigned int expected, unsigned int actual, int line, const char* msg) {
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected %u, got %u. %s", Unity.TestFile, line, expected, actual, msg ? msg : "");
    }
}

void UnityAssertEqualHex8(uint8_t expected, uint8_t actual, int line, const char* msg) {
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected 0x%02X, got 0x%02X. %s", Unity.TestFile, line, expected, actual, msg ? msg : "");
    }
}

void UnityAssertEqualHex16(uint16_t expected, uint16_t actual, int line, const char* msg) {
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected 0x%04X, got 0x%04X. %s", Unity.TestFile, line, expected, actual, msg ? msg : "");
    }
}

void UnityAssertEqualHex32(uint32_t expected, uint32_t actual, int line, const char* msg) {
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected 0x%08X, got 0x%08X. %s", Unity.TestFile, line, expected, actual, msg ? msg : "");
    }
}

void UnityAssertEqualString(const char* expected, const char* actual, int line, const char* msg) {
    if (!expected && !actual) return;
    if (!expected || !actual || strcmp(expected, actual) != 0) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Expected \"%s\", got \"%s\". %s",
               Unity.TestFile, line, expected ? expected : "(null)", actual ? actual : "(null)", msg ? msg : "");
    }
}

void UnityAssertEqualMemory(const void* expected, const void* actual, size_t length, int line, const char* msg) {
    if (!expected && !actual) return;
    if (!expected || !actual || memcmp(expected, actual, length) != 0) {
        Unity.CurrentTestFailed = 1;
        Unity.TestFailures++;
        printf("\n    -> %s:%d: FAIL: Memory buffers of length %zu mismatch. %s", Unity.TestFile, line, length, msg ? msg : "");
    }
}

void UnityAssertFail(int line, const char* msg) {
    Unity.CurrentTestFailed = 1;
    Unity.TestFailures++;
    printf("\n    -> %s:%d: FAIL: %s", Unity.TestFile, line, msg ? msg : "");
}
