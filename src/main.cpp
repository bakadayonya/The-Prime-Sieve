// ============================================================================
//  main.cpp — 程序入口：解析参数，分发到交互或各 action
// ============================================================================

#include <iostream>

#include "actions.h"
#include "cli.h"
#include "config.h"
#include "interactive.h"

int main(int argc, char* argv[]) {
    if (!parse_arguments(argc, argv)) {
        return 1;
    }

    if (g_config.interactive) {
        run_interactive();
        return 0;
    }

    switch (g_config.action) {
        case ACTION_RANGE:
            run_range(g_config.low, g_config.high);
            break;
        case ACTION_DIGITS:
            run_digits(g_config.digits);
            break;
        case ACTION_CHECK:
            run_check(g_config.check_num);
            break;
        case ACTION_NTH:
            run_nth(g_config.nth);
            break;
        case ACTION_PERF:
            run_performance();
            break;
        case ACTION_VERIFY:
            return run_verify();
        default:
            std::cerr << _("未知操作", "Unknown action") << std::endl;
            return 1;
    }

    return 0;
}
