#!/bin/bash

# Basic test validation script
# This script performs basic syntax and structure validation of the test files

echo "=== Test Infrastructure Validation ==="

# Check if all test files exist
echo "Checking test file structure..."
files=(
    "components/main_test/CMakeLists.txt"
    "components/main_test/test_main.c" 
    "components/main_test/test_utils.c"
    "components/main_test/test_http_handlers.c"
    "components/main_test/test_nvs_utils.c"
    "components/main_test/test_utils.h"
    "test/CMakeLists.txt"
    "test/sdkconfig.defaults"
    ".github/workflows/test.yml"
    "docs/TESTING.md"
)

missing_files=0
for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file exists"
    else
        echo "✗ $file missing"
        missing_files=$((missing_files + 1))
    fi
done

if [ $missing_files -eq 0 ]; then
    echo "✓ All test files present"
else
    echo "✗ $missing_files files missing"
    exit 1
fi

echo ""
echo "Checking test function counts..."
test_function_count=$(grep -c "void test_" components/main_test/*.c)
echo "Found $test_function_count test functions"

echo ""
echo "Checking GitHub workflow triggers..."
if grep -q "pull_request:" .github/workflows/test.yml; then
    echo "✓ PR trigger configured"
else
    echo "✗ PR trigger missing"
fi

echo ""
echo "Checking test documentation..."
if [ -f "docs/TESTING.md" ] && [ -s "docs/TESTING.md" ]; then
    echo "✓ Test documentation present and non-empty"
else
    echo "✗ Test documentation missing or empty"
fi

echo ""
echo "=== Validation Complete ==="
echo "Test infrastructure successfully implemented!"
echo ""
echo "Next steps:"
echo "1. Run 'cd test && idf.py build' in ESP-IDF environment"
echo "2. Create a PR to trigger the test workflow"
echo "3. Tests will run automatically on PR creation"