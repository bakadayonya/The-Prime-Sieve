cmake_minimum_required(VERSION 3.12)
project(PrimeSieve VERSION 8.0.0 LANGUAGES CXX)

# ============================================================================
#  编译器选择 / Compiler selection
#  默认使用系统默认 C++ 编译器（通常为 GCC）。
#  如需使用 LLVM Clang（本项目推荐 clang++-21，自带 omp.h 与 libomp.so），
#  在配置时显式指定编译器：
#      cmake -DCMAKE_CXX_COMPILER=clang++-21 ..
#  find_package(OpenMP) 会自动为 GCC(-fopenmp/libgomp) 与
#  Clang(-fopenmp=libomp/libomp) 设置正确的编译与链接选项。
# ============================================================================

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /MP)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra)
endif()

# OpenMP 支持（必需：源码无条件 #include <omp.h> 并调用 OMP API）
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
    add_link_options(${OpenMP_CXX_FLAGS})
    message(STATUS "OpenMP support enabled: ${OpenMP_CXX_FLAGS}")
else()
    message(FATAL_ERROR
        "OpenMP 未找到 / OpenMP not found. 请安装 libgomp，或改用 clang++-21："
        "cmake -DCMAKE_CXX_COMPILER=clang++-21 ..")
endif()

# 添加可执行文件
add_executable(prime_sieve prime_sieve.cpp)

# 设置属性
set_target_properties(prime_sieve PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 安装
install(TARGETS prime_sieve DESTINATION bin)

# 测试（可选）
enable_testing()
add_test(NAME test_small_range 
    COMMAND prime_sieve -r 2 100
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test_check_prime
    COMMAND prime_sieve -c 97
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# 内置正确性自检：校验已知 π(x)、Miller-Rabin、分段筛/混合筛一致性。
# prime_sieve -v 返回非零退出码表示存在失败项，ctest 据此判定通过/失败。
add_test(NAME verify_correctness
    COMMAND prime_sieve -v
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(verify_correctness PROPERTIES
    TIMEOUT 120
)