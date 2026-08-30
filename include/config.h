// ============================================================================
//  config.h — 全局配置与公共枚举
//  将原先散落的全局变量（g_lang / g_action / g_low / g_high / g_nth /
//  g_check_num / g_digits / g_quiet / g_output_file / g_interactive_mode）
//  收敛为单一 Config 结构体，参数解析、交互与各 action 统一读写它。
// ============================================================================

#ifndef PRIME_SIEVE_CONFIG_H
#define PRIME_SIEVE_CONFIG_H

#include <cstdint>
#include <string>

// 操作类型
enum Action {
    ACTION_NONE,      // 未指定任何操作
    ACTION_RANGE,     // -r 区间筛
    ACTION_DIGITS,    // -d 按位数
    ACTION_CHECK,     // -c 单数素性
    ACTION_NTH,       // -n 第 n 个素数
    ACTION_PERF,      // -p 性能测试
    ACTION_VERIFY     // -v 正确性自检
};

enum Language { LANG_CHINESE, LANG_ENGLISH };

// 全局配置（单个全局实例，供所有模块读写）
struct Config {
    Language lang = LANG_CHINESE;
    Action action = ACTION_NONE;
    bool interactive = false;   // 无参数时进入交互
    bool quiet = false;
    std::string output_file;    // 空表示不输出到文件
    long long low = 0, high = 0;
    long long nth = 0;
    uint64_t check_num = 0;
    int digits = 0;
};

extern Config g_config;

// 国际化辅助：按当前语言返回中/英文文案
inline std::string _(const std::string& zh, const std::string& en) {
    return (g_config.lang == LANG_CHINESE) ? zh : en;
}

#endif // PRIME_SIEVE_CONFIG_H
