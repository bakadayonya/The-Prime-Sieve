// ============================================================================
//  超级素数筛 - 极致优化版 v9.0 (支持命令行参数)
//  特性：动态 Miller-Rabin、大预筛、模30轮子分段筛、OpenMP 并行、混合策略、
//        Meissel-Lehmer 素数计数（第 n 个素数亚秒级）
//  用法：./prime_sieve [选项]
//  无参数时进入交互模式
// ============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <omp.h>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace std::chrono;

// ============================================================================
//  可调参数（经实测调优）
// ============================================================================

const int MR_BASE_COUNT_MAX = 12;
const uint64_t MR_BASES_ALL[MR_BASE_COUNT_MAX] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};

const int MR_BASE_COUNT_SMALL = 7;
const uint64_t MR_BASES_SMALL[MR_BASE_COUNT_SMALL] = {2, 3, 5, 7, 11, 13, 17};

// 预筛素数范围（混合筛中动态调整）
const long long SMALL_PRIME_LIMIT_MIN = 2000;   // 最小预筛上限
const long long SMALL_PRIME_LIMIT_MAX = 50000;  // 最大预筛上限

const long long HYBRID_THRESHOLD = 10'000'000'000LL;
// 每个分段覆盖的区间宽度（模 30 位图约 8/30 * SEG_SIZE 位，约 136 KB/线程）
const long long SEG_SIZE = 4 * 1024 * 1024;

// ============================================================================
//  全局状态与配置
// ============================================================================

enum Language { LANG_CHINESE, LANG_ENGLISH };
Language g_lang = LANG_CHINESE;

vector<long long> g_base_primes;

// 命令行配置
bool g_interactive_mode = false;   // 默认无参数时进入交互
bool g_quiet = false;
string g_output_file;              // 空表示不输出到文件

// 操作类型
enum Action { ACTION_NONE, ACTION_RANGE, ACTION_DIGITS, ACTION_CHECK, ACTION_NTH, ACTION_PERF, ACTION_VERIFY };
Action g_action = ACTION_NONE;
long long g_low = 0, g_high = 0;
long long g_nth = 0;
uint64_t g_check_num = 0;
int g_digits = 0;

// ============================================================================
//  国际化辅助
// ============================================================================

inline string _(const string& zh, const string& en) {
    return (g_lang == LANG_CHINESE) ? zh : en;
}

// ============================================================================
//  安全输入辅助（交互模式使用）
// ============================================================================

template <typename T>
bool safe_input(T& value, const string& prompt = "") {
    while (true) {
        if (!prompt.empty()) cout << prompt;
        cin >> value;
        if (cin.fail()) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << _("输入无效，请输入一个整数。", "Invalid input, please enter an integer.") << endl;
        } else {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return true;
        }
    }
}

bool confirm_action(const string& prompt) {
    string input;
    while (true) {
        cout << prompt;
        if (!(cin >> input)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << _("输入无效，请输入 y 或 n。", "Invalid input, please enter y or n.") << endl;
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (input.size() == 1 && (input[0] == 'y' || input[0] == 'Y' || input[0] == 'n' || input[0] == 'N')) {
            return (input[0] == 'y' || input[0] == 'Y');
        }
        cout << _("请输入 'y' 或 'n'。", "Please enter 'y' or 'n'.") << endl;
    }
}

// ============================================================================
//  工具函数：模幂、Miller-Rabin
// ============================================================================

inline uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t res = 1;
    base %= mod;
    while (exp) {
        if (exp & 1) res = (uint64_t)((__int128)res * base % mod);
        exp >>= 1;
        base = (uint64_t)((__int128)base * base % mod);
    }
    return res;
}

