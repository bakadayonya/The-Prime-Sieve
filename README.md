# 超级素数筛 (Super Prime Sieve) v8.0

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![OpenMP](https://img.shields.io/badge/OpenMP-Supported-green.svg)](https://www.openmp.org/)

高性能素数筛法实现，支持 **2 ~ 10^18** 范围的素数查找。v8.0 引入真正的**模 30 轮子分段筛**，
修复了此前分段筛漏标 3 的倍数、以及并行结果乱序的问题，性能与正确性同步提升。

## ✨ 特性

- 🚀 **模 30 轮子分段筛**：只处理与 30 互质的数（8 个剩余类 {1,7,11,13,17,19,23,29}），位图仅占 8/30 ≈ 26.7%，2、3、5 的倍数天然不进位图，无需标记
- ⚡ **并行计算**：基于 OpenMP 的多线程并行筛法，线程局部位图复用；段间用 **k 路归并**保证全局有序
- 🎯 **混合筛法**：大数稀疏区间自动切换「小素数预筛 + 确定性 Miller-Rabin」，无需生成到 √high 的全部基础素数
- 🧠 **计数专用路径**：`prime_count` 只计数不建向量，二分查找第 n 个素数更省内存
- 🌍 **双语支持**：中文/英文界面，交互与命令行均可切换
- 🔍 **多功能**：按位数查找、自定义区间、单个大数素性检测、第 n 个素数、性能基准
- 📊 **详细统计**：素数总数、最小/最大素数、密度、耗时；支持输出到文件

## 📊 性能

*测试设备：8 核 AArch64（8GB 内存），-O3 -march=native，默认线程数。数值随机器与编译器略有浮动。*

| 测试场景 | 耗时 | 说明 |
|---------|------|------|
| 2 ~ 10^7 | ~0.12 秒 | 全部素数 |
| 2 ~ 10^8 | ~0.6 秒 | 全部素数 |
| 2 ~ 5×10^8 | ~2.9 秒 | 全部素数 |
| 第 10^7 个素数 | ~9.6 秒 | 即 179424673 |
| 混合筛 [10^12, 10^12+5×10^6] | ~0.05 秒 | 稀疏大数区间 |

## 🛠️ 编译

### 依赖
- C++17 兼容编译器 (GCC 7+, Clang 6+, MSVC 2019+)
- OpenMP (可选，但推荐启用)

### 直接编译
```bash
# GCC
g++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp

# Clang
clang++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp
```

### 使用 CMake（推荐）
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest            # 运行内置正确性测试
```

## 🎮 使用

### 命令行模式

```bash
# 查找区间 [LOW, HIGH] 内的素数
./prime_sieve -r LOW HIGH

# 查找 N 位数范围内的所有素数
./prime_sieve -d N

# 检查 N 是否为素数
./prime_sieve -c N

# 查找第 N 个素数
./prime_sieve -n N

# 运行性能基准测试 (2 ~ 10^7)
./prime_sieve -p

# 输出到文件
./prime_sieve -r 1 1000 -o primes.txt

# 切换语言（默认中文）
./prime_sieve -l en -n 100

# 安静模式（仅统计信息）；强制交互模式
./prime_sieve -q -r 1 1000
./prime_sieve -i

# 帮助
./prime_sieve -h
```

完整选项：

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助 |
| `-l, --language zh\|en` | 设置语言 |
| `-r, --range LOW HIGH` | 查找区间内素数 |
| `-d, --digits N` | 查找 N 位数素数 |
| `-c, --check N` | 检查单个数 |
| `-n, --nth N` | 查找第 N 个素数 |
| `-p, --perf` | 性能测试 |
| `-o, --output FILE` | 结果写入文件 |
| `-q, --quiet` | 仅显示统计信息 |
| `-i, --interactive` | 强制交互模式 |

### 交互式模式

```bash
./prime_sieve
```

按菜单选择：

1. 按位数查找
2. 自定义区间
3. 检查单个大数
4. 查找第 n 个素数
5. 性能测试
6. 切换语言
7. 退出

## 🧠 算法详解（v8.0）

### 1. 模 30 轮子分段筛

- 候选集合 = 与 30 互质的数，每 30 个数只存 8 位，位图约为"全奇数"的 1/3.75
- 每个素数按 8 个剩余类分别标记：对剩余类 r，倍数 `n = p·m` 满足 `m ≡ r·p⁻¹ (mod 30)`，
  位图下标以固定步长 `8p` 前进，无需除法
- 因为 2、3、5 的倍数不在位图中，只需标记素数 p ≥ 7 的倍数，天然修复了此前"漏标 3 的倍数导致结果含合数"的 bug

### 2. 有序并行合并

- 各线程处理不连续的段，段内结果有序
- 使用 k 路归并（最小堆）把各线程有序结果合并为全局有序，替代此前按线程 id 直接拼接导致的乱序

### 3. 混合筛法 (Hybrid Sieve)

- 当 `high > 10^11`，或 `high > 10^10` 且区间很短时启用
- 流程：小素数预筛 → 候选数用 `is_prime_mr_fast` 并行 Miller-Rabin 验证
- 适合大数区间的稀疏素数查找，避免生成到 √high 的庞大基础素数表

### 4. 线程局部位图复用

- `thread_local` 缓存位图，每次分段仅需 `fill` 重置，避免重复分配

### 5. 第 n 个素数

- Rosser–Schoenfeld 估计初始上下界，再二分 `prime_count`
- `prime_count` 走只计数路径（不构造结果向量），显著降低反复全量筛分的开销

### 6. 确定性 Miller-Rabin

- n < 3.47×10^12 用 7 个基底 {2,3,5,7,11,13,17}
- 其余 64 位整数用 12 个基底 {2,3,5,7,11,13,17,19,23,29,31,37}
- 数学保证：通过测试的数必定为素数（确定性）

## 📁 项目结构

```
.
├── prime_sieve.cpp      # 主程序源码 (v8.0)
├── CMakeLists.txt       # CMake 构建配置（含正确性测试）
├── LICENSE              # MIT 许可证
└── README.md            # 本文件
```

## 🔧 配置参数

在 `prime_sieve.cpp` 开头可调整：

```cpp
const int MR_BASE_COUNT_MAX = 12;          // 大数 Miller-Rabin 基底数
const int MR_BASE_COUNT_SMALL = 7;         // 小数 Miller-Rabin 基底数
const long long SMALL_PRIME_LIMIT_MIN = 2000;   // 混合筛最小预筛上限
const long long SMALL_PRIME_LIMIT_MAX = 50000;  // 混合筛最大预筛上限
const long long HYBRID_THRESHOLD = 10'000'000'000LL;      // 混合筛触发阈值
const long long LARGE_RANGE_MR_THRESHOLD = 100'000'000'000LL;
const long long SEG_SIZE = 4 * 1024 * 1024;               // 分段区间宽度
```

## ⚠️ 注意事项

- 大数范围：支持到 10^18，但区间过大（>10^9）耗时与内存可能很大，非交互模式会拒绝 >10^9 的区间
- 内存使用：模 30 位图约 `SEG_SIZE * 8 / 30` 位/线程（约 136 KB）
- 线程数：默认使用所有 CPU 核心，可通过 `export OMP_NUM_THREADS=N` 调整

## 🔄 版本演进

- ✅ 奇数位图 → 真正的模 30 轮子位图（更小更快，修复漏标 3 倍数）
- ✅ 并行按线程拼接 → k 路归并（保证输出有序）
- ✅ 每段分配位图 → 线程局部位图复用
- ✅ 计数路径收集向量 → 只计数（省内存）
- ✅ 完整 MR → 快速 MR（预筛后使用）
- ✅ 命令行参数、输出到文件

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE)

---

⭐ 如果这个项目对你有帮助，请给个 Star！
