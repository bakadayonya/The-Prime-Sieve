// ============================================================================
//  sieve.cpp — 素数筛核心
//  简单奇数筛（生成基础素数表）、模 30 轮子分段筛、混合筛、并行大区间筛。
// ============================================================================

#include "sieve.h"
#include "primality.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <omp.h>
#include <queue>
#include <vector>

// 可调参数（经实测调优）
const long long SEG_SIZE = 4 * 1024 * 1024;          // 每段覆盖区间宽度
const long long HYBRID_THRESHOLD = 10'000'000'000LL; // 混合筛触发阈值
const long long SMALL_PRIME_LIMIT_MIN = 2000;        // 预筛上限最小值
const long long SMALL_PRIME_LIMIT_MAX = 50000;       // 预筛上限最大值

// 基础素数表（本模块私有，经 ensure_base_primes / base_primes 访问）
static std::vector<long long> g_base_primes;

// ============================================================================
//  简单奇数筛（用于生成基础素数表）
// ============================================================================

static std::vector<long long> simple_sieve_odd(long long limit) {
    std::vector<long long> primes;
    if (limit < 2) return primes;
    primes.push_back(2);
    if (limit == 2) return primes;

    long long odd_cnt = (limit - 1) / 2;
    size_t words = (odd_cnt + 63) / 64;
    std::vector<uint64_t> bits(words, ~0ULL);
    long long sqrt_lim = (long long)sqrt(limit);

    for (long long i = 1; 2 * i + 1 <= sqrt_lim; ++i) {
        if (bits[i >> 6] & (1ULL << (i & 63))) {
            long long p = 2 * i + 1;
            long long start = p * p;
            long long start_idx = (start - 1) / 2;
            for (long long j = start_idx; j < odd_cnt; j += p) {
                bits[j >> 6] &= ~(1ULL << (j & 63));
            }
        }
    }

    for (long long i = 1; i <= odd_cnt; ++i) {
        if (bits[i >> 6] & (1ULL << (i & 63))) {
            primes.push_back(2 * i + 1);
        }
    }
    return primes;
}

void ensure_base_primes(long long limit) {
    static std::mutex expand_mutex;
    std::lock_guard<std::mutex> lock(expand_mutex);
    long long needed = (long long)sqrt((double)limit) + 1;
    if (g_base_primes.empty() || g_base_primes.back() * g_base_primes.back() < limit) {
        g_base_primes = simple_sieve_odd(needed);
    }
}

const std::vector<long long>& base_primes() {
    return g_base_primes;
}

// ============================================================================
//  模 30 轮子分段筛核心
//  只处理与 30 互质的数（8 个剩余类 {1,7,11,13,17,19,23,29}），因此 2、3、5
//  的倍数天然不在位图中，无需标记。位图仅 8/30 ≈ 26.7% 大小：每 64 位覆盖 240 个数。
// ============================================================================

static const uint8_t WHEEL_R[8] = {1, 7, 11, 13, 17, 19, 23, 29};
static uint8_t WHEEL_INV[30];          // 剩余类 -> mod 30 下的乘法逆元
static std::once_flag g_wheel_init_flag;

static void init_wheel() {
    for (int k = 0; k < 8; ++k) {
        for (int inv = 1; inv < 30; inv += 2)
            if ((WHEEL_R[k] * inv) % 30 == 1) { WHEEL_INV[WHEEL_R[k]] = (uint8_t)inv; break; }
    }
}

// 对 [low, high] 内与 30 互质的数做筛，结果写入 bits 位图（每块 8 位）。
// 返回块数；low 需已 clamp 到 >=7。
static long long mark_segment(long long low, long long high,
                              const std::vector<long long>& base_primes,
                              std::vector<uint64_t>& bits) {
    std::call_once(g_wheel_init_flag, init_wheel);
    long long seg_base_q = low / 30;
    long long num_blocks = high / 30 - seg_base_q + 1;
    size_t words = (num_blocks + 7) / 8;
    if (bits.size() < words) bits.resize(words);
    std::fill(bits.begin(), bits.begin() + words, ~0ULL);
    uint64_t* bp = bits.data();
    const long long total_bits = num_blocks * 8;

    for (long long p : base_primes) {
        if (p < 7) continue;
        __int128 p2 = (__int128)p * p;
        if (p2 > high) break;
        long long n_min = (p2 > low) ? (long long)p2 : low;
        int invp = WHEEL_INV[(int)(p % 30)];
        for (int k = 0; k < 8; ++k) {
            int r = WHEEL_R[k];
            // 倍数 n=p*m 与 r 同余时，m ≡ r·p⁻¹ (mod 30)，且 m 与 30 互质
            int mr = (int)((r * invp) % 30);
            long long m_need = (n_min + p - 1) / p;
            long long t0 = (mr < m_need) ? (m_need - mr + 29) / 30 : 0;
            long long n = p * (mr + 30LL * t0);
            if (n > high) continue;
            // n 每 +30p，块号 +p，故位图下标步长为 8p
            long long idx = 8 * (n / 30 - seg_base_q) + k;
            for (long long i = idx; i < total_bits; i += 8 * p)
                bp[i >> 6] &= ~(1ULL << (i & 63));
        }
    }
    return num_blocks;
}

// 扫描已标记位图，把 [low, high] 内每个素数逐个交给 emit(n)。
template <typename Emit>
static void scan_segment(long long low, long long high,
                         const std::vector<uint64_t>& bits,
                         Emit&& emit) {
    long long seg_base_q = low / 30;
    long long num_blocks = high / 30 - seg_base_q + 1;
    const uint64_t* bp = bits.data();
    for (long long q = 0; q < num_blocks; ++q) {
        long long nbase = 30 * (seg_base_q + q);
        for (int k = 0; k < 8; ++k) {
            long long n = nbase + WHEEL_R[k];
            if (n < low) continue;
            if (n > high) break;
            size_t idx = (size_t)(8 * q + k);
            if (bp[idx >> 6] & (1ULL << (idx & 63))) emit(n);
        }
    }
}

void segmented_sieve_append(long long low, long long high,
                            const std::vector<long long>& base_primes,
                            std::vector<long long>& out_primes) {
    if (low > high || high < 2) return;
    if (low < 2) low = 2;
    for (long long p : {2LL, 3LL, 5LL})
        if (low <= p && p <= high) out_primes.push_back(p);
    if (high < 7) return;
    if (low < 7) low = 7;

    thread_local std::vector<uint64_t> bits;
    mark_segment(low, high, base_primes, bits);
    scan_segment(low, high, bits, [&](long long n) { out_primes.push_back(n); });
}

size_t count_primes_range(long long low, long long high,
                          const std::vector<long long>& base_primes) {
    if (low > high || high < 2) return 0;
    if (low < 2) low = 2;
    size_t c = 0;
    for (long long p : {2LL, 3LL, 5LL})
        if (low <= p && p <= high) ++c;
    if (high < 7) return c;
    if (low < 7) low = 7;

    thread_local std::vector<uint64_t> bits;
    mark_segment(low, high, base_primes, bits);
    scan_segment(low, high, bits, [&](long long) { ++c; });
    return c;
}

std::vector<long long> segmented_sieve(long long low, long long high,
                                       const std::vector<long long>& base_primes) {
    std::vector<long long> result;
    segmented_sieve_append(low, high, base_primes, result);
    return result;
}

// ============================================================================
//  混合筛（小素数预筛 + Miller-Rabin 验证）
//  动态调整预筛素数范围。
// ============================================================================

