// ============================================================================
//  actions.cpp — 各操作执行函数（区间筛 / 位数 / 单数检查 / 第 n 个 / 性能 / 自检）
//  供命令行 main 与交互模式共用。
// ============================================================================

#include "actions.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "config.h"
#include "output.h"
#include "pi.h"
#include "primality.h"
#include "sieve.h"

using std::chrono::steady_clock;
using std::chrono::duration;

static std::pair<long long, long long> get_range_by_digits(int digits) {
    if (digits <= 0 || digits > 18) return {0, 0};
    long long low = 1;
    for (int i = 1; i < digits; ++i) low *= 10;
    long long high = low * 10 - 1;
    if (digits == 1) low = 2;
    return {low, high};
}

void run_range(long long low, long long high) {
    if (low < 2) low = 2;
    if (high < low) {
        std::cerr << _("无效区间", "Invalid range") << std::endl;
        return;
    }
    long long range_size = high - low + 1;
    if (range_size > 1'000'000'000LL && !g_config.interactive) {
        std::cerr << _("区间过大（>10亿），若确认请使用交互模式",
                       "Range too large (>1e9), use interactive mode") << std::endl;
        return;
    }

    if (!g_config.quiet) std::cout << _("\n筛选中...", "\nSieving...") << std::endl;
    auto start = steady_clock::now();
    std::vector<long long> primes;
    if (range_size > 4 * 1024 * 1024 * 2LL) {  // > 2 段宽，走并行分段筛
        primes = segmented_sieve_large(low, high);
    } else {
        ensure_base_primes(high);
        primes = segmented_sieve(low, high, base_primes());
    }
    auto end = steady_clock::now();
    double elapsed = duration<double>(end - start).count();

    if (!g_config.quiet) std::cout << _(" 完成！", " Done!") << std::endl;
    output_results(primes, elapsed, g_config.quiet);
}

void run_digits(int digits) {
    auto range = get_range_by_digits(digits);
    if (range.first == 0) {
        std::cerr << _("位数超出范围 (1-18)", "Digits out of range (1-18)") << std::endl;
        return;
    }
    run_range(range.first, range.second);
}

void run_check(uint64_t n) {
    auto start = steady_clock::now();
    bool prime = is_prime_mr(n);
    auto end = steady_clock::now();
    double elapsed = duration<double>(end - start).count();

    OutputSink sink;
    if (!sink.open(g_config.output_file, g_config.quiet)) return;
    std::ostream& out = sink.out();

    out << n << (prime ? _(" 是素数", " is prime") : _(" 是合数", " is composite")) << std::endl;
    out << _("耗时: ", "Time: ") << elapsed << _(" 秒", " seconds") << std::endl;
}

void run_nth(long long n) {
    if (n <= 0) {
        std::cerr << _("n 必须为正整数", "n must be positive integer") << std::endl;
        return;
    }
    auto start = steady_clock::now();
    long long prime = find_nth_prime(n);
    auto end = steady_clock::now();
    double elapsed = duration<double>(end - start).count();

    OutputSink sink;
    if (!sink.open(g_config.output_file, g_config.quiet)) return;
    std::ostream& out = sink.out();

    if (prime > 0) {
        out << _("第 ", "The ") << n << _(" 个素数是: ", "-th prime is: ") << prime << std::endl;
    } else {
        out << _("计算失败", "Failed") << std::endl;
    }
    out << _("耗时: ", "Time: ") << elapsed << _(" 秒", " seconds") << std::endl;
}

void run_performance() {
    run_range(2, 10000000);
}

// 内置正确性自检（--verify）：校验已知 π(x)、Miller-Rabin、分段筛/混合筛一致性。
// 返回失败数（0 = 全部通过），供 main 作为退出码。
int run_verify() {
    int failures = 0;
    auto report = [&](const char* name, bool ok) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name;
        if (!ok) ++failures;
        std::cout << std::endl;
    };

    // 1) 已知 π(x)
    struct { long long x; long long pi; } pi_checks[] = {
        {1000000, 78498},
        {10000000, 664579},
        {100000000, 5761455},
    };
    for (auto& c : pi_checks) {
        long long got = prime_count(c.x);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "π(%lld) = %lld", c.x, got);
        report(buf, got == c.pi);
    }

    // 2) Miller-Rabin 已知素数/合数
    struct { uint64_t n; bool prime; } mr_checks[] = {
        {2ULL, true}, {3ULL, true}, {97ULL, true}, {999983ULL, true},
        {2147483647ULL, true},            // 2^31-1
        {1000000ULL, false}, {2147483646ULL, false},
        {4294967297ULL, false},           // 641 * 6700417
    };
    for (auto& c : mr_checks) {
        bool got = is_prime_mr(c.n);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "MR(%llu) 期望=%s 实际=%s",
                      (unsigned long long)c.n, c.prime ? "prime" : "composite",
                      got ? "prime" : "composite");
        report(buf, got == c.prime);
    }

    // 3) 分段筛 vs 混合筛 在稀疏大数区间上结果一致
    {
        long long lo = 999999999989LL, hi = 1000000000039LL;
        std::vector<long long> a, b;
        ensure_base_primes(hi);
        segmented_sieve_append(lo, hi, base_primes(), a);
        b = hybrid_sieve(lo, hi);
        bool same = (a == b);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "seg vs hybrid [%lld, %lld] 分段=%zu 混合=%zu",
                      lo, hi, a.size(), b.size());
        report(buf, same);
    }

    // 4) Lehmer π(x) 与已知 π 值、以及和分段筛 prime_count 交叉验证
    {
        struct { long long x; long long pi; } known_pi[] = {
            {1000000000LL, 50847534},
            {10000000000LL, 455052511},
        };
        for (auto& c : known_pi) {
            long long got = lehmer_pi((uint64_t)c.x);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "lehmer π(%lld) = %lld", c.x, got);
            report(buf, got == c.pi);
        }
        long long xs[] = {1234567LL, 99999989LL, 100000000LL,
                          150000000LL, 300000000LL, 500000000LL};
        for (long long x : xs) {
            long long p = prime_count(x), l = lehmer_pi((uint64_t)x);
            char buf[160];
            std::snprintf(buf, sizeof(buf), "π(%lld) prime_count=%lld lehmer=%lld", x, p, l);
            report(buf, p == l);
        }
        struct { long long n; long long p; } nth_checks[] = {
            {10, 29}, {100, 541}, {1000, 7919}, {10000, 104729},
            {1000000, 15485863}, {10000000, 179424673},
        };
        for (auto& c : nth_checks) {
            long long got = find_nth_prime(c.n);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "第 %lld 个素数 = %lld", c.n, got);
            report(buf, got == c.p);
        }
    }

    std::cout << _("\n验证完成，失败 ", "\nVerification done, ")
              << failures << _(" 项。", " failure(s).") << std::endl;
    return failures;
}
