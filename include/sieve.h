// ============================================================================
//  sieve.h — 素数筛核心（简单奇数筛 / 模30轮子分段筛 / 混合筛 / 并行大区间筛）
// ============================================================================

#ifndef PRIME_SIEVE_SIEVE_H
#define PRIME_SIEVE_SIEVE_H

#include <cstddef>
#include <cstdint>
#include <vector>

// 生成/扩充基础素数表（供分段筛与计数使用）。
void ensure_base_primes(long long limit);

// 当前基础素数表（const 引用，避免暴露全局可变状态）。
const std::vector<long long>& base_primes();

// 非并行分段筛（小范围使用）。
std::vector<long long> segmented_sieve(long long low, long long high,
                          const std::vector<long long>& base_primes);

// 分段筛，追加到 out_primes。
void segmented_sieve_append(long long low, long long high,
                            const std::vector<long long>& base_primes,
                            std::vector<long long>& out_primes);

// 计数 [low, high] 内素数个数（不构造结果向量，省内存）。
size_t count_primes_range(long long low, long long high,
                          const std::vector<long long>& base_primes);

// 并行大区间分段筛（k 路归并保证全局有序）。
std::vector<long long> segmented_sieve_large(long long low, long long high);

// 混合筛：小素数预筛 + 并行 Miller-Rabin 验证（稀疏大数区间）。
std::vector<long long> hybrid_sieve(long long low, long long high);

// 素数计数函数 π(x)，分段筛计数版（供 -v 自检与 lehmer_pi 交叉验证）。
long long prime_count(long long x);

#endif // PRIME_SIEVE_SIEVE_H