inline bool is_prime_mr_fast(uint64_t n) {
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

// ============================================================================
//  简单奇数筛（用于生成基础素数表）
// ============================================================================

vector<long long> simple_sieve_odd(long long limit) {
    vector<long long> primes;
    if (limit < 2) return primes;
    primes.push_back(2);
    if (limit == 2) return primes;

    long long odd_cnt = (limit - 1) / 2;
    size_t words = (odd_cnt + 63) / 64;
    vector<uint64_t> bits(words, ~0ULL);
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

// 生成基础素数表（供分段筛使用）
void ensure_base_primes(long long limit) {
    static mutex expand_mutex;
    lock_guard<mutex> lock(expand_mutex);
    long long needed = (long long)sqrt((double)limit) + 1;
    if (g_base_primes.empty() || g_base_primes.back() * g_base_primes.back() < limit) {
        g_base_primes = simple_sieve_odd(needed);
    }
}

inline long long get_segment_size() {
    return SEG_SIZE;
}

// ============================================================================
//  模 30 轮子分段筛核心
//  只处理与 30 互质的数（8 个剩余类 {1,7,11,13,17,19,23,29}），因此 2、3、5
//  的倍数天然不在位图中，无需标记（原实现漏标 3 的倍数导致结果含合数）。
//  位图仅 8/30 ≈ 26.7% 大小：每 30 个数 8 位，每 64 位覆盖 240 个数。
// ============================================================================

// 与 30 互质的剩余类（升序）
static const uint8_t WHEEL_R[8] = {1, 7, 11, 13, 17, 19, 23, 29};
// 剩余类 -> 其在 mod 30 下的乘法逆元
static uint8_t WHEEL_INV[30];
static once_flag g_wheel_init_flag;

static void init_wheel() {
    for (int k = 0; k < 8; ++k) {
        for (int inv = 1; inv < 30; inv += 2)
            if ((WHEEL_R[k] * inv) % 30 == 1) { WHEEL_INV[WHEEL_R[k]] = (uint8_t)inv; break; }
    }
}

// 对 [low, high] 内与 30 互质的数做筛，结果写入 bits 位图（每块 8 位）。
// 返回块数；low 需已 clamp 到 >=7（2、3、5 的倍数不在位图中）。
static long long mark_segment(long long low, long long high,
                              const vector<long long>& base_primes,
                              vector<uint64_t>& bits) {
    call_once(g_wheel_init_flag, init_wheel);
    long long seg_base_q = low / 30;
    long long num_blocks = high / 30 - seg_base_q + 1;
    size_t words = (num_blocks + 7) / 8;
    if (bits.size() < words) bits.resize(words);
    fill(bits.begin(), bits.begin() + words, ~0ULL);
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
            long long m_need = (n_min + p - 1) / p;   // 要求 m >= m_need
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
// low 需已 clamp 到 >=7（2、3、5 已另行处理）。count/append 两个用途共用此函数。
template <typename Emit>
static void scan_segment(long long low, long long high,
                         const vector<uint64_t>& bits,
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
                            const vector<long long>& base_primes,
                            vector<long long>& out_primes) {
    if (low > high || high < 2) return;
    if (low < 2) low = 2;
    for (long long p : {2LL, 3LL, 5LL})
        if (low <= p && p <= high) out_primes.push_back(p);
    if (high < 7) return;
    if (low < 7) low = 7;

    thread_local vector<uint64_t> bits;
    mark_segment(low, high, base_primes, bits);
    scan_segment(low, high, bits, [&](long long n) { out_primes.push_back(n); });
}

// 计数 [low, high] 内素数个数（low>=2），不构造结果向量，省内存。
size_t count_primes_range(long long low, long long high,
                          const vector<long long>& base_primes) {
    if (low > high || high < 2) return 0;
    if (low < 2) low = 2;
    size_t c = 0;
    for (long long p : {2LL, 3LL, 5LL})
        if (low <= p && p <= high) ++c;
    if (high < 7) return c;
    if (low < 7) low = 7;

    thread_local vector<uint64_t> bits;
    mark_segment(low, high, base_primes, bits);
    scan_segment(low, high, bits, [&](long long) { ++c; });
    return c;
}

// 非并行版本（用于小范围）
vector<long long> segmented_sieve(long long low, long long high,
                                  const vector<long long>& base_primes) {
    vector<long long> result;
    segmented_sieve_append(low, high, base_primes, result);
    return result;
}

// ============================================================================
//  混合筛（小素数预筛 + Miller-Rabin 验证）
//  动态调整预筛素数范围
// ============================================================================

vector<long long> hybrid_sieve(long long low, long long high) {
    vector<long long> primes;
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

    // 生成预筛小素数（使用简单奇数筛）
    vector<long long> small_primes = simple_sieve_odd(small_prime_limit);
    // 移除 2 和 3，因为已经单独处理
    small_primes.erase(
        remove_if(small_primes.begin(), small_primes.end(),
                  [](long long p) { return p == 2 || p == 3; }),
        small_primes.end());

    long long count = (high - first_odd) / 2 + 1; // 奇数个数
    size_t words = (count + 63) / 64;
    vector<uint64_t> maybe_prime(words, ~0ULL);

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

    // 提取候选
    vector<long long> candidates;
    candidates.reserve(count / 10); // 粗略估计
    for (long long i = 0; i < count; ++i) {
        if (is_set(i)) {
            candidates.push_back(first_odd + 2 * i);
        }
    }

    // 并行 Miller-Rabin 测试
    vector<char> is_prime(candidates.size(), 0);
#pragma omp parallel for schedule(dynamic, 1024)
    for (long long i = 0; i < (long long)candidates.size(); ++i) {
        if (is_prime_mr_fast(candidates[i])) {
            is_prime[i] = 1;
        }
    }

    // 收集结果
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (is_prime[i]) primes.push_back(candidates[i]);
    }

    return primes;
}

// ============================================================================
//  并行大区间分段筛
// ============================================================================

vector<long long> segmented_sieve_large(long long low, long long high) {
    if (low < 2) low = 2;
    if (low > high) return {};

    // 混合筛只适合「稀疏大数区间」：区间宽度很小但 high 很大。
    // 注意按区间宽度（而非 high）判定：宽区间即使 high 很大也走分段筛，
    // 因为分段筛内存只取决于段宽、与 high 无关；若这里按 high 无条件切混合筛，
    // 会把整个区间的奇数位图一次性物化，导致内存爆炸（如 digits>=12 时崩溃）。
    if (high - low < 1'000'000LL && high > HYBRID_THRESHOLD) {
        return hybrid_sieve(low, high);
    }

    ensure_base_primes(high);

    int nthreads = omp_get_max_threads();
    long long seg_size = get_segment_size();
    vector<vector<long long>> thread_results(nthreads);

#pragma omp parallel
    {
        int tid = omp_get_thread_num();
        vector<long long> local;
        local.reserve((size_t)((high - low + 1) / log((double)high + 1) * 1.2) / nthreads + 100);

#pragma omp for schedule(dynamic, 8)
        for (long long seg_low = low; seg_low <= high; seg_low += seg_size) {
            long long seg_high = min(seg_low + seg_size - 1, high);
            if (seg_low > seg_high) continue;
            segmented_sieve_append(seg_low, seg_high, g_base_primes, local);
        }

        thread_results[tid] = std::move(local);
    }

    // schedule(dynamic) 下各线程拿到的是非连续的段，故每线程结果虽升序，
    // 但按 tid 拼接并不有序。这里对线程结果做 k 路归并，保证全局有序。
    size_t total = 0;
    for (auto& v : thread_results) total += v.size();
    vector<long long> all_primes;
    all_primes.reserve(total);

    vector<const vector<long long>*> lists;
    for (auto& v : thread_results) if (!v.empty()) lists.push_back(&v);
    struct HeapItem { long long val; size_t list; size_t pos; };
    auto cmp = [](const HeapItem& a, const HeapItem& b) { return a.val > b.val; }; // 最小堆
    priority_queue<HeapItem, vector<HeapItem>, decltype(cmp)> pq(cmp);
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

// ============================================================================
//  素数计数函数 π(x)（分段筛计数版，用于 -v 自检与 lehmer_pi 交叉验证）
// ============================================================================

long long prime_count(long long x) {
    if (x < 2) return 0;
    ensure_base_primes(x);
    long long seg = get_segment_size();
    long long count = 0;
    for (long long low = 2; low <= x; low += seg) {
        long long high = min(low + seg - 1, x);
        count += (long long)count_primes_range(low, high, g_base_primes);
        if (high == x) break;
    }
    return count;
}

// ============================================================================
//  Meissel–Lehmer 素数计数 π(x)（供第 n 个素数快速二分）
//  对 x < LEHMER_SIEVE_LIMIT 直接查前缀表 O(1)；否则用 Lehmer 公式递归。
//  phi 与 π 结果均记忆化，单次 π(x)（x 到 10^10 量级）亚毫秒级，
//  彻底消除旧实现「二分每步都对 [2,x] 完整重筛」的巨额开销。
//  注意：本函数为单线程使用（仅 find_nth_prime 与自检调用）。
// ============================================================================

const int LEHMER_SIEVE_LIMIT = 5'000'000;   // 前缀表与基础素数表上限
const int LEHMER_PHI_N = 200'000;           // phi 记忆表 x 上限
const int LEHMER_PHI_S = 7;                 // phi 记忆表 s 上限（前 7 个素数）

static vector<int> g_lehmer_primes;         // <= LEHMER_SIEVE_LIMIT 的素数
static vector<uint32_t> g_pi_small;         // pi(n), n < LEHMER_SIEVE_LIMIT
static vector<vector<uint32_t>> g_phi;      // phi[s][x] for s<=7, x<LEHMER_PHI_N
static once_flag g_lehmer_init_flag;

// phi 与 π 的记忆化缓存（跨二分步骤复用，越查越快）。
struct LehmerKey { uint64_t x; int s;
    bool operator==(const LehmerKey& o) const { return x == o.x && s == o.s; }
};
struct LehmerKeyHash {
    size_t operator()(const LehmerKey& k) const {
        return (size_t)(k.x * 1000003ull + (uint64_t)(unsigned)k.s);
    }
};
static unordered_map<LehmerKey, long long, LehmerKeyHash> g_phi_memo;
static unordered_map<uint64_t, long long> g_pi_memo;

static void init_lehmer() {
    long long limit = LEHMER_SIEVE_LIMIT;
    vector<uint8_t> is_prime((size_t)limit + 1, 1);
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
    g_phi.assign(LEHMER_PHI_S + 1, vector<uint32_t>(LEHMER_PHI_N, 0));
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

// π(x) 主入口。
static long long lehmer_pi(uint64_t x) {
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

// ============================================================================
//  查找第 n 个素数（二分，π(x) 用 Lehmer）
// ============================================================================

long long find_nth_prime(long long n) {
    if (n <= 0) return -1;
    if (n == 1) return 2;
    if (n == 2) return 3;

    call_once(g_lehmer_init_flag, init_lehmer);

    // 初始估计上下界（Rosser–Schoenfeld）
    double logn = log((double)n);
    double loglogn = log(logn);
    double approx = n * (logn + loglogn - 1.0 + (loglogn - 2.0) / logn);
    long long lower = max(5LL, (long long)(approx * 0.95));
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

// ============================================================================
//  辅助功能
// ============================================================================

pair<long long, long long> get_range_by_digits(int digits) {
    if (digits <= 0 || digits > 18) return {0, 0};
    long long low = 1;
    for (int i = 1; i < digits; ++i) low *= 10;
    long long high = low * 10 - 1;
    if (digits == 1) low = 2;
    return {low, high};
}

// RAII 输出目标：未指定输出文件时写 stdout；指定时写入文件，并在析构时
// 统一给出「结果已写入文件」的提示。open() 失败返回 false（已打印错误）。
class OutputSink {
public:
    ostream& out() { return *stream_; }
    bool open(const string& path, bool quiet) {
        if (path.empty()) { stream_ = &cout; return true; }
        file_.open(path);
        if (!file_) {
            cerr << _("无法打开输出文件: ", "Cannot open output file: ") << path << endl;
            return false;
        }
        stream_ = &file_;
        path_ = path;
        quiet_ = quiet;
        return true;
    }
    ~OutputSink() {
        if (file_.is_open()) {
            file_.close();
            if (!quiet_)
                cout << _("结果已写入文件: ", "Results written to file: ") << path_ << endl;
        }
    }

private:
    ostream* stream_ = &cout;
    ofstream file_;
    string path_;
    bool quiet_ = false;
};

// 输出结果到标准输出或文件
void output_results(const vector<long long>& primes, double elapsed, bool show_stats_only = false) {
    OutputSink sink;
    if (!sink.open(g_output_file, g_quiet)) return;
    ostream& out = sink.out();

    if (primes.empty()) {
        out << _("该区间没有素数", "No primes in this range") << endl;
        return;
    }

    if (!g_quiet) {
        out << _("\n找到 ", "\nFound ") << primes.size() << _(" 个素数", " primes");
        if (!show_stats_only) {
            out << ":" << endl;
        } else {
            out << endl;
        }
    }

    if (!show_stats_only) {
        const int max_display = g_quiet ? 0 : 200;
        if (max_display > 0 && !primes.empty()) {
            out << "----------------------------------------" << endl;
            int cnt = 0;
            size_t limit = min(primes.size(), (size_t)max_display);
            for (size_t i = 0; i < limit; ++i) {
                out << primes[i];
                if (++cnt % 10 == 0) out << '\n';
                else out << '\t';
            }
            if (cnt % 10 != 0) out << '\n';
            if (primes.size() > (size_t)max_display) {
                out << _("... 还有 ", "... and ") << (primes.size() - max_display) << _(" 个未显示", " more not shown") << endl;
            }
            out << "----------------------------------------" << endl;
        }
    }

    // 统计信息
    out << _("\n========== 统计信息 ==========", "\n========== Statistics ==========") << endl;
    out << _("素数总数: ", "Total primes: ") << primes.size() << endl;
    out << _("最小素数: ", "Smallest prime: ") << primes.front() << endl;
    out << _("最大素数: ", "Largest prime: ") << primes.back() << endl;
    out << _("耗时: ", "Time: ") << fixed << setprecision(3) << elapsed << _(" 秒", " seconds") << endl;
    if (primes.size() > 1) {
        double density = (double)primes.size() / (primes.back() - primes.front()) * 100;
        out << _("素数密度: ", "Prime density: ") << fixed << setprecision(4) << density << "%" << endl;
    }
    out << _("================================", "====================================") << endl;
}

// ============================================================================
//  功能执行函数（供交互和命令行调用）
// ============================================================================

void run_range(long long low, long long high) {
    if (low < 2) low = 2;
    if (high < low) {
        cerr << _("无效区间", "Invalid range") << endl;
        return;
    }
    long long range_size = high - low + 1;
    if (range_size > 1'000'000'000LL && !g_interactive_mode) {
        cerr << _("区间过大（>10亿），若确认请使用交互模式", 
                  "Range too large (>1e9), use interactive mode") << endl;
        return;
    }

    if (!g_quiet) cout << _("\n筛选中...", "\nSieving...") << endl;
    auto start = steady_clock::now();
    vector<long long> primes;
    if (range_size > get_segment_size() * 2) {
        primes = segmented_sieve_large(low, high);
    } else {
        ensure_base_primes(high);
        primes = segmented_sieve(low, high, g_base_primes);
    }
    auto end = steady_clock::now();
    double elapsed = duration<double>(end - start).count();

    if (!g_quiet) cout << _(" 完成！", " Done!") << endl;
    output_results(primes, elapsed, g_quiet);
}

void run_digits(int digits) {
    auto range = get_range_by_digits(digits);
    if (range.first == 0) {
        cerr << _("位数超出范围 (1-18)", "Digits out of range (1-18)") << endl;
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
    if (!sink.open(g_output_file, g_quiet)) return;
    ostream& out = sink.out();

    out << n << (prime ? _(" 是素数", " is prime") : _(" 是合数", " is composite")) << endl;
    out << _("耗时: ", "Time: ") << elapsed << _(" 秒", " seconds") << endl;
}

void run_nth(long long n) {
    if (n <= 0) {
        cerr << _("n 必须为正整数", "n must be positive integer") << endl;
        return;
    }
    auto start = steady_clock::now();
    long long prime = find_nth_prime(n);
    auto end = steady_clock::now();
    double elapsed = duration<double>(end - start).count();

    OutputSink sink;
    if (!sink.open(g_output_file, g_quiet)) return;
    ostream& out = sink.out();

    if (prime > 0) {
        out << _("第 ", "The ") << n << _(" 个素数是: ", "-th prime is: ") << prime << endl;
    } else {
        out << _("计算失败", "Failed") << endl;
    }
    out << _("耗时: ", "Time: ") << elapsed << _(" 秒", " seconds") << endl;
}

void run_performance() {
    run_range(2, 10000000);
}

// ============================================================================
//  内置正确性自检（--verify）
//  校验已知 π(x)、Miller-Rabin、分段筛与混合筛一致性；供 ctest 使用。
//  返回失败数（0 = 全部通过），供主函数作为退出码。
// ============================================================================

int run_verify() {
    int failures = 0;
    auto report = [&](const char* name, bool ok) {
        cout << (ok ? "[PASS] " : "[FAIL] ") << name;
        if (!ok) ++failures;
        cout << endl;
    };

    // 1) 已知 π(x)（素数计数）
    struct { long long x; long long pi; } pi_checks[] = {
        {1000000,   78498},
        {10000000,  664579},
        {100000000, 5761455},
    };
    for (auto& c : pi_checks) {
        long long got = prime_count(c.x);
        char buf[128];
        snprintf(buf, sizeof(buf), "π(%lld) = %lld", c.x, got);
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
        snprintf(buf, sizeof(buf), "MR(%llu) 期望=%s 实际=%s",
                 (unsigned long long)c.n, c.prime ? "prime" : "composite", got ? "prime" : "composite");
        report(buf, got == c.prime);
    }

    // 3) 分段筛 vs 混合筛 在稀疏大数区间上结果一致
    {
        long long lo = 999999999989LL, hi = 1000000000039LL;
        vector<long long> a, b;
        ensure_base_primes(hi);
        segmented_sieve_append(lo, hi, g_base_primes, a);
        b = hybrid_sieve(lo, hi);
        bool same = (a == b);
        char buf[128];
        snprintf(buf, sizeof(buf), "seg vs hybrid [%lld, %lld] 分段=%zu 混合=%zu",
                 lo, hi, a.size(), b.size());
        report(buf, same);
    }

    // 4) Lehmer π(x) 与已知 π 值、以及和分段筛 prime_count 交叉验证
    {
        call_once(g_lehmer_init_flag, init_lehmer);
        struct { long long x; long long pi; } known_pi[] = {
            {1000000000LL, 50847534},
            {10000000000LL, 455052511},
        };
        for (auto& c : known_pi) {
            long long got = lehmer_pi((uint64_t)c.x);
            char buf[128];
            snprintf(buf, sizeof(buf), "lehmer π(%lld) = %lld", c.x, got);
            report(buf, got == c.pi);
        }
        // 分段筛 vs Lehmer 交叉验证（跨数量级）
        long long xs[] = {1234567LL, 99999989LL, 100000000LL,
                          150000000LL, 300000000LL, 500000000LL};
        for (long long x : xs) {
            long long p = prime_count(x), l = lehmer_pi((uint64_t)x);
            char buf[160];
            snprintf(buf, sizeof(buf), "π(%lld) prime_count=%lld lehmer=%lld", x, p, l);
            report(buf, p == l);
        }
        // 已知第 n 个素数
        struct { long long n; long long p; } nth_checks[] = {
            {10, 29}, {100, 541}, {1000, 7919}, {10000, 104729},
            {1000000, 15485863}, {10000000, 179424673},
        };
        for (auto& c : nth_checks) {
            long long got = find_nth_prime(c.n);
            char buf[128];
            snprintf(buf, sizeof(buf), "第 %lld 个素数 = %lld", c.n, got);
            report(buf, got == c.p);
        }
    }

    cout << _("\n验证完成，失败 ", "\nVerification done, ")
         << failures << _(" 项。", " failure(s).") << endl;
    return failures;
}

// ============================================================================
//  交互模式主循环
// ============================================================================

void run_interactive() {
    cout << _("========== 超级素数筛（极致优化版 v9.0） ==========",
              "========== Super Prime Sieve (Ultra Optimized v9.0) ==========")
         << endl;
    cout << _("支持范围: 2 ~ 10^18（更大可能极慢）",
              "Supported: 2 ~ 10^18 (larger may be slow)")
         << endl;
    cout << _("自动切换分段筛 / 混合筛（OpenMP + 动态调度 + 模30轮子）",
              "Auto-select segmented/hybrid sieve (OpenMP + dynamic scheduling + mod30 wheel)")
         << endl
         << endl;

    while (true) {
        cout << _("\n请选择模式：", "\nSelect mode:") << endl;
        cout << _("1. 按位数查找", "1. Search by digits") << endl;
        cout << _("2. 自定义区间", "2. Custom range") << endl;
        cout << _("3. 检查单数", "3. Check a single number") << endl;
        cout << _("4. 查找第 n 个素数", "4. Find nth prime") << endl;
        cout << _("5. 性能测试 (2~10^7)", "5. Performance test (2~10^7)") << endl;
        cout << _("6. 切换语言 (Switch Language)", "6. Switch Language") << endl;
        cout << _("7. 退出", "7. Exit") << endl;

        int choice;
        safe_input(choice, _("输入 (1-7): ", "Enter (1-7): "));

        if (choice == 7) {
            cout << _("再见！", "Goodbye!") << endl;
            break;
        }

        // 新增：语言切换
        if (choice == 6) {
            cout << _("\n当前语言: ", "\nCurrent language: ") 
                 << (g_lang == LANG_CHINESE ? _("中文", "Chinese") : _("英文", "English")) 
                 << endl;
            cout << _("1. 中文", "1. Chinese") << endl;
            cout << _("2. English", "2. English") << endl;
            
            int lang_choice;
            safe_input(lang_choice, _("请选择 (1-2): ", "Select (1-2): "));
            
            if (lang_choice == 1) {
                g_lang = LANG_CHINESE;
                cout << _("已切换到中文", "Switched to Chinese") << endl;
            } else if (lang_choice == 2) {
                g_lang = LANG_ENGLISH;
                cout << _("已切换到英文", "Switched to English") << endl;
            } else {
                cout << _("无效选择，保持当前语言", "Invalid choice, keeping current language") << endl;
            }
            continue;
        }

        long long low, high;
        if (choice == 1) {
            int digits;
            safe_input(digits, _("位数 (1-18): ", "Digits (1-18): "));
            auto range = get_range_by_digits(digits);
            low = range.first;
            high = range.second;
            if (low == 0) {
                cout << _("超出范围", "Out of range") << endl;
                continue;
            }
            string saved_output = g_output_file;
            g_output_file.clear();
            run_digits(digits);
            g_output_file = saved_output;
            continue;
        } else if (choice == 2) {
            safe_input(low, _("下限 low (>=2): ", "low (>=2): "));
            safe_input(high, _("上限 high: ", "high: "));
            if (low < 2 || high < low) {
                cout << _("无效区间", "Invalid range") << endl;
                continue;
            }
            string saved_output = g_output_file;
            g_output_file.clear();
            run_range(low, high);
            g_output_file = saved_output;
            continue;
        } else if (choice == 3) {
            uint64_t n;
            safe_input(n, _("输入数: ", "Enter number: "));
            string saved_output = g_output_file;
            g_output_file.clear();
            run_check(n);
            g_output_file = saved_output;
            continue;
        } else if (choice == 4) {
            long long n;
            safe_input(n, _("n: ", "n: "));
            if (n <= 0) {
                cout << _("请输入正整数", "Enter positive integer") << endl;
                continue;
            }
            if (n > 100000000LL) {
                if (!confirm_action(_("n 较大，继续? (y/n): ", "Large n, continue? (y/n): "))) {
                    continue;
                }
            }
            string saved_output = g_output_file;
            g_output_file.clear();
            run_nth(n);
            g_output_file = saved_output;
            continue;
        } else if (choice == 5) {
            string saved_output = g_output_file;
            g_output_file.clear();
            run_performance();
            g_output_file = saved_output;
            continue;
        } else {
            cout << _("无效选择", "Invalid choice") << endl;
        }
    }
}

// ============================================================================
//  命令行参数解析（手动实现，兼容 POSIX 风格）
// ============================================================================

void print_usage(const char* prog) {
    cout << _("用法: ", "Usage: ") << prog << _(" [选项]\n",
            " [options]\n")
         << _("无参数时进入交互模式。\n\n",
              "No arguments enters interactive mode.\n\n")
         << _("选项:\n", "Options:\n")
         << _("  -h, --help                 显示此帮助\n",
              "  -h, --help                 Show this help\n")
         << _("  -l, --language zh|en      设置语言 (中文/英文)\n",
              "  -l, --language zh|en      Set language (Chinese/English)\n")
         << _("  -r, --range LOW HIGH      查找区间 [LOW, HIGH] 内的素数\n",
              "  -r, --range LOW HIGH      Find primes in range [LOW, HIGH]\n")
         << _("  -d, --digits N            查找 N 位数范围内的素数\n",
              "  -d, --digits N            Find primes with N digits\n")
         << _("  -c, --check N             检查 N 是否为素数\n",
              "  -c, --check N             Check if N is prime\n")
         << _("  -n, --nth N               查找第 N 个素数\n",
              "  -n, --nth N               Find the N-th prime\n")
         << _("  -p, --perf                运行性能测试 (2~10^7)\n",
              "  -p, --perf                Run performance test (2~10^7)\n")
         << _("  -v, --verify              运行内置正确性自检 (ctest 用)\n",
              "  -v, --verify              Run built-in correctness self-test (for ctest)\n")
         << _("  -o, --output FILE         将结果输出到文件 (默认标准输出)\n",
              "  -o, --output FILE         Write results to FILE (default stdout)\n")
         << _("  -q, --quiet               安静模式 (仅显示统计信息)\n",
              "  -q, --quiet               Quiet mode (only statistics)\n")
         << _("  -i, --interactive         强制进入交互模式 (即使有参数)\n",
              "  -i, --interactive         Force interactive mode (even with args)\n")
         << endl;
}

bool parse_arguments(int argc, char* argv[]) {
    if (argc == 1) {
        g_interactive_mode = true;
        return true;
    }

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return false;  // 直接退出
        } else if (arg == "-i" || arg == "--interactive") {
            g_interactive_mode = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_quiet = true;
        } else if (arg == "-l" || arg == "--language") {
            if (i + 1 < argc) {
                string lang = argv[++i];
                if (lang == "zh" || lang == "zh_CN") g_lang = LANG_CHINESE;
                else if (lang == "en" || lang == "en_US") g_lang = LANG_ENGLISH;
                else {
                    cerr << _("未知语言: ", "Unknown language: ") << lang << endl;
                    return false;
                }
            } else {
                cerr << _("选项 -l 需要参数", "Option -l requires an argument") << endl;
                return false;
            }
        } else if (arg == "-r" || arg == "--range") {
            if (i + 2 < argc) {
                g_action = ACTION_RANGE;
                try {
                    g_low = stoll(argv[++i]);
                    g_high = stoll(argv[++i]);
                } catch (...) {
                    cerr << _("无效的数字参数", "Invalid numeric argument") << endl;
                    return false;
                }
            } else {
                cerr << _("选项 -r 需要两个参数", "Option -r requires two arguments") << endl;
                return false;
            }
        } else if (arg == "-d" || arg == "--digits") {
            if (i + 1 < argc) {
                g_action = ACTION_DIGITS;
                try {
                    g_digits = stoi(argv[++i]);
                } catch (...) {
                    cerr << _("无效的数字参数", "Invalid numeric argument") << endl;
                    return false;
                }
            } else {
                cerr << _("选项 -d 需要一个参数", "Option -d requires an argument") << endl;
                return false;
            }
        } else if (arg == "-c" || arg == "--check") {
            if (i + 1 < argc) {
                g_action = ACTION_CHECK;
                try {
                    g_check_num = stoull(argv[++i]);
                } catch (...) {
                    cerr << _("无效的数字参数", "Invalid numeric argument") << endl;
                    return false;
                }
            } else {
                cerr << _("选项 -c 需要一个参数", "Option -c requires an argument") << endl;
                return false;
            }
        } else if (arg == "-n" || arg == "--nth") {
            if (i + 1 < argc) {
                g_action = ACTION_NTH;
                try {
                    g_nth = stoll(argv[++i]);
                } catch (...) {
                    cerr << _("无效的数字参数", "Invalid numeric argument") << endl;
                    return false;
                }
            } else {
                cerr << _("选项 -n 需要一个参数", "Option -n requires an argument") << endl;
                return false;
            }
        } else if (arg == "-p" || arg == "--perf") {
            g_action = ACTION_PERF;
        } else if (arg == "-v" || arg == "--verify") {
            g_action = ACTION_VERIFY;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                g_output_file = argv[++i];
            } else {
                cerr << _("选项 -o 需要一个参数", "Option -o requires an argument") << endl;
                return false;
            }
        } else {
            cerr << _("未知选项: ", "Unknown option: ") << arg << endl;
            print_usage(argv[0]);
            return false;
        }
    }

    if (g_interactive_mode) {
        return true;
    }

    if (g_action == ACTION_NONE) {
        cerr << _("未指定任何操作，使用 -h 查看帮助。", "No action specified, use -h for help.") << endl;
        return false;
    }

    return true;
}

// ============================================================================
//  主函数
// ============================================================================

int main(int argc, char* argv[]) {
    if (!parse_arguments(argc, argv)) {
        return 1;
    }

    if (g_interactive_mode) {
        run_interactive();
        return 0;
    }

    switch (g_action) {
        case ACTION_RANGE:
            run_range(g_low, g_high);
            break;
        case ACTION_DIGITS:
            run_digits(g_digits);
            break;
        case ACTION_CHECK:
            run_check(g_check_num);
            break;
        case ACTION_NTH:
            run_nth(g_nth);
            break;
        case ACTION_PERF:
            run_performance();
            break;
        case ACTION_VERIFY:
            return run_verify();
        default:
            cerr << _("未知操作", "Unknown action") << endl;
            return 1;
    }

    return 0;
}