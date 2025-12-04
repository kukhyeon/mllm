#include "cmdline.h"
#include "hardware/dvfs.h"
#include "hardware/utils.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using Clock = std::chrono::steady_clock;

struct Stats {
    double mean = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
    size_t count = 0;
};

inline double now_ns() {
    return std::chrono::duration<double, std::nano>(Clock::now().time_since_epoch()).count();
}

double measure_one_switch(
    DVFS& dvfs,
    int target_idx,
    int poll_interval_us = 50,
    int timeout_us = 10000   // 10ms
) {
    // 1. 스위치 요청 시각
    double t0_ns = now_ns();
    std::vector<int> target_conf = dvfs.get_cpu_freqs_conf(target_idx);
    int ok = dvfs.set_cpu_freq(target_conf);
    if (ok != 0) {
        std::cerr << "[WARN] set_cpu_frequency(";
        for (auto f : target_conf) std::cerr << f << " ";
        std::cerr << ") failed\n";
        return std::numeric_limits<double>::quiet_NaN();
    }

    // 2. polling으로 실제 freq 반영 시점 찾기
    // const double timeout_ns = timeout_us * 1e3;
    // while (true) {
    //     double t_now_ns = now_ns();
    //     if (t_now_ns - t0_ns > timeout_ns) {
    //         std::cerr << "[WARN] timeout while waiting for freq = ( ";
    //         for (auto f : target_conf) std::cerr << f << " ";
    //         std::cerr << ")";
    //         return std::numeric_limits<double>::quiet_NaN();
    //     }

        
    //     int cur = dvfs.get_cpu_freq(); ???
    //     if (cur == target_khz) {
    //         double t_applied_ns = t_now_ns;
    //         double delta_us = (t_applied_ns - t0_ns) / 1e3;  // ns → us
    //         return delta_us;
    //     }

    //     if (poll_interval_us > 0) {
    //         std::this_thread::sleep_for(std::chrono::microseconds(poll_interval_us));
    //     }
    // }
}

Stats compute_stats(const std::vector<double>& vals) {
    Stats s{};
    if (vals.empty()) return s;

    s.count = vals.size();
    s.min = std::numeric_limits<double>::infinity();
    s.max = -std::numeric_limits<double>::infinity();

    double sum = 0.0;
    double sum_sq = 0.0;

    for (double v : vals) {
        if (std::isnan(v)) continue;
        sum += v;
        sum_sq += v * v;
        if (v < s.min) s.min = v;
        if (v > s.max) s.max = v;
    }

    s.mean = sum / s.count;
    double var = sum_sq / s.count - s.mean * s.mean;
    if (var < 0) var = 0;
    s.stddev = std::sqrt(var);
    return s;
}

void write_latencies_to_file(const std::string &filename,
                             const std::vector<double> &latencies_us) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "[ERROR] Failed to open file: " << filename << "\n";
        return;
    }

    ofs << "# index\tlatency_us\n";
    for (size_t i = 0; i < latencies_us.size(); ++i) {
        ofs << i << "\t" << latencies_us[i] << "\n";
    }

    ofs.close();
    std::cout << "  [INFO] Saved " << latencies_us.size()
              << " samples to " << filename << "\n";
}

int main(){
    cmdline::parser cmdParser;
    cmdParser.add<std::string>("device", 'D', "specify your android phone [Pixel9 | S24]", true, "");
    cmdParser.add<int>("freq-a", 'a', "specify CPU clock index for CPU DVFS", true, 0);
    cmdParser.add<int>("freq-b", 'b', "specify RAM clock index for RAM DVFS", true, 0);

    std::string device_name = cmdParser.get<std::string>("device");
    int freq_a = cmdParser.get<int>("freq-a");
    int freq_b = cmdParser.get<int>("freq-b");

    DVFS dvfs(device_name);


    const int warmup_iters  = 10;   // 캐시/드라이버 워밍업
    const int measure_iters = 1000;  // 실제 통계 계산 반복 횟수
    std::vector<int> freqs_idx = { freq_a, freq_b };
    std::vector<double> latencies_us;

    // 1) 워밍업
    double us;
    for (int i = 0; i < warmup_iters; ++i) {
        us = measure_one_switch(dvfs, freqs_idx[i % freqs_idx.size()]);
        (void)us; // 워밍업 결과는 버림
    }


    us = 0.0;
    for (int i = 0; i < measure_iters; ++i) {
        // 2) 실제 측정
        latencies_us.reserve(measure_iters);

        for (int i = 0; i < measure_iters; ++i) {
            us = measure_one_switch(dvfs, freqs_idx[i % freqs_idx.size()]);
            if (!std::isnan(us)) {
                latencies_us.push_back(us);
            }
        }
    }

    Stats st = compute_stats(latencies_us);

    std::cout << "  count = " << st.count << "\n";
    std::cout << "  mean  = " << st.mean  << " us\n";
    std::cout << "  std   = " << st.stddev << " us\n";
    std::cout << "  min   = " << st.min   << " us\n";
    std::cout << "  max   = " << st.max   << " us\n";
    std::cout << std::endl;

    std::string filename = "dvfs_latency_" + std::to_string(freq_a) + "_" + std::to_string(freq_b)  + ".txt";
    write_latencies_to_file(filename, latencies_us);

    return 0;
}