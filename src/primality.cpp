// ============================================================================
//  primality.cpp — Miller-Rabin 确定性素性测试
// ============================================================================

#include "primality.h"

#include <cstdint>

// 可调参数：确定性 Miller-Rabin 基数集
const int MR_BASE_COUNT_MAX = 12;
const uint64_t MR_BASES_ALL[MR_BASE_COUNT_MAX] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};

const int MR_BASE_COUNT_SMALL = 6;
// 前 6 个素基数对 n < 3,474,749,660,383 确定性充分（原实现用 7 个，偏保守）。
const uint64_t MR_BASES_SMALL[MR_BASE_COUNT_SMALL] = {2, 3, 5, 7, 11, 13};

// 模幂：base^exp mod mod（用 __int128 避免 64 位乘法溢出）
static inline uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t res = 1;
    base %= mod;
    while (exp) {
        if (exp & 1) res = (uint64_t)((__int128)res * base % mod);
        exp >>= 1;
        base = (uint64_t)((__int128)base * base % mod);
    }
    return res;
}

bool is_prime_mr_fast(uint64_t n) {
    uint64_t d = n - 1;
    int s = 0;
    while ((d & 1) == 0) {
        d >>= 1;
        ++s;
    }
    int base_count;
    const uint64_t* bases;
    if (n < 3474749660383ULL) {
        base_count = MR_BASE_COUNT_SMALL;
        bases = MR_BASES_SMALL;
    } else {
        base_count = MR_BASE_COUNT_MAX;
        bases = MR_BASES_ALL;
    }
    for (int i = 0; i < base_count; ++i) {
        uint64_t a = bases[i];
        if (a >= n) continue;
        uint64_t x = mod_pow(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int r = 1; r < s; ++r) {
            x = (uint64_t)((__int128)x * x % n);
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

bool is_prime_mr(uint64_t n) {
    if (n < 2) return false;
    static const uint64_t small_primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    for (uint64_t p : small_primes) {
        if (n % p == 0) return n == p;
    }
    return is_prime_mr_fast(n);
}
