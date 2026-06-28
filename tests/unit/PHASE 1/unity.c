#include "unity.h"
#include <stdio.h>

// Global structures to track test results
static struct {
    uint32_t NumberOfTests;
    uint32_t TestFailures;
} Unity;

// Private string container to hold the active test name
static const char* CurrentTestName = NULL;

void UnityBegin(const char* filename) {
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    printf("Unity Test Automation Started for: %s\n", filename);
    printf("----------------------------------------\n");
}

int UnityEnd(void) {
    printf("----------------------------------------\n");
    printf("%d Tests Executed, %d Failures Registered.\n", Unity.NumberOfTests, Unity.TestFailures);
    if (Unity.TestFailures == 0) {
        printf("ALL TESTS PASSED (OK)\n");
        return 0;
    } else {
        printf("TEST SUITE FAILED\n");
        return 1;
    }
}

void UnityDefaultTestRun(void (*Func)(void), const char* FuncName, const int FuncLineNum) {
    Unity.NumberOfTests++;
    CurrentTestName = FuncName;
    
    // Execute setUp logic before the test, run the test, then clean up
    setUp();
    Func();
    tearDown();
}

void UnityAssertEqualNumber(const int32_t expected, const int32_t actual, const char* msg, const uint32_t line) {
    if (expected != actual) {
        Unity.TestFailures++;
        printf("  [FAIL] %s (Line %d): Expected %d, but got %d\n", CurrentTestName, line, expected, actual);
    } else {
        printf("  [PASS] %s\n", CurrentTestName);
    }
}
