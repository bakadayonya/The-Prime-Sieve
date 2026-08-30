// ============================================================================
//  actions.h — 各操作执行函数
// ============================================================================

#ifndef PRIME_SIEVE_ACTIONS_H
#define PRIME_SIEVE_ACTIONS_H

#include <cstdint>

void run_range(long long low, long long high);
void run_digits(int digits);
void run_check(uint64_t n);
void run_nth(long long n);
void run_performance();
// 内置正确性自检；返回失败数（0 = 全部通过）。
int run_verify();

#endif // PRIME_SIEVE_ACTIONS_H
