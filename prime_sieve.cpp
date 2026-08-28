#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <cstring>
#include <omp.h>
#include <limits>

using namespace std;
using namespace std::chrono;

// ==================== 可调参数 ====================
const int MR_BASE_COUNT = 12;
const uint64_t MR_BASES[MR_BASE_COUNT] = {2,3,5,7,11,13,17,19,23,29,31,37};
const long long SMALL_PRIME_LIMIT = 2000;
const size_t SMALL_PRIME_COUNT = 200;
const long long HYBRID_THRESHOLD = 1'000'000'000'000LL;
const long long LARGE_RANGE_MR_THRESHOLD = 100'000'000'000LL;

// ==================== 全局状态 ====================
vector<long long> global_base_primes;
mutex base_primes_mutex;

enum Language { CHINESE, ENGLISH };
Language current_language = CHINESE;

inline string _(const string& zh, const string& en) {
    return (current_language == CHINESE) ? zh : en;
}

// ==================== 安全输入函数 ====================
template<typename T>
bool safe_input(T& value, const string& prompt = "") {
    while (true) {
        if (!prompt.empty()) cout << prompt;
        cin >> value;
        if (cin.fail()) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << _("输入无效，请输入一个整数。", "Invalid input, please enter an integer.") << endl;
        } else {
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // 清空多余字符
            return true;
        }
    }
}

// 专门用于读取字符确认（y/n）
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
        if (input.size() == 1 && (input[0] == 'y' || input[0] == 'Y' || input[0] == 'n' || input[0] == 'N'))
            return (input[0] == 'y' || input[0] == 'Y');
        cout << _("请输入 'y' 或 'n'。", "Please enter 'y' or 'n'.") << endl;
    }
}

// ==================== 工具：模幂 & Miller-Rabin ====================
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
    while ((d & 1) == 0) { d >>= 1; ++s; }
    for (int i = 0; i < MR_BASE_COUNT; ++i) {
        uint64_t a = MR_BASES[i];
        if (a >= n) continue;
        uint64_t x = mod_pow(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (int r = 1; r < s; ++r) {
            x = (uint64_t)((__int128)x * x % n);
            if (x == n - 1) { composite = false; break; }
        }
        if (composite) return false;
    }
    return true;
}

bool is_prime_mr(uint64_t n) {
    if (n < 2) return false;
    static const uint64_t small[] = {2,3,5,7,11,13,17,19,23,29,31,37};
    for (uint64_t p : small) {
        if (n % p == 0) return n == p;
    }
    return is_prime_mr_fast(n);
}

// ==================== 简单筛（位图） ====================
vector<long long> simple_sieve_odd(long long limit) {
    vector<long long> primes;
    if (limit < 2) return primes;
    primes.push_back(2);
    if (limit == 2) return primes;
    long long odd_cnt = (limit - 1) / 2;
    size_t words = (odd_cnt + 63) / 64;
    vector<uint64_t> bits(words, ~0ULL);
    long long sqrt_lim = (long long)sqrt(limit);
    for (long long i = 1; 2*i+1 <= sqrt_lim; ++i) {
        if (bits[i>>6] & (1ULL << (i&63))) {
            long long p = 2*i+1;
            long long start = p*p;
            long long start_idx = (start - 1) / 2;
            for (long long j = start_idx; j < odd_cnt; j += p)
                bits[j>>6] &= ~(1ULL << (j&63));
        }
    }
    for (long long i = 1; i <= odd_cnt; ++i)
        if (bits[i>>6] & (1ULL << (i&63)))
            primes.push_back(2*i+1);
    return primes;
}

void ensure_base_primes(long long limit) {
    lock_guard<mutex> lock(base_primes_mutex);
    long long needed = (long long)sqrt(limit) + 1;
    if (!global_base_primes.empty() && global_base_primes.back() * global_base_primes.back() >= limit)
        return;
    global_base_primes = simple_sieve_odd(needed);
}

// ==================== 分段筛核心（优化版） ====================
inline long long get_dynamic_segment_size() {
    static long long seg_size = 0;
    if (seg_size == 0) {
        long long l3_cache = 8 * 1024 * 1024;
        int threads = omp_get_max_threads();
        long long bytes_per_seg = (long long)(l3_cache / max(1, threads) * 0.7);
        seg_size = max(100000LL, min(10000000LL, bytes_per_seg * 8 * 2));
        seg_size = (seg_size / 4) * 4;
    }
    return seg_size;
}

