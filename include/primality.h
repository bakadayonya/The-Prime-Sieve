// ============================================================================
//  primality.h — Miller-Rabin 素性测试
// ============================================================================

#ifndef PRIME_SIEVE_PRIMALITY_H
#define PRIME_SIEVE_PRIMALITY_H

#include <cstdint>

// 对任意 uint64 做确定性 Miller-Rabin 测试（先除小素数）。
bool is_prime_mr(uint64_t n);

// Miller-Rabin 核心（跳过小素数试除，供混合筛批量调用）。
bool is_prime_mr_fast(uint64_t n);

#endif // PRIME_SIEVE_PRIMALITY_H
