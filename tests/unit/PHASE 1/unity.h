#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include <stddef.h>
#include <stdint.h>

void setUp(void);
void tearDown(void);

void UnityBegin(const char* filename);
int UnityEnd(void);
void UnityDefaultTestRun(void (*Func)(void), const char* FuncName, const int FuncLineNum);
void UnityAssertEqualNumber(const int32_t expected, const int32_t actual, const char* msg, const uint32_t line);

// Map the test macros directly to our engine functions
#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END()   UnityEnd()
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#define TEST_ASSERT_EQUAL(expected, actual) UnityAssertEqualNumber((int32_t)(expected), (int32_t)(actual), NULL, __LINE__)

#endif
