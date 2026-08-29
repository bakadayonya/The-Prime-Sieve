# 超级素数筛 (Super Prime Sieve) v6.0

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![OpenMP](https://img.shields.io/badge/OpenMP-Supported-green.svg)](https://www.openmp.org/)

高性能素数筛法实现，支持 **2 ~ 10^18** 范围的素数查找。v6.0 引入动态分段、混合筛法和更精确的上界估计，性能大幅提升。

## ✨ 特性

- 🚀 **混合算法**：分段埃氏筛 + 确定性Miller-Rabin素性测试，自动切换
- ⚡ **并行计算**：基于OpenMP的多线程并行筛法，线程局部位图复用
- 🎯 **智能优化**：
  - 动态分段大小（感知L3缓存，自动调整）
  - 混合筛法（小素数预筛 + MR快速验证）
  - 位图存储，缓存友好
  - 无排序并行合并（线程区间连续，直接拼接）
- 🌍 **双语支持**：中文/英文界面，自动切换
- 🔍 **多功能**：
  - 按位数查找素数
  - 自定义区间筛选
  - 单个大数素性检测（含快速MR版本）
  - 查找第n个素数（Rosser–Schoenfeld精确上界）
  - 性能基准测试
- 📊 **详细统计**：素数总数、密度、耗时等

## 📊 性能

| 测试场景 | 耗时 | 说明 |
|---------|------|------|
| 2 ~ 10^7 | ~0.06秒 | 全部素数 |
| 2 ~ 10^8 | ~0.3秒 | 全部素数 |
| 第 10^7 个素数 | ~0.7秒 | 即 179424673 |

*测试设备内存仅8GB且为AArch64

## 🛠️ 编译

### 依赖
- C++17 兼容编译器 (GCC 7+, Clang 6+, MSVC 2019+)
- OpenMP (可选，但推荐启用)

### Linux / macOS
```bash
# 使用GCC
g++ -O3 -march=native -flto -funroll-loops -fopenmp -o prime_sieve prime_sieve.cpp -lm

# 使用Clang
clang++ -O3 -march=native -flto=thin -funroll-loops -fopenmp=libomp -o prime_sieve prime_sieve.cpp -lm
```

(作者我使用的clang++-21)

Windows (MinGW)

```bash
g++ -O3 -march=native -flto -funroll-loops -fopenmp -o prime_sieve.exe prime_sieve.cpp -lm
```

使用CMake

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

🎮 使用

交互式模式

```bash
./prime_sieve
```

启动后根据菜单选择功能：

1. 按位数查找 - 查找指定位数的所有素数
2. 自定义区间 - 指定 [low, high] 范围筛选
3. 单个数检测 - 快速判断大数是否为素数
4. 第n个素数 - 查找第 n 个素数（使用精确上界估计）
5. 性能测试 - 运行基准测试 (2~10^7)

示例

```bash
# 查找 1~100 的素数
请选择模式: 2
请输入区间下限: 1
请输入区间上限: 100

找到 25 个素数：
2   3   5   7   11  13  17  19  23  29
31  37  41  43  47  53  59  61  67  71
73  79  83  89  97
```

🧠 算法详解（v6.0 新特性）

1. 动态分段大小

· 根据 L3 缓存大小（默认 8MB）和线程数动态计算
· 公式：seg_size = (L3_cache / threads * 0.7) * 16
· 范围：100,000 ~ 10,000,000，自动对齐到4的倍数

2. 混合筛法 (Hybrid Sieve)

· 当区间 > 1e12 且区间长度 < 1e6 时启用
· 步骤：
  1. 用前200个小素数预筛，快速排除合数
  2. 候选数使用 is_prime_mr_fast 并行验证（跳过小素数检查）
· 适合大数区间的稀疏素数查找

3. 线程局部位图复用

· 使用 thread_local 缓存位图，避免重复分配
· 每个分段仅需 fill 重置，大幅减少内存分配开销

4. 无排序并行合并

· 线程分配连续区间，且内部有序
· 直接拼接结果，无需 sort()，O(n log n) → O(n)

5. 第n个素数精确上界

· 对 n ≥ 7022 使用 Rosser–Schoenfeld 公式：
  · upper_bound = n * (logn + loglogn - 0.9385)
  · 更紧凑，减少上界扩展次数

6. 确定性 Miller-Rabin

· 对64位整数使用12个固定基底 {2,3,5,7,11,13,17,19,23,29,31,37}
· 数学保证：通过测试的数必定为素数
· v6.0 拆分为 is_prime_mr（完整版）和 is_prime_mr_fast（跳过预筛）

📁 项目结构

```
.
├── prime_sieve.cpp      # 主程序源码 (v6.0)
├── CMakeLists.txt       # CMake构建配置
├── LICENSE              # MIT许可证
└── README.md           # 本文件
```

🔧 配置参数（v6.0）

在源码开头可调整：

```cpp
const int MR_BASE_COUNT = 12;                    // Miller-Rabin基底数
const long long SMALL_PRIME_LIMIT = 2000;        // 预筛小素数范围
const size_t SMALL_PRIME_COUNT = 200;            // 预筛使用前N个素数
const long long HYBRID_THRESHOLD = 1'000'000'000'000LL;  // 混合筛触发阈值
const long long LARGE_RANGE_MR_THRESHOLD = 100'000'000'000LL;
```

⚠️ 注意事项

· 大数范围：支持到 10^18，但区间过大（>10^9）可能耗时很长
· 内存使用：动态分段，单线程位图约占用 seg_size / 2 字节（~0.05~5MB）
· 线程数：默认使用所有CPU核心，可通过 export OMP_NUM_THREADS=N 调整
· L3缓存：如系统L3缓存不是8MB，可修改 get_dynamic_segment_size() 中的值

🔄 版本演进

主要改进

· ✅ 固定分段 → 动态分段（L3感知）
· ✅ 每段分配位图 → 线程局部位图复用
· ✅ 并行后排序 → 无排序直接拼接
· ✅ 简单上界 → Rosser–Schoenfeld 精确上界
· ✅ 完整MR → 快速MR（预筛后使用）
· ❌ 移除轮式筛法（mod 30）→ 避免取模开销

🤝 贡献

欢迎提交 Issue 和 Pull Request！

开发计划

☐ 支持命令行参数（非交互模式）
☐ 输出格式选项（JSON, CSV）
☐ Baillie-PSW 素性测试
☐ 支持 128位整数
☐ 自适应线程数调整

📄 许可证

本项目采用 MIT 许可证 - 详见 LICENSE 文件

📧 联系方式

如有问题或建议，请提交 Issue 或联系 [Your Email]

---

⭐ 如果这个项目对你有帮助，请给个 Star！
