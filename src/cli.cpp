// ============================================================================
//  cli.cpp — 命令行参数解析（手动实现，兼容 POSIX 风格）
// ============================================================================

#include "cli.h"

#include <cstdint>
#include <iostream>
#include <string>

#include "config.h"

void print_usage(const char* prog) {
    std::cout << _("用法: ", "Usage: ") << prog << _(" [选项]\n",
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
         << std::endl;
}

bool parse_arguments(int argc, char* argv[]) {
    if (argc == 1) {
        g_config.interactive = true;
        return true;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return false;  // 直接退出
        } else if (arg == "-i" || arg == "--interactive") {
            g_config.interactive = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
        } else if (arg == "-l" || arg == "--language") {
            if (i + 1 < argc) {
                std::string lang = argv[++i];
                if (lang == "zh" || lang == "zh_CN") g_config.lang = LANG_CHINESE;
                else if (lang == "en" || lang == "en_US") g_config.lang = LANG_ENGLISH;
                else {
                    std::cerr << _("未知语言: ", "Unknown language: ") << lang << std::endl;
                    return false;
                }
            } else {
                std::cerr << _("选项 -l 需要参数", "Option -l requires an argument") << std::endl;
                return false;
            }
        } else if (arg == "-r" || arg == "--range") {
            if (i + 2 < argc) {
                g_config.action = ACTION_RANGE;
                try {
                    g_config.low = std::stoll(argv[++i]);
                    g_config.high = std::stoll(argv[++i]);
                } catch (...) {
                    std::cerr << _("无效的数字参数", "Invalid numeric argument") << std::endl;
                    return false;
                }
            } else {
                std::cerr << _("选项 -r 需要两个参数", "Option -r requires two arguments") << std::endl;
                return false;
            }
        } else if (arg == "-d" || arg == "--digits") {
            if (i + 1 < argc) {
                g_config.action = ACTION_DIGITS;
                try {
                    g_config.digits = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << _("无效的数字参数", "Invalid numeric argument") << std::endl;
                    return false;
                }
            } else {
                std::cerr << _("选项 -d 需要一个参数", "Option -d requires an argument") << std::endl;
                return false;
            }
        } else if (arg == "-c" || arg == "--check") {
            if (i + 1 < argc) {
                g_config.action = ACTION_CHECK;
                try {
                    g_config.check_num = std::stoull(argv[++i]);
                } catch (...) {
                    std::cerr << _("无效的数字参数", "Invalid numeric argument") << std::endl;
                    return false;
                }
            } else {
                std::cerr << _("选项 -c 需要一个参数", "Option -c requires an argument") << std::endl;
                return false;
            }
        } else if (arg == "-n" || arg == "--nth") {
            if (i + 1 < argc) {
                g_config.action = ACTION_NTH;
                try {
                    g_config.nth = std::stoll(argv[++i]);
                } catch (...) {
                    std::cerr << _("无效的数字参数", "Invalid numeric argument") << std::endl;
                    return false;
                }
            } else {
                std::cerr << _("选项 -n 需要一个参数", "Option -n requires an argument") << std::endl;
                return false;
            }
        } else if (arg == "-p" || arg == "--perf") {
            g_config.action = ACTION_PERF;
        } else if (arg == "-v" || arg == "--verify") {
            g_config.action = ACTION_VERIFY;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                g_config.output_file = argv[++i];
            } else {
                std::cerr << _("选项 -o 需要一个参数", "Option -o requires an argument") << std::endl;
                return false;
            }
        } else {
            std::cerr << _("未知选项: ", "Unknown option: ") << arg << std::endl;
            print_usage(argv[0]);
            return false;
        }
    }

    if (g_config.interactive) {
        return true;
    }

    if (g_config.action == ACTION_NONE) {
        std::cerr << _("未指定任何操作，使用 -h 查看帮助。", "No action specified, use -h for help.") << std::endl;
        return false;
    }

    return true;
}
