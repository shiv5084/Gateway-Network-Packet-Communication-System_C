#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include "unity_internals.h"

#ifdef __cplusplus
extern "C" {
#endif

void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

/* Standard Test Runner Macros */
#define TEST_PROTECT() (1)
#define TEST_ABORT()   return

#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

/* Core Assertions */
#define TEST_ASSERT(condition)                               UnityAssertTrue((condition), __LINE__, " Expression Evaluated To FALSE")
#define TEST_ASSERT_TRUE(condition)                          UnityAssertTrue((condition), __LINE__, " Expected TRUE Was FALSE")
#define TEST_ASSERT_FALSE(condition)                         UnityAssertTrue(!(condition), __LINE__, " Expected FALSE Was TRUE")
#define TEST_ASSERT_NULL(pointer)                            UnityAssertNull((const void*)(pointer), __LINE__, " Expected NULL Pointer")
#define TEST_ASSERT_NOT_NULL(pointer)                        UnityAssertNotNull((const void*)(pointer), __LINE__, " Expected Non-NULL Pointer")
#define TEST_ASSERT_EQUAL_INT(expected, actual)              UnityAssertEqualInt((int)(expected), (int)(actual), __LINE__, " Expected Values Equal")
#define TEST_ASSERT_EQUAL_UINT(expected, actual)             UnityAssertEqualUInt((unsigned int)(expected), (unsigned int)(actual), __LINE__, " Expected Values Equal")
#define TEST_ASSERT_EQUAL_HEX8(expected, actual)             UnityAssertEqualHex8((uint8_t)(expected), (uint8_t)(actual), __LINE__, " Expected Hex8 Equal")
#define TEST_ASSERT_EQUAL_HEX16(expected, actual)            UnityAssertEqualHex16((uint16_t)(expected), (uint16_t)(actual), __LINE__, " Expected Hex16 Equal")
#define TEST_ASSERT_EQUAL_HEX32(expected, actual)            UnityAssertEqualHex32((uint32_t)(expected), (uint32_t)(actual), __LINE__, " Expected Hex32 Equal")
#define TEST_ASSERT_EQUAL_STRING(expected, actual)           UnityAssertEqualString((expected), (actual), __LINE__, " Expected Strings Equal")
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, length)   UnityAssertEqualMemory((expected), (actual), (length), __LINE__, " Expected Memory Equal")
#define TEST_FAIL_MESSAGE(message)                           UnityAssertFail(__LINE__, (message))

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FRAMEWORK_H */
