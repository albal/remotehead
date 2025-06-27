#ifndef SIMPLE_TEST_H
#define SIMPLE_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Simple test framework macros
#define TEST_ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_FALSE(condition) \
    TEST_ASSERT_TRUE(!(condition))

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: %s:%d - Expected %d, got %d\n", __FILE__, __LINE__, (expected), (actual)); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAIL: %s:%d - Expected '%s', got '%s'\n", __FILE__, __LINE__, (expected), (actual)); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT_TRUE((ptr) != NULL)

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT_TRUE((ptr) == NULL)

// Test function type
typedef int (*test_func_t)(void);

// Test registration structure
typedef struct {
    const char* name;
    test_func_t func;
} test_case_t;

// Global test registration
extern test_case_t g_tests[];
extern int g_test_count;

// Test registration macro
#define REGISTER_TEST(test_name) \
    int test_name(void); \
    static void __attribute__((constructor)) register_##test_name(void) { \
        g_tests[g_test_count].name = #test_name; \
        g_tests[g_test_count].func = test_name; \
        g_test_count++; \
    }

#define MAX_TESTS 100

#endif // SIMPLE_TEST_H