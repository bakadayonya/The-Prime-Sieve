// ============================================================================
//  output.cpp — 输出目标与结果打印
// ============================================================================

#include "output.h"

#include <iomanip>
#include <iostream>

#include "config.h"

bool OutputSink::open(const std::string& path, bool quiet) {
    if (path.empty()) { stream_ = &std::cout; return true; }
    file_.open(path);
    if (!file_) {
        std::cerr << _("无法打开输出文件: ", "Cannot open output file: ") << path << std::endl;
        return false;
    }
    stream_ = &file_;
    path_ = path;
    quiet_ = quiet;
    return true;
}

OutputSink::~OutputSink() {
    if (file_.is_open()) {
        file_.close();
        if (!quiet_)
            std::cout << _("结果已写入文件: ", "Results written to file: ") << path_ << std::endl;
    }
}

void output_results(const std::vector<long long>& primes, double elapsed,
                    bool show_stats_only) {
    OutputSink sink;
    if (!sink.open(g_config.output_file, g_config.quiet)) return;
    std::ostream& out = sink.out();

    if (primes.empty()) {
        out << _("该区间没有素数", "No primes in this range") << std::endl;
        return;
    }

    if (!g_config.quiet) {
        out << _("\n找到 ", "\nFound ") << primes.size() << _(" 个素数", " primes");
        if (!show_stats_only) {
            out << ":" << std::endl;
        } else {
            out << std::endl;
        }
    }

    if (!show_stats_only) {
        const int max_display = g_config.quiet ? 0 : 200;
        if (max_display > 0 && !primes.empty()) {
            out << "----------------------------------------" << std::endl;
            int cnt = 0;
            size_t limit = std::min(primes.size(), (size_t)max_display);
            for (size_t i = 0; i < limit; ++i) {
                out << primes[i];
                if (++cnt % 10 == 0) out << '\n';
                else out << '\t';
            }
            if (cnt % 10 != 0) out << '\n';
            if (primes.size() > (size_t)max_display) {
                out << _("... 还有 ", "... and ") << (primes.size() - max_display)
                    << _(" 个未显示", " more not shown") << std::endl;
            }
            out << "----------------------------------------" << std::endl;
        }
    }

    // 统计信息
    out << _("\n========== 统计信息 ==========", "\n========== Statistics ==========") << std::endl;
    out << _("素数总数: ", "Total primes: ") << primes.size() << std::endl;
    out << _("最小素数: ", "Smallest prime: ") << primes.front() << std::endl;
    out << _("最大素数: ", "Largest prime: ") << primes.back() << std::endl;
    out << _("耗时: ", "Time: ") << std::fixed << std::setprecision(3) << elapsed
        << _(" 秒", " seconds") << std::endl;
    if (primes.size() > 1) {
        double density = (double)primes.size() / (primes.back() - primes.front()) * 100;
        out << _("素数密度: ", "Prime density: ") << std::fixed << std::setprecision(4)
            << density << "%" << std::endl;
    }
    out << _("================================", "====================================") << std::endl;
}
