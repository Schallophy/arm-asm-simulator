#!/bin/bash

set -e

cd "$(dirname "$0")/.."
SIMULATOR="./build/arm_asm_simulator"
PASSED=0
FAILED=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

run_test() {
    local test_name="$1"
    local test_file="$2"
    shift 2
    local expected=("$@")
    
    printf "%-55s " "$test_name"
    
    local output
    if ! output=$($SIMULATOR --batch "$test_file" 2>/dev/null); then
        local stderr
        stderr=$($SIMULATOR --batch "$test_file" 2>&1 1>/dev/null || true)
        echo -e "${RED}FAILED${NC} (compile error)"
        echo "  $stderr"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    local last_line
    last_line=$(echo "$output" | tail -1)
    
    if [[ -z "$last_line" ]]; then
        echo -e "${RED}FAILED${NC} (no output)"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    local all_match=true
    local failed_details=""
    
    for check in "${expected[@]}"; do
        local key="${check%%=*}"
        local expected_val="${check#*=}"
        
        # Extract value with word boundary to avoid matching substrings
        local actual_val
        actual_val=$(echo " $last_line " | grep -oE " ${key}=-?[0-9]+ " | head -1 | tr -d ' ' | cut -d= -f2)
        
        if [ -z "$actual_val" ]; then
            all_match=false
            failed_details="$failed_details\n  $key: expected=$expected_val actual=<missing>"
        elif [ "$actual_val" != "$expected_val" ]; then
            all_match=false
            failed_details="$failed_details\n  $key: expected=$expected_val actual=$actual_val"
        fi
    done
    
    if $all_match; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}$failed_details"
        echo "  Output: $last_line"
        FAILED=$((FAILED + 1))
    fi
}

echo "=========================================="
echo "ARM Assembly Simulator Test Suite"
echo "=========================================="
echo ""

# Data Processing
run_test "MOV/MVN" test/mov_mvn.s "R0=42" "R1=-43"
run_test "Immediate values" test/immediate.s "R0=255" "R1=65535" "R2=1048576"
run_test "ADD/SUB" test/add_sub.s "R0=10" "R1=20" "R2=30"
run_test "RSB" test/rsb.s "R0=-10" "R2=10"
run_test "ADC" test/adc.s "R2=101"
run_test "SBC" test/sbc.s "R4=-1"
run_test "RSC" test/rsc.s "R0=-1"
run_test "AND/ORR/EOR/BIC" test/logical.s "R0=8" "R1=10" "R2=2" "R3=0"
run_test "MUL/MLA" test/mul_mla.s "R2=60" "R6=110"

# Comparison & Test
run_test "CMP flags" test/cmp.s "N=0" "Z=1" "C=1" "V=0"
run_test "CMN flags" test/cmn.s "N=1" "Z=0"
run_test "TST" test/tst.s "R0=5" "N=0" "Z=1"
run_test "TEQ" test/teq.s "R0=5" "N=0" "Z=0"
run_test "S flag (SUBS)" test/s_flag.s "R0=0" "N=0" "Z=1" "C=1"

# Conditional execution
run_test "Conditional EQ/NE" test/cond_eq_ne.s "R0=1" "R1=0"
run_test "Conditional GT/LT" test/cond_gt_lt.s "R0=1" "R1=0"
run_test "Conditional GE/LE" test/cond_ge_le.s "R0=1" "R1=1"
run_test "Conditional CS/CC" test/cond_cs_cc.s "R0=1" "R1=0"
run_test "Conditional MI/PL" test/cond_mi_pl.s "R0=1" "R1=0"
run_test "Conditional VS/VC" test/cond_vs_vc.s "R0=1" "R1=0"

# Branches
run_test "B unconditional" test/b_uncond.s "R0=42"
run_test "BL with return" test/bl_return.s "R0=10" "R1=20"
run_test "BX LR" test/bx_lr.s "R0=42"
run_test "Conditional BEQ/BNE" test/b_eq_ne.s "R0=1" "R1=0"

# Memory - LDR/STR
run_test "LDR/STR basic with stack" test/ldr_str.s "R0=305419896" "SP=0"
run_test "LDR/STR with offset" test/ldr_str_offset.s "R0=100" "R1=200" "R2=300"
run_test "LDR/STR register offset" test/ldr_str_reg_offset.s "R2=0"
run_test "LDR/STR pre-indexed" test/ldr_str_preindexed.s "R2=42"
run_test "LDR/STR post-indexed" test/ldr_str_postindexed.s "R2=42"
run_test "LDR =constant" test/ldr_const.s "R0=305419896"
run_test "LDR =label" test/ldr_label.s "R0=42"

# LDM/STM
run_test "LDMIA/STMIA" test/ldm_stm_ia.s "R4=1" "R5=2" "R6=3"
run_test "LDMDB/STMDB" test/ldm_stm_db.s "R4=1" "R5=2" "R6=3"
run_test "LDMFD/STMFD aliases" test/ldm_stm_fd.s "R4=10" "R5=20"

# PUSH/POP
run_test "PUSH/POP single" test/push_pop_single.s "R0=0" "R1=42"
run_test "PUSH/POP multiple" test/push_pop_multiple.s "R2=10" "R3=20" "R4=30"
run_test "PUSH LR / POP PC" test/push_lr_pop_pc.s "R0=100"

# Complex scenarios
run_test "Nested function calls" test/nested_calls.s "R0=3"
run_test "Recursive factorial(5)" test/recursive.s "R0=120"
run_test "Loop sum 1-10" test/loop_counter.s "R0=55" "R1=0"

# 32-bit & overflow
run_test "32-bit masking" test/masking.s "R0=-1" "R1=-2147483648" "R2=-2147483648"
run_test "Sign extension" test/sign_ext.s "R0=-1" "R1=-1"
run_test "Overflow/carry flags" test/overflow_carry.s "R0=-2147483648" "N=1" "Z=0" "C=0" "V=1"
run_test "Conditional HI/LS" test/cond_hi_ls.s "R2=1" "R3=0" "R4=0" "R5=1"
run_test "Comprehensive flag checks" test/flags_comprehensive.s "R2=0" "R3=-2147483648" "R4=-5" "R5=5"

echo ""
echo "=========================================="
echo -e "Tests: $((PASSED + FAILED)) | ${GREEN}Passed: $PASSED${NC} | ${RED}Failed: $FAILED${NC}"
echo "=========================================="

if [[ $FAILED -eq 0 ]]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    exit 1
fi
