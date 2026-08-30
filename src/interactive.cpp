// ============================================================================
//  interactive.cpp — 交互模式主循环
// ============================================================================

#include "interactive.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "actions.h"
#include "config.h"

// 安全输入辅助（交互模式使用）
template <typename T>
static bool safe_input(T& value, const std::string& prompt = "") {
    while (true) {
        if (!prompt.empty()) std::cout << prompt;
        std::cin >> value;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << _("输入无效，请输入一个整数。", "Invalid input, please enter an integer.") << std::endl;
        } else {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return true;
        }
    }
}

static bool confirm_action(const std::string& prompt) {
    std::string input;
    while (true) {
        std::cout << prompt;
        if (!(std::cin >> input)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << _("输入无效，请输入 y 或 n。", "Invalid input, please enter y or n.") << std::endl;
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (input.size() == 1 && (input[0] == 'y' || input[0] == 'Y' || input[0] == 'n' || input[0] == 'N')) {
            return (input[0] == 'y' || input[0] == 'Y');
        }
        std::cout << _("请输入 'y' 或 'n'。", "Please enter 'y' or 'n'.") << std::endl;
    }
}

void run_interactive() {
    std::cout << _("========== 超级素数筛（极致优化版 v9.0） ==========",
                   "========== Super Prime Sieve (Ultra Optimized v9.0) ==========")
              << std::endl;
    std::cout << _("支持范围: 2 ~ 10^18（更大可能极慢）",
                   "Supported: 2 ~ 10^18 (larger may be slow)")
              << std::endl;
    std::cout << _("自动切换分段筛 / 混合筛（OpenMP + 动态调度 + 模30轮子）",
                   "Auto-select segmented/hybrid sieve (OpenMP + dynamic scheduling + mod30 wheel)")
              << std::endl
              << std::endl;

    while (true) {
        std::cout << _("\n请选择模式：", "\nSelect mode:") << std::endl;
        std::cout << _("1. 按位数查找", "1. Search by digits") << std::endl;
        std::cout << _("2. 自定义区间", "2. Custom range") << std::endl;
        std::cout << _("3. 检查单数", "3. Check a single number") << std::endl;
        std::cout << _("4. 查找第 n 个素数", "4. Find nth prime") << std::endl;
        std::cout << _("5. 性能测试 (2~10^7)", "5. Performance test (2~10^7)") << std::endl;
        std::cout << _("6. 切换语言 (Switch Language)", "6. Switch Language") << std::endl;
        std::cout << _("7. 退出", "7. Exit") << std::endl;

        int choice;
        safe_input(choice, _("输入 (1-7): ", "Enter (1-7): "));

        if (choice == 7) {
            std::cout << _("再见！", "Goodbye!") << std::endl;
            break;
        }

        if (choice == 6) {
            std::cout << _("\n当前语言: ", "\nCurrent language: ")
                      << (g_config.lang == LANG_CHINESE ? _("中文", "Chinese") : _("英文", "English"))
                      << std::endl;
            std::cout << _("1. 中文", "1. Chinese") << std::endl;
            std::cout << _("2. English", "2. English") << std::endl;

            int lang_choice;
            safe_input(lang_choice, _("请选择 (1-2): ", "Select (1-2): "));

            if (lang_choice == 1) {
                g_config.lang = LANG_CHINESE;
                std::cout << _("已切换到中文", "Switched to Chinese") << std::endl;
            } else if (lang_choice == 2) {
                g_config.lang = LANG_ENGLISH;
                std::cout << _("已切换到英文", "Switched to English") << std::endl;
            } else {
                std::cout << _("无效选择，保持当前语言", "Invalid choice, keeping current language") << std::endl;
            }
            continue;
        }

        // 交互模式始终输出到屏幕，暂存并清空 output_file
        std::string saved_output = g_config.output_file;
        g_config.output_file.clear();

        long long low, high;
        if (choice == 1) {
            int digits;
            safe_input(digits, _("位数 (1-18): ", "Digits (1-18): "));
            auto range = [&] {
                if (digits <= 0 || digits > 18) return std::make_pair(0LL, 0LL);
                long long l = 1;
                for (int i = 1; i < digits; ++i) l *= 10;
                long long h = l * 10 - 1;
                if (digits == 1) l = 2;
                return std::make_pair(l, h);
            }();
            if (range.first == 0) {
                std::cout << _("超出范围", "Out of range") << std::endl;
            } else {
                run_digits(digits);
            }
        } else if (choice == 2) {
            safe_input(low, _("下限 low (>=2): ", "low (>=2): "));
            safe_input(high, _("上限 high: ", "high: "));
            if (low < 2 || high < low) {
                std::cout << _("无效区间", "Invalid range") << std::endl;
            } else {
                run_range(low, high);
            }
        } else if (choice == 3) {
            uint64_t n;
            safe_input(n, _("输入数: ", "Enter number: "));
            run_check(n);
        } else if (choice == 4) {
            long long n;
            safe_input(n, _("n: ", "n: "));
            if (n <= 0) {
                std::cout << _("请输入正整数", "Enter positive integer") << std::endl;
            } else {
                if (n > 100000000LL) {
                    if (!confirm_action(_("n 较大，继续? (y/n): ", "Large n, continue? (y/n): "))) {
                        g_config.output_file = saved_output;
                        continue;
                    }
                }
                run_nth(n);
            }
        } else if (choice == 5) {
            run_performance();
        } else {
            std::cout << _("无效选择", "Invalid choice") << std::endl;
        }

        g_config.output_file = saved_output;
    }
}
