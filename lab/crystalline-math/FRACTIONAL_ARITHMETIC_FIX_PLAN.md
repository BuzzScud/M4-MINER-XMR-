# Fractional Arithmetic Fix - Implementation Plan

## Problem Statement

Current arithmetic operations (add, sub, mul, div) operate on beads by **array index**, but fractional numbers have beads at different **exponents**.

### Example of the Bug:
```
Number 1.5 in base 10:
  beads[0]: value=5, weight_exponent=-1  (0.5)
  beads[1]: value=1, weight_exponent=0   (1.0)

Number 2.25 in base 10:
  beads[0]: value=5, weight_exponent=-2  (0.05)
  beads[1]: value=2, weight_exponent=-1  (0.2)
  beads[2]: value=2, weight_exponent=0   (2.0)

Current addition (WRONG):
  beads[0] + beads[0] = 5 + 5 = 10 (but these are different exponents!)
  beads[1] + beads[1] = 1 + 2 = 3
  beads[2] + beads[2] = 0 + 2 = 2
  Result: INCORRECT

Correct addition (by exponent):
  exp=-2: 5 (from 2.25)
  exp=-1: 5 + 2 = 7
  exp=0:  1 + 2 = 3
  Result: 3.75 ✓
```

## Solution Architecture

### 1. Helper Function: Find Bead by Exponent
```c
/**
 * Find bead with specific exponent in abacus
 * Returns pointer to bead, or NULL if not found
 */
static const AbacusBead* find_bead_by_exponent(
    const CrystallineAbacus* abacus,
    int32_t exponent
);
```

### 2. Helper Function: Get Exponent Range
```c
/**
 * Get the range of exponents in one or two abacuses
 */
static void get_exponent_range(
    const CrystallineAbacus* a,
    const CrystallineAbacus* b,  // can be NULL
    int32_t* min_exp,
    int32_t* max_exp
);
```

### 3. Updated Addition Algorithm
```
For each exponent from min_exp to max_exp:
  1. Find bead in operand A at this exponent (or 0 if not found)
  2. Find bead in operand B at this exponent (or 0 if not found)
  3. Add values + carry
  4. Store result at this exponent
  5. Propagate carry to next exponent
```

### 4. Updated Subtraction Algorithm
```
For each exponent from min_exp to max_exp:
  1. Find bead in operand A at this exponent (or 0 if not found)
  2. Find bead in operand B at this exponent (or 0 if not found)
  3. Subtract with borrow
  4. Store result at this exponent
  5. Propagate borrow to next exponent
```

### 5. Updated Multiplication Algorithm
```
For each bead in A (at exponent exp_a):
  For each bead in B (at exponent exp_b):
    product = value_a * value_b
    result_exponent = exp_a + exp_b
    Accumulate product at result_exponent
```

### 6. Updated Division Algorithm
```
Long division with exponent tracking:
  - Quotient exponent = dividend_max_exp - divisor_max_exp
  - Process from highest to lowest exponent
  - Track remainder exponents
```

## Implementation Steps

### Step 1: Add Helper Functions ✓
- [x] `find_bead_by_exponent()`
- [x] `get_exponent_range()`

### Step 2: Fix Addition ✓
- [x] Rewrite to operate by exponent, not by index
- [x] Handle carry propagation correctly
- [x] Update min_exponent tracking

### Step 3: Fix Subtraction ✓
- [x] Rewrite to operate by exponent, not by index
- [x] Handle borrow propagation correctly
- [x] Update min_exponent tracking

### Step 4: Fix Multiplication ✓
- [x] Rewrite to accumulate at correct exponents
- [x] Handle exponent arithmetic (exp_a + exp_b)
- [x] Update min_exponent tracking

### Step 5: Fix Division ✓
- [x] Rewrite to handle exponent arithmetic
- [x] Track quotient and remainder exponents
- [x] Update min_exponent tracking

### Step 6: Testing ✓
- [x] Create comprehensive fractional arithmetic tests
- [x] Test mixed precision operations
- [x] Test edge cases (zero, negative, large differences)

## Expected Outcomes

1. **Correctness**: All fractional arithmetic operations produce correct results
2. **Precision**: Operations maintain arbitrary precision
3. **Flexibility**: Can mix numbers with different fractional precisions
4. **Performance**: O(n) for add/sub, O(n²) for mul/div (same as before)

## Testing Strategy

### Test Cases:
1. Simple fractional addition: 1.5 + 2.25 = 3.75
2. Different precisions: 1.5 + 2.125 = 3.625
3. Carry across exponents: 0.9 + 0.2 = 1.1
4. Subtraction with borrow: 1.0 - 0.1 = 0.9
5. Multiplication: 1.5 * 2.0 = 3.0
6. Division: 3.0 / 2.0 = 1.5
7. Negative exponents: 0.01 + 0.001 = 0.011
8. Large exponent differences: 1000.0 + 0.001 = 1000.001

## Timeline

- **Helper Functions**: 2 hours
- **Addition Fix**: 3 hours
- **Subtraction Fix**: 2 hours
- **Multiplication Fix**: 4 hours
- **Division Fix**: 4 hours
- **Testing**: 4 hours
- **Documentation**: 2 hours

**Total**: ~21 hours (~2-3 days)

## Success Criteria

✓ All existing integer tests pass (21/21)
✓ All new fractional tests pass
✓ Zero warnings, zero errors
✓ No math.h dependencies
✓ Clean, maintainable code