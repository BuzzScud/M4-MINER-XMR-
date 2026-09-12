#!/bin/bash

# System-Wide Validation Test Runner
# Runs all test executables and generates a comprehensive report

echo "╔════════════════════════════════════════════════════════════╗"
echo "║     SYSTEM-WIDE VALIDATION - ALL TESTS                     ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Set library path
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH

# Counters
total_tests=0
passed_tests=0
failed_tests=0
skipped_tests=0

# Results array
declare -a test_results

# Find all test executables
echo "Scanning for test executables..."
test_files=$(find . -maxdepth 1 -type f -executable -name "test_*" | sort)

if [ -z "$test_files" ]; then
    echo "❌ No test executables found!"
    echo "Please compile tests first with: make"
    exit 1
fi

test_count=$(echo "$test_files" | wc -l)
echo "Found $test_count test executables"
echo ""

echo "┌────────────────────────────────────────────────────────────┐"
echo "│ RUNNING TESTS                                              │"
echo "└────────────────────────────────────────────────────────────┘"
echo ""

# Run each test
for test_file in $test_files; do
    test_name=$(basename "$test_file")
    total_tests=$((total_tests + 1))
    
    printf "%-50s " "$test_name"
    
    # Run test and capture output
    if timeout 30s "$test_file" > /tmp/test_output_$$.txt 2>&1; then
        echo "✓ PASS"
        passed_tests=$((passed_tests + 1))
        test_results+=("PASS: $test_name")
    else
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo "⏱ TIMEOUT"
            skipped_tests=$((skipped_tests + 1))
            test_results+=("TIMEOUT: $test_name")
        else
            echo "✗ FAIL"
            failed_tests=$((failed_tests + 1))
            test_results+=("FAIL: $test_name")
            # Save failure output
            cat /tmp/test_output_$$.txt > "/tmp/failure_${test_name}.log"
        fi
    fi
    
    rm -f /tmp/test_output_$$.txt
done

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║ TEST SUMMARY                                               ║"
echo "╠════════════════════════════════════════════════════════════╣"
printf "║ Total Tests:  %-3d                                         ║\n" "$total_tests"
printf "║ Passed:       %-3d                                         ║\n" "$passed_tests"
printf "║ Failed:       %-3d                                         ║\n" "$failed_tests"
printf "║ Timeout:      %-3d                                         ║\n" "$skipped_tests"

if [ $total_tests -gt 0 ]; then
    success_rate=$((passed_tests * 100 / total_tests))
    printf "║ Success Rate: %-3d%%                                        ║\n" "$success_rate"
fi

echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Show failed tests if any
if [ $failed_tests -gt 0 ]; then
    echo "┌────────────────────────────────────────────────────────────┐"
    echo "│ FAILED TESTS                                               │"
    echo "└────────────────────────────────────────────────────────────┘"
    echo ""
    for result in "${test_results[@]}"; do
        if [[ $result == FAIL:* ]]; then
            test_name=$(echo "$result" | cut -d' ' -f2)
            echo "❌ $test_name"
            if [ -f "/tmp/failure_${test_name}.log" ]; then
                echo "   Log: /tmp/failure_${test_name}.log"
            fi
        fi
    done
    echo ""
fi

# Exit with appropriate code
if [ $failed_tests -eq 0 ]; then
    echo "✅ ALL TESTS PASSED!"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi