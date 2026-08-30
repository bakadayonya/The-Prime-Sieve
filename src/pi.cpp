// ============================================================================
//  pi.cpp — Meissel–Lehmer 素数计数 π(x) 与第 n 个素数
//  对 x < LEHMER_SIEVE_LIMIT 直接查前缀表 O(1)；否则用 Lehmer 公式递归。
//  phi 与 π 结果均记忆化，单次 π(x)（x 到 10^10 量级）亚毫秒级。
//  注意：本模块为单线程使用（仅 find_nth_prime 与自检调用）。
// ============================================================================

#include "pi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

const int LEHMER_SIEVE_LIMIT = 5'000'000;   // 前缀表与基础素数表上限
const int LEHMER_PHI_N = 200'000;           // phi 记忆表 x 上限
const int LEHMER_PHI_S = 7;                 // phi 记忆表 s 上限（前 7 个素数）

static std::vector<int> g_lehmer_primes;    // <= LEHMER_SIEVE_LIMIT 的素数
static std::vector<uint32_t> g_pi_small;    // pi(n), n < LEHMER_SIEVE_LIMIT
static std::vector<std::vector<uint32_t>> g_phi; // phi[s][x] for s<=7, x<LEHMER_PHI_N
static std::once_flag g_lehmer_init_flag;

// phi 与 π 的记忆化缓存（跨二分步骤复用，越查越快）。
struct LehmerKey {
    uint64_t x; int s;
    bool operator==(const LehmerKey& o) const { return x == o.x && s == o.s; }
};
struct LehmerKeyHash {
    size_t operator()(const LehmerKey& k) const {
        return (size_t)(k.x * 1000003ull + (uint64_t)(unsigned)k.s);
    }
};
static std::unordered_map<LehmerKey, long long, LehmerKeyHash> g_phi_memo;
static std::unordered_map<uint64_t, long long> g_pi_memo;

static void init_lehmer() {
    long long limit = LEHMER_SIEVE_LIMIT;
    std::vector<uint8_t> is_prime((size_t)limit + 1, 1);
    if (limit >= 0) is_prime[0] = 0;
    if (limit >= 1) is_prime[1] = 0;
    for (long long i = 2; i * i <= limit; ++i)
        if (is_prime[(size_t)i])
            for (long long j = i * i; j <= limit; j += i) is_prime[(size_t)j] = 0;
    g_pi_small.assign((size_t)limit + 1, 0);
    for (long long i = 2; i <= limit; ++i) {
        g_pi_small[(size_t)i] = g_pi_small[(size_t)(i - 1)] + (is_prime[(size_t)i] ? 1 : 0);
        if (is_prime[(size_t)i]) g_lehmer_primes.push_back((int)i);
    }
    // phi 记忆表：phi[s][x] = [1,x] 内不被前 s 个素数整除的个数
    g_phi.assign(LEHMER_PHI_S + 1, std::vector<uint32_t>(LEHMER_PHI_N, 0));
    for (int i = 0; i < LEHMER_PHI_N; ++i) g_phi[0][(size_t)i] = (uint32_t)i;
    for (int s = 1; s <= LEHMER_PHI_S; ++s) {
        uint32_t p = (uint32_t)g_lehmer_primes[s - 1];
        for (int i = 0; i < LEHMER_PHI_N; ++i)
            g_phi[(size_t)s][(size_t)i] =
                g_phi[(size_t)(s - 1)][(size_t)i] - g_phi[(size_t)(s - 1)][(size_t)(i / p)];
    }
}

// 精确整数平方根 / 立方根 / 四次方根（带校正，避免浮点截断误差）。
static uint64_t lehmer_isqrt(uint64_t x) {
    uint64_t r = (uint64_t)sqrtl((long double)x);
    while ((r + 1) * (r + 1) <= x) ++r;
    while (r * r > x) --r;
    return r;
}
static uint64_t lehmer_iroot4(uint64_t x) {
    uint64_t r = lehmer_isqrt(lehmer_isqrt(x));
    while ((r + 1) * (r + 1) <= x / (r + 1) / (r + 1)) ++r;  // (r+1)^4 <= x
    while (r * r > x / r / r) --r;                            // r^4 > x
    return r;
}
static uint64_t lehmer_icbrt(uint64_t x) {
    uint64_t lo = 0, hi = 2'100'000;
    while (lo < hi) {
        uint64_t mid = (lo + hi + 1) / 2;
        if (mid * mid * mid <= x) lo = mid; else hi = mid - 1;
    }
    return lo;
}