void segmented_sieve_append(long long low, long long high,
                            const vector<long long>& base_primes,
                            vector<long long>& out_primes) {
    if (low > high || high < 2) return;
    if (low < 2) low = 2;
    if (low <= 2 && high >= 2) out_primes.push_back(2);

    long long first_odd = (low % 2 == 0) ? low + 1 : low;
    if (first_odd > high) return;

    long long odd_count = (high - first_odd) / 2 + 1;
    size_t words = (odd_count + 63) / 64;

    static thread_local vector<uint64_t> bits;
    if (bits.size() < words) bits.resize(words);
    fill(bits.begin(), bits.begin() + words, ~0ULL);

    for (long long p : base_primes) {
        if (p == 2) continue;
        __int128 p2 = (__int128)p * p;
        if (p2 > high) break;

        long long first = ((low + p - 1) / p) * p;
        if ((first & 1) == 0) first += p;
        if (first < (long long)p2) first = (long long)p2;
        if ((first & 1) == 0) first += p;

        long long idx = (first - first_odd) / 2;
        if (idx < 0) idx = 0;

        for (long long j = idx; j < odd_count; j += p)
            bits[j>>6] &= ~(1ULL << (j&63));
    }

    long long num = first_odd;
    for (long long i = 0; i < odd_count; ++i, num += 2)
        if (bits[i>>6] & (1ULL << (i&63)))
            out_primes.push_back(num);
}

vector<long long> segmented_sieve(long long low, long long high,
                                  const vector<long long>& base_primes) {
    vector<long long> result;
    segmented_sieve_append(low, high, base_primes, result);
    return result;
}

// ==================== 混合筛（预筛 + 并行 MR） ====================
vector<long long> hybrid_sieve(long long low, long long high) {
    vector<long long> primes;
    if (low <= 2 && high >= 2) primes.push_back(2);
    long long first_odd = (low % 2 == 0) ? low + 1 : low;
    if (first_odd > high) return primes;

    static const vector<long long> small_primes = [] {
        auto sp = simple_sieve_odd(SMALL_PRIME_LIMIT);
        if (sp.size() > SMALL_PRIME_COUNT) sp.resize(SMALL_PRIME_COUNT);
        return sp;
    }();

    long long count = (high - first_odd) / 2 + 1;
    size_t words = (count + 63) / 64;
    vector<uint64_t> maybe_prime(words, ~0ULL);

    auto clear_bit = [&](long long idx) {
        maybe_prime[idx>>6] &= ~(1ULL << (idx&63));
    };
    auto is_set = [&](long long idx) -> bool {
        return (maybe_prime[idx>>6] & (1ULL << (idx&63))) != 0;
    };

    for (long long p : small_primes) {
        if (p == 2) continue;
        long long first = ((first_odd + p - 1) / p) * p;
        if ((first & 1) == 0) first += p;
        long long idx = (first - first_odd) / 2;
        for (long long j = idx; j < count; j += p)
            clear_bit(j);
    }

    vector<long long> candidates;
    candidates.reserve(count);
    for (long long i = 0; i < count; ++i)
        if (is_set(i)) candidates.push_back(first_odd + 2*i);

    vector<char> is_prime(candidates.size(), 0);
    #pragma omp parallel for schedule(dynamic, 1024)
    for (long long i = 0; i < (long long)candidates.size(); ++i)
        if (is_prime_mr_fast(candidates[i]))
            is_prime[i] = 1;

    for (size_t i = 0; i < candidates.size(); ++i)
        if (is_prime[i]) primes.push_back(candidates[i]);

    return primes;
}

