# Phase 7.6: Fractional Bead Support - Implementation Plan

## Mission
Extend the Crystalline Abacus to support real numbers with arbitrary precision by implementing fractional beads using negative weight exponents.

---

## Structural Changes

### 1. AbacusBead Structure - UPDATED ✅
**Before:**
```c
typedef struct {
    ClockPosition position;
    uint32_t value;
    uint32_t weight;  // Only positive weights (base^0, base^1, base^2, ...)
} AbacusBead;
```

**After:**
```c
typedef struct {
    ClockPosition position;
    uint32_t value;
    int32_t weight_exponent;  // Can be negative! (base^-1, base^-2, ...)
} AbacusBead;
```

**Impact:** All existing code that uses `weight` must be updated to calculate weight from `weight_exponent`

### 2. CrystallineAbacus Structure - UPDATED ✅
**Before:**
```c
typedef struct {
    AbacusBead* beads;
    size_t num_beads;
    size_t capacity;
    uint32_t base;
    bool negative;
} CrystallineAbacus;
```

**After:**
```c
typedef struct {
    AbacusBead* beads;
    size_t num_beads;
    size_t capacity;
    uint32_t base;
    bool negative;
    int32_t min_exponent;  // NEW: Tracks fractional precision
} CrystallineAbacus;
```

---

## Implementation Strategy

### Phase 1: Update Existing Functions (CRITICAL)
All existing functions must be updated to work with `weight_exponent` instead of `weight`.

**Functions to Update:**
1. `abacus_new()` - Initialize `min_exponent = 0`
2. `abacus_init_zero()` - Set `min_exponent = 0`
3. `abacus_from_uint64()` - Use `weight_exponent` instead of `weight`
4. `abacus_to_uint64()` - Calculate weight from `weight_exponent`
5. `multiply_by_digit()` - Use `weight_exponent`
6. `subtract_magnitude()` - Use `weight_exponent`
7. `abacus_add()` - Align by `weight_exponent`
8. `abacus_sub()` - Align by `weight_exponent`
9. `abacus_mul()` - Handle `weight_exponent` addition
10. `abacus_div()` - Handle `weight_exponent` subtraction
11. `abacus_shift_left()` - Increment `weight_exponent`
12. `abacus_shift_right()` - Decrement `weight_exponent`
13. `abacus_normalize()` - Handle fractional beads
14. `compare_magnitude()` - Compare by `weight_exponent`
15. `find_quotient_digit()` - Use `weight_exponent`

**Key Pattern:**
```c
// OLD: Direct weight access
result->beads[i].weight = (i == 0) ? 1 : result->beads[i-1].weight * result->base;

// NEW: Use weight_exponent
result->beads[i].weight_exponent = i;  // For integer part
// or
result->beads[i].weight_exponent = -i;  // For fractional part
```

### Phase 2: Implement New Fractional Functions
1. `abacus_from_double()` - Convert double to abacus with fractional beads
2. `abacus_to_double()` - Convert abacus to double
3. `abacus_set_precision()` - Adjust fractional precision
4. `abacus_get_precision()` - Query fractional precision
5. `abacus_round()` - Round to specified precision
6. `abacus_truncate()` - Truncate to specified precision

### Phase 3: Update Tests
1. Update existing tests to work with new structure
2. Add new tests for fractional operations
3. Test precision handling
4. Test rounding and truncation

---

## Implementation Details

### Weight Calculation Helper
```c
/**
 * @brief Calculate weight from exponent
 * @param base Number base
 * @param exponent Weight exponent
 * @return weight = base^exponent
 */
static double calculate_weight(uint32_t base, int32_t exponent) {
    if (exponent == 0) return 1.0;
    if (exponent > 0) {
        double result = 1.0;
        for (int32_t i = 0; i < exponent; i++) {
            result *= base;
        }
        return result;
    } else {
        double result = 1.0;
        for (int32_t i = 0; i < -exponent; i++) {
            result /= base;
        }
        return result;
    }
}
```

### Bead Alignment by Exponent
```c
/**
 * @brief Align two abacuses by weight exponent
 * Ensures both have beads for all exponents from min to max
 */
static MathError align_by_exponent(CrystallineAbacus* a, CrystallineAbacus* b) {
    int32_t min_exp = (a->min_exponent < b->min_exponent) ? a->min_exponent : b->min_exponent;
    int32_t max_exp_a = (a->num_beads > 0) ? a->beads[a->num_beads-1].weight_exponent : 0;
    int32_t max_exp_b = (b->num_beads > 0) ? b->beads[b->num_beads-1].weight_exponent : 0;
    int32_t max_exp = (max_exp_a > max_exp_b) ? max_exp_a : max_exp_b;
    
    // Pad with zeros as needed
    // ...
}
```

