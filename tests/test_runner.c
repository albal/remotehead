#include "include/simple_test.h"

// Global test storage
test_case_t g_tests[MAX_TESTS];
int g_test_count = 0;

int main(void) {
    printf("Running %d tests...\n\n", g_test_count);
    
    int passed = 0;
    int failed = 0;
    
    for (int i = 0; i < g_test_count; i++) {
        printf("Running %s... ", g_tests[i].name);
        if (g_tests[i].func()) {
            printf("PASS\n");
            passed++;
        } else {
            failed++;
        }
    }
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n", passed + failed);
    
    if (failed > 0) {
        printf("\nSome tests failed!\n");
        return 1;
    } else {
        printf("\nAll tests passed!\n");
        return 0;
    }
}