// ==================== 并行大区间分段筛 ====================
vector<long long> segmented_sieve_large(long long low, long long high) {
    if (low < 2) low = 2;
    if (low > high) return {};

    if ((high - low < 1'000'000LL && high > HYBRID_THRESHOLD) || high > LARGE_RANGE_MR_THRESHOLD)
        return hybrid_sieve(low, high);

    ensure_base_primes(high);

    int nthreads = omp_get_max_threads();
    long long seg_size = get_dynamic_segment_size();
    vector<vector<long long>> thread_results(nthreads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        long long range_len = high - low + 1;
        long long chunk = (range_len + nthreads - 1) / nthreads;
        long long t_low = low + tid * chunk;
        long long t_high = min(t_low + chunk - 1, high);
        if (t_low <= t_high) {
            auto& local = thread_results[tid];
            local.reserve((size_t)((t_high - t_low + 1) / log(t_high + 1) * 1.2) + 100);
            long long seg_low = t_low;
            while (seg_low <= t_high) {
                long long seg_high = min(seg_low + seg_size - 1, t_high);
                segmented_sieve_append(seg_low, seg_high, global_base_primes, local);
                seg_low = seg_high + 1;
            }
        }
    }

    size_t total = 0;
    for (auto& v : thread_results) total += v.size();
    vector<long long> all_primes;
    all_primes.reserve(total);
    for (auto& v : thread_results)
        all_primes.insert(all_primes.end(), v.begin(), v.end());
    return all_primes;
}

// ==================== 查找第 n 个素数 ====================
long long find_nth_prime(long long n) {
    if (n <= 0) return -1;
    if (n == 1) return 2;

    double logn = log((double)n);
    double loglogn = log(logn);
    double approx = n * (logn + loglogn);
    long long upper_bound;
    if (n >= 7022) {
        upper_bound = (long long)(n * (logn + loglogn - 0.9385)) + 1000;
    } else {
        upper_bound = (long long)(approx * 1.2) + 1000;
    }
    upper_bound = max(upper_bound, 100LL);

    ensure_base_primes(upper_bound);
    long long count = 1;
    long long low = 3;
    long long seg_size = get_dynamic_segment_size();

    while (low <= upper_bound) {
        long long high = min(low + seg_size - 1, upper_bound);
        vector<long long> seg_primes;
        segmented_sieve_append(low, high, global_base_primes, seg_primes);
        for (long long p : seg_primes) {
            if (++count == n) return p;
        }
        low = high + 1;
        if (low > upper_bound && count < n) {
            upper_bound = (long long)(upper_bound * 1.5) + 1000000;
            ensure_base_primes(upper_bound);
        }
    }
    return -1;
}

// ==================== 辅助功能 ====================
pair<long long, long long> get_range_by_digits(int digits) {
    if (digits <= 0 || digits > 18) return {0,0};
    long long low = 1;
    for (int i=1; i<digits; ++i) low *= 10;
    long long high = low * 10 - 1;
    if (digits == 1) low = 2;
    return {low, high};
}

void print_primes(const vector<long long>& primes, int max_display=200) {
    if (primes.empty()) {
        cout << _("该区间没有素数", "No primes in this range") << endl;
        return;
    }
    cout << _("\n找到 ", "\nFound ") << primes.size() << _(" 个素数", " primes");
    if (primes.size() > max_display)
        cout << _("（只显示前 ", " (showing first ") << max_display << _(" 个）", ")") << endl;
    else
        cout << _("：", ":") << endl;
    cout << "----------------------------------------" << endl;
    int cnt = 0;
    for (size_t i=0; i<min(primes.size(), (size_t)max_display); ++i) {
        cout << primes[i];
        if (++cnt % 10 == 0) cout << '\n';
        else cout << '\t';
    }
    if (cnt % 10 != 0) cout << '\n';
    cout << "----------------------------------------" << endl;
}

void show_stats(const vector<long long>& primes, double elapsed) {
    if (primes.empty()) return;
    cout << _("\n========== 统计信息 ==========", "\n========== Statistics ==========") << endl;
    cout << _("素数总数: ", "Total primes: ") << primes.size() << endl;
    cout << _("最小素数: ", "Smallest prime: ") << primes.front() << endl;
    cout << _("最大素数: ", "Largest prime: ") << primes.back() << endl;
    cout << _("耗时: ", "Time: ") << fixed << setprecision(3) << elapsed << _(" 秒", " seconds") << endl;
    if (primes.size() > 1) {
        double density = (double)primes.size() / (primes.back() - primes.front()) * 100;
        cout << _("素数密度: ", "Prime density: ") << fixed << setprecision(4) << density << "%" << endl;
    }
    cout << _("================================", "====================================") << endl;
}