std::vector<long long> hybrid_sieve(long long low, long long high) {
    std::vector<long long> primes;
    if (low <= 2 && high >= 2) primes.push_back(2);
    if (low <= 3 && high >= 3) primes.push_back(3);
    long long first_odd = (low % 2 == 0) ? low + 1 : low;
    if (first_odd > high) return primes;
    if (first_odd == 3) first_odd = 5; // 3 已经处理

    // 动态选择预筛素数上限：区间越大，预筛范围越大
    long long range_size = high - low + 1;
    long long small_prime_limit = SMALL_PRIME_LIMIT_MIN;
    if (range_size > 1'000'000'000LL) {
        small_prime_limit = SMALL_PRIME_LIMIT_MAX; // 大区间使用最大预筛
    } else if (range_size > 10'000'000LL) {
        small_prime_limit = 10000;
    } else {
        small_prime_limit = SMALL_PRIME_LIMIT_MIN;
    }

    // 生成预筛小素数（使用简单奇数筛）；2、3 已单独处理，移除
    std::vector<long long> small_primes = simple_sieve_odd(small_prime_limit);
    small_primes.erase(
        std::remove_if(small_primes.begin(), small_primes.end(),
                       [](long long p) { return p == 2 || p == 3; }),
        small_primes.end());

    long long count = (high - first_odd) / 2 + 1; // 奇数个数
    size_t words = (count + 63) / 64;
    std::vector<uint64_t> maybe_prime(words, ~0ULL);

    auto clear_bit = [&](long long idx) {
        maybe_prime[idx >> 6] &= ~(1ULL << (idx & 63));
    };
    auto is_set = [&](long long idx) -> bool {
        return (maybe_prime[idx >> 6] & (1ULL << (idx & 63))) != 0;
    };

    // 预筛
    for (long long p : small_primes) {
        long long first = ((first_odd + p - 1) / p) * p;
        if ((first & 1) == 0) first += p;
        long long idx = (first - first_odd) / 2;
        if (idx < 0) idx = 0;
        for (long long j = idx; j < count; j += p) {
            clear_bit(j);
        }
    }

    // 并行 Miller-Rabin 验证，直接收集素数，省去中间候选向量与 is_prime 位图。
    // 用 schedule(static)：线程各取一段连续、互不重叠的下标区间，因此各线程
    // 结果内部升序且线程 t 的下标整体小于线程 t+1，按线程号拼接即全局有序。
    int nthreads = omp_get_max_threads();
    std::vector<std::vector<long long>> thread_hits(nthreads);
#pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<long long> local;
#pragma omp for schedule(static)
        for (long long i = 0; i < count; ++i) {
            if (is_set(i)) {
                long long n = first_odd + 2 * i;
                if (is_prime_mr_fast((uint64_t)n)) local.push_back(n);
            }
        }
        thread_hits[tid] = std::move(local);
    }
    for (auto& v : thread_hits) primes.insert(primes.end(), v.begin(), v.end());

    return primes;
}

// ============================================================================
//  并行大区间分段筛
// ============================================================================

std::vector<long long> segmented_sieve_large(long long low, long long high) {
    if (low < 2) low = 2;
    if (low > high) return {};

    // 混合筛只适合「稀疏大数区间」：区间宽度很小但 high 很大。
    // 注意按区间宽度（而非 high）判定：宽区间即使 high 很大也走分段筛。
    if (high - low < 1'000'000LL && high > HYBRID_THRESHOLD) {
        return hybrid_sieve(low, high);
    }

    ensure_base_primes(high);

    int nthreads = omp_get_max_threads();
    long long seg_size = SEG_SIZE;
    std::vector<std::vector<long long>> thread_results(nthreads);

#pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<long long> local;
        local.reserve((size_t)((high - low + 1) / log((double)high + 1) * 1.2) / nthreads + 100);

#pragma omp for schedule(dynamic, 8)
        for (long long seg_low = low; seg_low <= high; seg_low += seg_size) {
            long long seg_high = std::min(seg_low + seg_size - 1, high);
            if (seg_low > seg_high) continue;
            segmented_sieve_append(seg_low, seg_high, g_base_primes, local);
        }

        thread_results[tid] = std::move(local);
    }

    // schedule(dynamic) 下各线程拿到的是非连续的段，故每线程结果虽升序，
    // 但按 tid 拼接并不有序。这里对线程结果做 k 路归并，保证全局有序。
    size_t total = 0;
    for (auto& v : thread_results) total += v.size();
    std::vector<long long> all_primes;
    all_primes.reserve(total);

    std::vector<const std::vector<long long>*> lists;
    for (auto& v : thread_results) if (!v.empty()) lists.push_back(&v);
    struct HeapItem { long long val; size_t list; size_t pos; };
    auto cmp = [](const HeapItem& a, const HeapItem& b) { return a.val > b.val; }; // 最小堆
    std::priority_queue<HeapItem, std::vector<HeapItem>, decltype(cmp)> pq(cmp);
    for (size_t i = 0; i < lists.size(); ++i)
        pq.push({(*lists[i])[0], i, 0});
    while (!pq.empty()) {
        HeapItem it = pq.top(); pq.pop();
        all_primes.push_back(it.val);
        size_t np = it.pos + 1;
        if (np < lists[it.list]->size())
            pq.push({(*lists[it.list])[np], it.list, np});
    }
    return all_primes;
}

long long prime_count(long long x) {
    if (x < 2) return 0;
    ensure_base_primes(x);
    long long seg = SEG_SIZE;
    long long count = 0;
    for (long long low = 2; low <= x; low += seg) {
        long long high = std::min(low + seg - 1, x);
        count += (long long)count_primes_range(low, high, g_base_primes);
        if (high == x) break;
    }
    return count;
}
