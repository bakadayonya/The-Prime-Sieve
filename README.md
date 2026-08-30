# 超级素数筛 (Super Prime Sieve) v9.0

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![OpenMP](https://img.shields.io/badge/OpenMP-Supported-green.svg)](https://www.openmp.org/)

高性能素数筛法实现，支持 **2 ~ 10^18** 范围的素数查找。v8.0 引入真正的**模 30 轮子分段筛**，
修复了此前分段筛漏标 3 的倍数、以及并行结果乱序的问题；**v9.0** 为第 n 个素数加入
**Meissel-Lehmer 素数计数**，把二分每步都重筛全区间改为亚毫秒级的 π(x) 查询，
第 10^7 个素数从约 7 秒降至约 0.2 秒。

> A high-performance prime number sieve supporting the range **2 ~ 10^18**.
> v8.0 introduced a true **mod-30 wheel segmented sieve**, fixing two earlier bugs — multiples of 3
> never being marked as composite, and out-of-order output from the parallel path — while also improving speed.
> **v9.0** adds a **Meissel–Lehmer** prime-count to the nth-prime path: binary-search steps now use
> sub-millisecond π(x) instead of re-sieving the whole range, cutting the 10^7-th prime from ~7 s to ~0.2 s.

## ✨ 特性 / Features

- 🚀 **模 30 轮子分段筛 / Mod-30 wheel sieve**：只处理与 30 互质的数（8 个剩余类 `{1,7,11,13,17,19,23,29}`），
  位图仅占 `8/30 ≈ 26.7%`，2、3、5 的倍数天然不进位图，无需标记。
  Only keeps numbers coprime to 30 (8 residues); the bitmap is just `8/30 ≈ 26.7%` size, and multiples of 2/3/5 never enter the bitmap.
- ⚡ **并行计算 / Parallel computing**：基于 OpenMP 的多线程并行筛法，线程局部位图复用；段间用 **k 路归并**保证全局有序。
  OpenMP multi-threaded sieving with per-thread bitmap reuse; **k-way merge** keeps the global output ordered.
- 🎯 **混合筛法 / Hybrid sieve**：大数稀疏区间自动切换「小素数预筛 + 确定性 Miller-Rabin」，无需生成到 √high 的全部基础素数。
  Auto-switches to "small-prime presieve + deterministic Miller-Rabin" for sparse large ranges, avoiding a huge base-prime table up to √high.
- 🧮 **Meissel–Lehmer 素数计数 / Meissel–Lehmer prime count**：`lehmer_pi` 亚毫秒计算 π(x)（phi 与 π 均记忆化），
  二分求第 n 个素数不再整段重筛，10^7 第素数 ~7s → ~0.2s，10^8 第素数 ~0.5s。
  `lehmer_pi` computes π(x) in sub-millisecond time (both phi and π results are memoized); nth-prime
  binary search no longer re-sieves the whole range — the 10^7-th prime drops from ~7 s to ~0.2 s, the 10^8-th to ~0.5 s.
- 🧠 **计数专用路径 / Counting-only path**：`prime_count` 只计数不建向量，省内存；亦用作 Lehmer 的自检交叉验证。
  `prime_count` counts without building a vector, saving memory; it also serves as the self-test cross-check for Lehmer.
- 🧪 **内置自检 / Built-in self-test**：`-v, --verify` 校验已知 π(x)、Miller-Rabin、分段/混合筛一致性，
  并交叉验证 Lehmer vs 分段筛 π(x) 与已知第 n 个素数，供 `ctest` 使用。
  `-v, --verify` validates known π(x), Miller-Rabin, segmented/hybrid agreement, plus a Lehmer-vs-segmented
  π(x) cross-check and known nth primes; used by `ctest`.
- 🌍 **双语支持 / Bilingual UI**：中文/英文界面，交互与命令行均可切换。
  Chinese/English interface, switchable in both interactive and CLI modes.
- 🔍 **多功能 / Multiple functions**：按位数查找、自定义区间、单个大数素性检测、第 n 个素数、性能基准。
  Search by digit count, custom range, single-number primality check, nth prime, and a performance benchmark.
- 📊 **详细统计 / Detailed stats**：素数总数、最小/最大素数、密度、耗时；支持输出到文件。
  Total/min/max primes, density, elapsed time; results can be written to a file.

## 📊 性能 / Performance

*测试设备：8 核 AArch64（8GB 内存），-O3 -march=native，默认线程数。数值随机器与编译器略有浮动。*
*Tested on: 8-core AArch64 (8GB RAM), -O3 -march=native, default thread count. Values vary by machine and compiler.*

| 测试场景 / Scenario | 耗时 / Time | 说明 / Notes |
|---------|------|------|
| 2 ~ 10^7 | ~0.12 秒 | 全部素数 / all primes |
| 2 ~ 10^8 | ~0.6 秒 | 全部素数 / all primes |
| 2 ~ 5×10^8 | ~2.9 秒 | 全部素数 / all primes |
| 第 10^7 个素数 / 10^7-th prime | **~0.2 秒**（v8.0 ~7 秒 / v8.0+ 优化前）| 即 / i.e. 179424673 |
| 第 10^8 个素数 / 10^8-th prime | **~0.5 秒** | 即 / i.e. 2038074743 |
| 混合筛 [10^12, 10^12+5×10^6] / hybrid | ~0.05 秒 | 稀疏大数区间 / sparse large range |

> 注：上表第 n 个素数为本机（x86_64, GCC 14, -O3 -march=native）实测，含约 0.3 秒的一次性
> Lehmer 基础素数表初始化（后续调用复用缓存，耗时远低于首查）。
> Note: nth-prime times above are measured on this machine (x86_64, GCC 14, -O3 -march=native) and
> include ~0.3 s of one-time Lehmer table init; later calls reuse the memoized cache.

## 🛠️ 编译 / Building

### 依赖 / Dependencies
- C++17 兼容编译器 (GCC 7+, Clang 6+, MSVC 2019+)
- OpenMP（可选，但推荐启用）/ optional but recommended

### 直接编译 / Direct compile
```bash
# GCC
g++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp

# Clang（推荐 clang++-21，自带 omp.h 与 libomp.so）
clang++-21 -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp
```

### 使用 CMake（推荐）/ With CMake (recommended)
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest            # 运行内置正确性测试 / run built-in correctness tests
```

CMake 默认使用系统编译器（通常为 GCC）。如需使用 **clang++-21**（自带 OpenMP 头文件与 libomp），
配置时显式指定编译器即可，`find_package(OpenMP)` 会自动设置正确的 `-fopenmp` 编译/链接选项：

```bash
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-21 ..
make -j$(nproc)
ctest
```

## 🎮 使用 / Usage

### 命令行模式 / Command-line mode

```bash
# 查找区间 [LOW, HIGH] 内的素数 / find primes in [LOW, HIGH]
./prime_sieve -r LOW HIGH

# 查找 N 位数范围内的所有素数 / all primes with N digits
./prime_sieve -d N

# 检查 N 是否为素数 / check whether N is prime
./prime_sieve -c N

# 查找第 N 个素数 / find the N-th prime
./prime_sieve -n N

# 运行性能基准测试 (2 ~ 10^7) / run performance benchmark
./prime_sieve -p

# 运行内置正确性自检 / run built-in correctness self-test
./prime_sieve -v

# 输出到文件 / write results to a file
./prime_sieve -r 1 1000 -o primes.txt

# 切换语言（默认中文）/ switch language (default: Chinese)
./prime_sieve -l en -n 100

# 安静模式（仅统计信息）/ quiet mode (statistics only); 强制交互 / force interactive
./prime_sieve -q -r 1 1000
./prime_sieve -i

# 帮助 / help
./prime_sieve -h
```

完整选项 / Full options:

| 选项 / Option | 说明 / Description |
|------|------|
| `-h, --help` | 显示帮助 / show help |
| `-l, --language zh\|en` | 设置语言 / set language |
| `-r, --range LOW HIGH` | 查找区间内素数 / primes in a range |
| `-d, --digits N` | 查找 N 位数素数 / primes with N digits |
| `-c, --check N` | 检查单个数 / check a single number |
| `-n, --nth N` | 查找第 N 个素数 / find the N-th prime |
| `-p, --perf` | 性能测试 / performance test |
| `-v, --verify` | 内置正确性自检 / built-in correctness self-test |
| `-o, --output FILE` | 结果写入文件 / write results to FILE |
| `-q, --quiet` | 仅显示统计信息 / statistics only |
| `-i, --interactive` | 强制交互模式 / force interactive mode |

### 交互式模式 / Interactive mode

```bash
./prime_sieve
```

按菜单选择 / choose from the menu:

1. 按位数查找 / search by digits
2. 自定义区间 / custom range
3. 检查单个大数 / check a single number
4. 查找第 n 个素数 / find the nth prime
5. 性能测试 / performance test
6. 切换语言 / switch language
7. 退出 / exit

## 🧠 算法详解 / Algorithm details

### 1. 模 30 轮子分段筛 / Mod-30 wheel segmented sieve

- 候选集合 = 与 30 互质的数，每 30 个数只存 8 位，位图约为"全奇数"的 1/3.75。
  Candidates are numbers coprime to 30 — 8 bits per 30 numbers, roughly 1/3.75 the size of an all-odd bitmap.
- 每个素数按 8 个剩余类分别标记：对剩余类 r，倍数 `n = p·m` 满足 `m ≡ r·p⁻¹ (mod 30)`，
  位图下标以固定步长 `8p` 前进，无需除法。
  Each prime is marked per residue: for residue r, a multiple `n = p·m` satisfies `m ≡ r·p⁻¹ (mod 30)`,
  so the bitmap index advances by a fixed step `8p` with no division.
- 因为 2、3、5 的倍数不在位图中，只需标记素数 p ≥ 7 的倍数，天然修复了此前"漏标 3 的倍数导致结果含合数"的 bug。
  Since multiples of 2/3/5 aren't in the bitmap, only primes p ≥ 7 need marking —
  which inherently fixes the old "multiples of 3 leaked into results" bug.

### 2. 有序并行合并 / Ordered parallel merge

- 各线程处理不连续的段，段内结果有序；用 k 路归并（最小堆）合并为全局有序。
  Threads handle non-contiguous segments (each internally sorted); a k-way merge (min-heap) yields globally sorted output.
- 替代了此前按线程 id 直接拼接导致的乱序。
  Replaces the old thread-id concatenation that produced unsorted output.

### 3. 混合筛法 / Hybrid sieve

- 仅用于**稀疏大数区间**（区间宽度很小、但 high 很大，例如 `high > 10^10` 且宽度 < 10^6）。
  Used only for **sparse large ranges** (narrow width but large high, e.g. `high > 10^10` and width < 10^6).
- 流程：小素数预筛 → 候选数用 `is_prime_mr_fast` 并行 Miller-Rabin 验证。
  Pipeline: small-prime presieve → parallel Miller-Rabin (`is_prime_mr_fast`) on survivors.
- ⚠️ 混合筛按**区间宽度**判定，宽区间即使 high 很大也走分段筛——因为分段筛内存只取决于段宽、与 high 无关，
  若按 high 无条件切混合筛会把整段位图一次性物化导致内存爆炸（v8.0 已修复）。
  Hybrid is selected by **range width**, not high — wide ranges use the segmented sieve regardless of high,
  since segmented memory depends only on segment width. (A prior version unconditionally switched on high,
  materializing the whole bitmap and crashing with OOM; fixed in v8.0.)

### 4. 线程局部位图复用 / Thread-local bitmap reuse

- `thread_local` 缓存位图，每次分段仅需 `fill` 重置，避免重复分配。
  A `thread_local` bitmap is reused and only `fill`-reset per segment, avoiding repeated allocation.

### 5. 第 n 个素数 / Nth prime

- Rosser–Schoenfeld 估计初始上下界，再二分 `prime_count`。
  Bounds are estimated via Rosser–Schoenfeld, then `prime_count` is binary-searched.
- `prime_count` 走只计数路径（不构造结果向量），显著降低反复全量筛分的开销。
  The counting-only path avoids building result vectors, cutting the cost of repeated full sieving.

### 6. 确定性 Miller-Rabin / Deterministic Miller-Rabin

- n < 3.47×10^12 用 7 个基底 `{2,3,5,7,11,13,17}`（确定性上限实为 3.4×10^14，留有余量）。
  7 bases `{2,3,5,7,11,13,17}` for n < 3.47×10^12 (the proven deterministic bound is 3.4×10^14).
- 其余 64 位整数用 12 个基底 `{2,3,5,7,11,13,17,19,23,29,31,37}`，覆盖全部 64 位。
  12 bases `{2,3,5,7,11,13,17,19,23,29,31,37}` for other 64-bit integers, covering the full range.
- 数学保证：通过测试的数必定为素数（确定性）。
  Mathematically guaranteed deterministic — a pass means the number is definitely prime.

## 📁 项目结构 / Project structure

```
.
├── prime_sieve.cpp      # 主程序源码 (v9.0) / main source
├── CMakeLists.txt       # CMake 构建配置（含正确性测试）/ build config + tests
├── LICENSE              # MIT 许可证 / MIT license
└── README.md            # 本文件 / this file
```

## 🔧 配置参数 / Configuration

在 `prime_sieve.cpp` 开头可调整 / Adjustable at the top of `prime_sieve.cpp`:

```cpp
const int MR_BASE_COUNT_MAX = 12;          // 大数 Miller-Rabin 基底数 / large-number bases
const int MR_BASE_COUNT_SMALL = 7;         // 小数 Miller-Rabin 基底数 / small-number bases
const long long SMALL_PRIME_LIMIT_MIN = 2000;   // 混合筛最小预筛上限 / hybrid min presieve limit
const long long SMALL_PRIME_LIMIT_MAX = 50000;  // 混合筛最大预筛上限 / hybrid max presieve limit
const long long HYBRID_THRESHOLD = 10'000'000'000LL;      // 混合筛触发阈值 / hybrid trigger
const long long SEG_SIZE = 4 * 1024 * 1024;               // 分段区间宽度 / segment width
// Meissel-Lehmer（第 n 个素数用）：
const int LEHMER_SIEVE_LIMIT = 5'000'000;   // Lehmer 前缀表/基础素数表上限
const int LEHMER_PHI_N = 200'000;           // phi 记忆表 x 上限
const int LEHMER_PHI_S = 7;                 // phi 记忆表 s 上限
```

## ⚠️ 注意事项 / Notes

- 大数范围：支持到 10^18，但区间过大（>10^9）耗时与内存可能很大，非交互模式会拒绝 >10^9 的区间。
  Supports up to 10^18, but very large ranges (>10^9) can be slow and memory-hungry; non-interactive mode rejects ranges >10^9.
- 内存使用：模 30 位图约 `SEG_SIZE * 8 / 30` 位/线程（约 136 KB），与 high 无关。
  Memory: the mod-30 bitmap is about `SEG_SIZE * 8 / 30` bits per thread (~136 KB), independent of high.
- 线程数：默认使用所有 CPU 核心，可通过 `export OMP_NUM_THREADS=N` 调整。
  Threads: all CPU cores by default; tune via `export OMP_NUM_THREADS=N`.

## 🔄 版本演进 / Changelog

- ✅ 奇数位图 → 真正的模 30 轮子位图（更小更快，修复漏标 3 倍数）/ odd bitmap → real mod-30 wheel (smaller, faster, fixes the 3-multiple bug)
- ✅ 并行按线程拼接 → k 路归并（保证输出有序）/ per-thread concat → k-way merge (guaranteed sorted output)
- ✅ 每段分配位图 → 线程局部位图复用 / per-segment allocation → thread-local reuse
- ✅ 计数路径收集向量 → 只计数（省内存）/ collect-then-count → counting-only (less memory)
- ✅ 完整 MR → 快速 MR（预筛后使用）/ full MR → fast MR (after presieve)
- ✅ 混合筛按 high 无条件切换 → 按区间宽度切换（修复大区间 OOM）/ hybrid switch by high → by range width (fixes OOM on large ranges)
- ✅ 新增 `-v, --verify` 内置正确性自检（供 ctest 使用）/ added `-v, --verify` built-in correctness self-test (for ctest)
- ✅ 命令行参数、输出到文件 / CLI args, file output
- ✅ **v9.0**：第 n 个素数改走 Meissel–Lehmer π(x)（phi/π 记忆化），二分不再重筛全区间
  第 10^7 素数 ~7s → ~0.2s / **v9.0**: nth-prime uses Meissel–Lehmer π(x) (memoized phi/π);
  binary search no longer re-sieves the whole range — 10^7-th prime ~7s → ~0.2s
- ✅ **v9.0**：去重 `segmented_sieve_append`/`count_primes_range` → 共享 `scan_segment`；
  输出文件样板 → RAII `OutputSink`；删除冗余 `is_prime_mr_hybrid` / **v9.0**: deduplicated the two
  scan loops into shared `scan_segment`; file-output boilerplate → RAII `OutputSink`; removed redundant wrapper

## 📄 许可证 / License

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE)
This project is licensed under the MIT License — see [LICENSE](LICENSE).

---

⭐ 如果这个项目对你有帮助，请给个 Star！/ If this project helped you, please give it a Star!