### Example: 157.25 in Base 12
```
Integer part: 157 = 13*12 + 1
Fractional part: 0.25 = 3/12

Beads:
  beads[0]: value=3,  weight_exponent=-1  (3 * 12^-1 = 0.25)
  beads[1]: value=1,  weight_exponent=0   (1 * 12^0 = 1)
  beads[2]: value=13, weight_exponent=1   (13 * 12^1 = 156)

Total: 156 + 1 + 0.25 = 157.25 ✓
min_exponent: -1
```

---

## Backward Compatibility

### Integer-Only Operations
For integer-only operations (existing behavior):
- `min_exponent = 0`
- All `weight_exponent >= 0`
- No fractional beads

### Mixed Operations
When mixing integer and fractional abacuses:
- Align by exponent
- Pad with zeros as needed
- Result has precision of most precise operand

---

## Testing Strategy

### Phase 1: Update Existing Tests
```c
// Existing tests should still pass with integer-only operations
test_add_simple() {
    // 100 + 50 = 150 (integers only)
    // Should work exactly as before
}
```

### Phase 2: New Fractional Tests
```c
test_from_double() {
    // 157.25 in base 12
    CrystallineAbacus* a = abacus_from_double(157.25, 12, 2);
    // Verify beads are correct
}

test_add_fractional() {
    // 1.5 + 2.25 = 3.75
    CrystallineAbacus* a = abacus_from_double(1.5, 12, 2);
    CrystallineAbacus* b = abacus_from_double(2.25, 12, 2);
    CrystallineAbacus* result = abacus_new(12);
    abacus_add(result, a, b);
    
    double value;
    abacus_to_double(result, &value);
    assert(fabs(value - 3.75) < 0.001);
}

test_mul_fractional() {
    // 1.5 * 2.0 = 3.0
    // 2.5 * 4.0 = 10.0
}

test_div_fractional() {
    // 3.0 / 2.0 = 1.5
    // 10.0 / 4.0 = 2.5
}

test_precision() {
    // Set precision to 3 fractional digits
    // Verify padding/truncation
}

test_rounding() {
    // 1.567 rounded to 2 digits = 1.57
    // 1.564 rounded to 2 digits = 1.56
}
```

---

## Timeline

### Week 1: Core Implementation
**Days 1-2:** Update existing functions
- Update all functions to use `weight_exponent`
- Fix compilation errors
- Ensure existing tests pass

**Days 3-4:** Implement fractional functions
- `abacus_from_double()`
- `abacus_to_double()`
- `abacus_set_precision()`
- `abacus_get_precision()`

**Days 5-7:** Implement rounding/truncation
- `abacus_round()`
- `abacus_truncate()`
- Test all new functions
- Write comprehensive tests

---

## Expected Challenges

### 1. Bead Ordering
**Challenge:** Beads must be ordered by `weight_exponent`
**Solution:** Always maintain sorted order, use binary search for insertion

### 2. Precision Management
**Challenge:** Different precisions in operations
**Solution:** Align to maximum precision, pad with zeros

### 3. Rounding
**Challenge:** Proper rounding with carries
**Solution:** Implement banker's rounding (round to even)

### 4. Performance
**Challenge:** Fractional operations may be slower
**Solution:** Optimize alignment, minimize reallocations

---

## Success Criteria

### Must Have
- [ ] All existing tests pass with updated structure
- [ ] `abacus_from_double()` works correctly
- [ ] `abacus_to_double()` works correctly
- [ ] Addition with fractional numbers works
- [ ] Subtraction with fractional numbers works
- [ ] Multiplication with fractional numbers works
- [ ] Division with fractional numbers works
- [ ] Precision management works
- [ ] Rounding works correctly
- [ ] Zero warnings, zero errors

### Nice to Have
- [ ] Optimized alignment algorithm
- [ ] Banker's rounding implementation
- [ ] Comprehensive test suite (50+ tests)
- [ ] Performance benchmarks
- [ ] Documentation with examples

---

## Master Plan Alignment

### OBJECTIVE 2E: Crystalline Math Everywhere (NO math.h)
✅ **Extends capability**
- Real number support with arbitrary precision
- No math.h dependency
- Pure geometric operations

### OBJECTIVE 25: Platonic Solid Model Architecture
✅ **Supports**
- Model weights often have fractional parts
- Arbitrary precision for gradients
- Efficient real number operations

---

## Next Steps

1. **Immediate:** Update existing functions to use `weight_exponent`
2. **Then:** Implement `abacus_from_double()` and `abacus_to_double()`
3. **Then:** Implement precision management
4. **Then:** Implement rounding and truncation
5. **Finally:** Comprehensive testing and documentation

---

**Status:** Ready to implement
**Estimated Time:** 1 week
**Priority:** 🔴 HIGH - Extends abacus to real numbers