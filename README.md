# 超级素数筛 (Super Prime Sieve)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![OpenMP](https://img.shields.io/badge/OpenMP-Supported-green.svg)](https://www.openmp.org/)

高性能素数筛法实现，支持 **2 ~ 10^18** 范围的素数查找。自动选择最优算法，充分利用多核CPU性能。

## ✨ 特性

- 🚀 **混合算法**：分段埃氏筛 + 确定性Miller-Rabin素性测试
- ⚡ **并行计算**：基于OpenMP的多线程并行筛法
- 🎯 **智能优化**：轮式筛法（Wheel Sieve）、位图存储、缓存友好分段
- 🌍 **双语支持**：中文/英文界面，自动切换
- 🔍 **多功能**：
  - 按位数查找素数
  - 自定义区间筛选
  - 单个大数素性检测
  - 查找第n个素数
  - 性能基准测试
- 📊 **详细统计**：素数总数、密度、耗时等

## 📊 性能

| 测试场景 | 耗时 | 说明 |
|---------|------|------|
| 2 ~ 10^7 | ~0.06秒 | 全部素数 |
| 2 ~ 10^8 | ~0.2秒 | 全部素数 |
| 第 10^7 个素数 | ~0.7秒 | 即 1179424673 |

*测试设备内存仅8GB且为AArch64


## 🛠️ 编译

### 依赖
- C++17 兼容编译器 (GCC 7+, Clang 6+, MSVC 2019+)
- OpenMP (可选，但推荐启用)

### Linux / macOS
```bash
# 使用GCC
g++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp

# 使用Clang
clang++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve prime_sieve.cpp
```

### Windows (MinGW)

```bash
g++ -O3 -march=native -fopenmp -std=c++17 -o prime_sieve.exe prime_sieve.cpp
```

### 使用CMake

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```



# 🎮 使用

交互式模式

```bash
./prime_sieve
```

启动后根据菜单选择功能：

1. 按位数查找 - 查找指定位数的所有素数
2. 自定义区间 - 指定 [low, high] 范围筛选
3. 单个数检测 - 快速判断大数是否为素数
4. 第n个素数 - 查找第 n 个素数
5. 性能测试 - 运行基准测试

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

🧠 算法详解

1. 分段埃氏筛 (Segmented Sieve)

· 将大区间分成小块（默认 5,000,000），适应CPU缓存
· 每个分段使用位图（bitmap）存储，内存效率高
· 仅用小于 √high 的素数进行筛选

2. 确定性 Miller-Rabin

· 对 64位整数使用 12 个固定基底
· 数学保证：通过测试的数必定为素数
· 时间复杂度 O(k·log³n)，k为基底数

3. 轮式筛法 (Wheel Sieve)

· 基于模30剩余类 {1,7,11,13,17,19,23,29}
· 跳过 73.3% 的合数，减少标记次数

4. OpenMP 并行

· 大区间按线程数切分，并行处理
· 动态调度（schedule(dynamic)）均衡负载

📁 项目结构

```
.
├── prime_sieve.cpp      # 主程序源码
├── CMakeLists.txt       # CMake构建配置
├── LICENSE              # MIT许可证
└── README.md           # 本文件
```

🔧 配置参数

在源码开头可调整：

```cpp
const long long SEGMENT_SIZE = 5'000'000;  // 分段大小（字节）
const int MR_BASE_COUNT = 12;              // Miller-Rabin基底数
```

⚠️ 注意事项

· 大数范围：虽然支持到 10^18，但区间过大（>10^9）可能耗时很长
· 内存使用：分段筛内存占用约 SEGMENT_SIZE / 2 字节，约2.5MB
· 线程数：默认使用所有CPU核心，可通过 export OMP_NUM_THREADS=N 调整

🤝 贡献

欢迎提交 Issue 和 Pull Request！

开发计划

☐ 支持命令行参数（非交互模式）
☐ 输出格式选项（JSON, CSV）
☐ 更高效的素性测试（如 Baillie-PSW）
☐ 支持 128位整数

📄 许可证

本项目采用 MIT 许可证 - 详见 LICENSE 文件

📧 联系方式

如有问题或建议，请提交 Issue 或联系 [Your Email]

---

⭐ 如果这个项目对你有帮助，请给个 Star！

```
