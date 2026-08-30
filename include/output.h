// ============================================================================
//  output.h — 输出目标（stdout / 文件）与结果打印
// ============================================================================

#ifndef PRIME_SIEVE_OUTPUT_H
#define PRIME_SIEVE_OUTPUT_H

#include <fstream>
#include <ostream>
#include <string>
#include <vector>

// RAII 输出目标：未指定输出文件时写 stdout；指定时写入文件，并在析构时
// 统一给出「结果已写入文件」的提示。open() 失败返回 false（已打印错误）。
class OutputSink {
public:
    std::ostream& out() { return *stream_; }
    bool open(const std::string& path, bool quiet);
    ~OutputSink();

private:
    std::ostream* stream_ = nullptr;
    std::ofstream file_;
    std::string path_;
    bool quiet_ = false;
};

// 输出结果到标准输出或文件。
void output_results(const std::vector<long long>& primes, double elapsed,
                    bool show_stats_only = false);

#endif // PRIME_SIEVE_OUTPUT_H
