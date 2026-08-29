cmake_minimum_required(VERSION 3.12)
project(PrimeSieve VERSION 5.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /MP)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra)
endif()

# OpenMP 支持
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
    add_link_options(${OpenMP_CXX_FLAGS})
    message(STATUS "OpenMP support enabled")
else()
    message(WARNING "OpenMP not found, performance may be reduced")
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