// phi(x, s)：[1,x] 内不被前 s 个素数整除的整数个数。
static long long lehmer_phi(uint64_t x, int s) {
    if (s == 0) return (long long)x;
    if (s == 1) return (long long)x - (long long)(x / 2);
    if (s == 2) return (long long)x - (long long)(x / 2) - (long long)(x / 3) + (long long)(x / 6);
    if (s == 3)
        return (long long)x - (long long)(x / 2) - (long long)(x / 3) + (long long)(x / 6)
             - (long long)(x / 5) + (long long)(x / 10) + (long long)(x / 15) - (long long)(x / 30);
    if (s <= LEHMER_PHI_S && x < (uint64_t)LEHMER_PHI_N) return g_phi[(size_t)s][(size_t)x];
    auto it = g_phi_memo.find(LehmerKey{x, s});
    if (it != g_phi_memo.end()) return it->second;
    long long r = lehmer_phi(x, s - 1) - lehmer_phi(x / (uint64_t)g_lehmer_primes[s - 1], s - 1);
    g_phi_memo.emplace(LehmerKey{x, s}, r);
    return r;
}

long long lehmer_pi(uint64_t x) {
    std::call_once(g_lehmer_init_flag, init_lehmer);
    if (x < (uint64_t)LEHMER_SIEVE_LIMIT) return g_pi_small[(size_t)x];
    auto it = g_pi_memo.find(x);
    if (it != g_pi_memo.end()) return it->second;
    uint64_t a = (uint64_t)lehmer_pi(lehmer_iroot4(x));
    uint64_t b = (uint64_t)lehmer_pi(lehmer_isqrt(x));
    uint64_t c = (uint64_t)lehmer_pi(lehmer_icbrt(x));
    long long sum = lehmer_phi(x, (int)a) + (long long)(b + a - 2) * (b - a + 1) / 2;
    for (uint64_t i = a + 1; i <= b; ++i) {
        uint64_t w = x / (uint64_t)g_lehmer_primes[i - 1];
        sum -= lehmer_pi(w);
        if (i <= c) {
            uint64_t lim = (uint64_t)lehmer_pi(lehmer_isqrt(w));
            for (uint64_t j = i; j <= lim; ++j)
                sum -= lehmer_pi(w / (uint64_t)g_lehmer_primes[j - 1]) - (long long)(j - 1);
        }
    }
    g_pi_memo.emplace(x, sum);
    return sum;
}

long long find_nth_prime(long long n) {
    if (n <= 0) return -1;
    if (n == 1) return 2;
    if (n == 2) return 3;

    std::call_once(g_lehmer_init_flag, init_lehmer);

    // 初始估计上下界（Rosser–Schoenfeld）
    double logn = log((double)n);
    double loglogn = log(logn);
    double approx = n * (logn + loglogn - 1.0 + (loglogn - 2.0) / logn);
    long long lower = std::max(5LL, (long long)(approx * 0.95));
    long long upper = (long long)(approx * 1.2) + 1000;

    // 确保 lower < 第 n 个素数 <= upper
    while ((uint64_t)lehmer_pi((uint64_t)lower) >= (uint64_t)n) lower = (lower * 2) / 3;
    while ((uint64_t)lehmer_pi((uint64_t)upper) < (uint64_t)n) upper = (long long)(upper * 1.5) + 1000;

    // 二分搜索
    while (lower < upper) {
        long long mid = lower + (upper - lower) / 2;
        if ((uint64_t)lehmer_pi((uint64_t)mid) >= (uint64_t)n) {
            upper = mid;
        } else {
            lower = mid + 1;
        }
    }
    return lower;
}
