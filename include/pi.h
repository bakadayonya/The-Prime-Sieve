// ============================================================================
//  pi.h — Meissel–Lehmer 素数计数与第 n 个素数
// ============================================================================

#ifndef PRIME_SIEVE_PI_H
#define PRIME_SIEVE_PI_H

#include <cstdint>

// π(x) 主入口（Lehmer 公式，记忆化）。
long long lehmer_pi(uint64_t x);

// 查找第 n 个素数（二分 + Lehmer π(x)）。
long long find_nth_prime(long long n);

#endif // PRIME_SIEVE_PI_H