// ==================== 主菜单 ====================
int main() {
    cout << "Please select language / 请选择语言:\n";
    cout << "1. 中文\n2. English\nEnter choice: ";
    int lang_choice;
    safe_input(lang_choice);
    current_language = (lang_choice == 2) ? ENGLISH : CHINESE;

    cout << _("========== 超级素数筛（优化版 v6.1） ==========",
              "========== Super Prime Sieve (Optimized v6.1) ==========") << endl;
    cout << _("支持范围: 2 ~ 10^18（更大可能极慢）",
              "Supported: 2 ~ 10^18 (larger may be slow)") << endl;
    cout << _("自动切换分段筛 / 混合筛（OpenMP + 动态分段 + 无排序合并）",
              "Auto-select segmented/hybrid sieve (OpenMP + dynamic seg + no sort)") << endl << endl;

    while (true) {
        cout << _("\n请选择模式：", "\nSelect mode:") << endl;
        cout << _("1. 按位数查找", "1. Search by digits") << endl;
        cout << _("2. 自定义区间", "2. Custom range") << endl;
        cout << _("3. 检查单数", "3. Check a single number") << endl;
        cout << _("4. 查找第 n 个素数", "4. Find nth prime") << endl;
        cout << _("5. 性能测试 (2~10^7)", "5. Performance test (2~10^7)") << endl;
        cout << _("6. 退出", "6. Exit") << endl;

        int choice;
        safe_input(choice, _("输入 (1-6): ", "Enter (1-6): "));

        if (choice == 6) {
            cout << _("再见！", "Goodbye!") << endl;
            break;
        }

        long long low, high;
        if (choice == 1) {
            int digits;
            safe_input(digits, _("位数 (1-18): ", "Digits (1-18): "));
            auto range = get_range_by_digits(digits);
            low = range.first; high = range.second;
            if (low == 0) {
                cout << _("超出范围", "Out of range") << endl;
                continue;
            }
        } else if (choice == 2) {
            safe_input(low, _("下限 low (>=2): ", "low (>=2): "));
            safe_input(high, _("上限 high: ", "high: "));
            if (low < 2 || high < low) {
                cout << _("无效区间", "Invalid range") << endl;
                continue;
            }
        } else if (choice == 3) {
            uint64_t n;
            safe_input(n, _("输入数: ", "Enter number: "));
            auto start = steady_clock::now();
            bool prime = is_prime_mr(n);
            auto end = steady_clock::now();
            double elapsed = duration<double>(end - start).count();
            cout << n << (prime ? _(" 是素数", " is prime") : _(" 是合数", " is composite")) << endl;
            cout << _("耗时: ", "Time: ") << elapsed << _(" 秒", " seconds") << endl;
            continue;
        } else if (choice == 4) {
            long long n;
            safe_input(n, _("n: ", "n: "));
            if (n <= 0) {
                cout << _("请输入正整数", "Enter positive integer") << endl;
                continue;
            }
            if (n > 100000000LL) {
                if (!confirm_action(_("n 较大，继续? (y/n): ", "Large n, continue? (y/n): ")))
                    continue;
            }
            auto start = steady_clock::now();
            long long prime = find_nth_prime(n);
            auto end = steady_clock::now();
            if (prime > 0) {
                cout << _("第 ", "The ") << n << _(" 个素数是: ", "-th prime is: ") << prime << endl;
                cout << _("耗时: ", "Time: ") << duration<double>(end-start).count() << _(" 秒", " seconds") << endl;
            } else {
                cout << _("计算失败", "Failed") << endl;
            }
            continue;
        } else if (choice == 5) {
            low = 2; high = 10000000;
            cout << _("性能测试: 2 ~ 10,000,000", "Performance test: 2 ~ 10,000,000") << endl;
        } else {
            cout << _("无效选择", "Invalid choice") << endl;
            continue;
        }

        long long range_size = high - low + 1;
        if (range_size > 1'000'000'000LL) {
            if (!confirm_action(_("区间过大，继续? (y/n): ", "Range too large, continue? (y/n): ")))
                continue;
        }

        cout << _("\n筛选中...", "\nSieving...") << endl;
        auto start = steady_clock::now();
        vector<long long> primes;
        if (range_size > get_dynamic_segment_size() * 2)
            primes = segmented_sieve_large(low, high);
        else {
            ensure_base_primes(high);
            primes = segmented_sieve(low, high, global_base_primes);
        }
        auto end = steady_clock::now();
        double elapsed = duration<double>(end - start).count();

        cout << _(" 完成！", " Done!") << endl;
        show_stats(primes, elapsed);
        print_primes(primes);
    }
    return 0;
}