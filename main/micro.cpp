#include "cmdline.h"
#include "hardware/dvfs.h"
#include "hardware/utils.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <limits>
#include <cmath>

#ifdef __linux__
#include <sched.h>
#include <unistd.h>   // gettid()
#endif

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

    double t_now_ns = now_ns();
    double delta_us = (t_now_ns - t0_ns) / 1e3;  // ns → us
    return delta_us;
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

// ===============================
// CPU affinity 설정 (Linux/Android)
// ===============================
bool set_affinity_to_cpu(int cpu_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    // 현재 스레드의 TID 가져오기 (Android/bionic 에서 지원)
    pid_t tid = gettid();  // calling thread

    int rc = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "[WARN] sched_setaffinity failed, rc=" << rc << "\n";
        return false;
    }
    return true;
#else
    (void)cpu_id;
    return true;
#endif
}


// ===============================
// 샘플 저장용 struct
// ===============================
struct Sample {
    double t_ns;   // 현재 시각 (ns)
    double dt_us;  // 이전 샘플과의 간격 (us)
};

// 전역 컨트롤 플래그
std::atomic<bool> g_stop{false};
std::atomic<bool> g_started{false};
std::atomic<double> g_dvfs_req_time_ns{0.0};  // DVFS 요청 시각 기록 (옵션)

// ===============================
// 워커 스레드: 메인 프로세스 역할
// ===============================
void worker_thread_func(int cpu_id,
                        std::vector<Sample>& samples,
                        size_t max_samples)
{
    // 특정 CPU 코어에 고정 (예: big 코어 번호)
    set_affinity_to_cpu(cpu_id);

    samples.clear();
    samples.reserve(max_samples);

    // 시작 시각
    double t_prev = now_ns();
    g_started.store(true, std::memory_order_release);

    while (!g_stop.load(std::memory_order_acquire)) {
        double t_now = now_ns();
        double dt_us = (t_now - t_prev) / 1e3;  // ns → us

        if (samples.size() < max_samples) {
            samples.push_back({t_now, dt_us});
        }

        t_prev = t_now;
        // ※ 의도적으로 아무 것도 안 함: "바쁜 루프"로 DVFS 때문에 생기는
        //    스톨을 최대한 민감하게 감지하기 위함.
    }
}


int main(int argc, char** argv) {
    cmdline::parser cmdParser;
    
    cmdParser.add<int>("mode", 'm', "specify the mode", true, 0);
    cmdParser.add<std::string>("device", 'D', "specify your android phone [Pixel9 | S24]", true, "");
    cmdParser.add<int>("freq-a", 'a', "specify CPU clock index for CPU DVFS", true, 0);
    cmdParser.add<int>("freq-b", 'b', "specify RAM clock index for RAM DVFS", true, 0);
    cmdParser.parse_check(argc, argv);

    const int mode = cmdParser.get<int>("mode");
    std::string device_name = cmdParser.get<std::string>("device");
    int freq_a = cmdParser.get<int>("freq-a");
    int freq_b = cmdParser.get<int>("freq-b");

    DVFS dvfs(device_name);

    if (mode == 0){

#pragma region DVFS_LATENCY_TEST
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

        us = measure_one_switch(dvfs, freqs_idx[i % freqs_idx.size()]);
        if (!std::isnan(us)) {
            latencies_us.push_back(us);
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
#pragma endregion // DVFS_LATENCY_TEST
    }
    else if (mode == 1){
#pragma region DVFS_JITTER_TEST
    // (예시) big 코어 CPU ID
    // - Snapdragon: big 코어가 4~7일 가능성이 큼 → 6 같은 값 사용
    // - Tensor / Exynos도 big 코어 번호 확인해서 맞춰주면 됨.
    const int worker_cpu_id = 7;  // 기기에 맞게 수정
    const size_t max_samples = 2000000; // 필요에 따라 조정
    const int switch_iter = 1;          // A<->B 스위칭 횟수

    // 워커 스레드에서 쌓을 샘플 버퍼
    std::vector<Sample> samples;

    // 워커 스레드 시작
    std::thread worker(worker_thread_func,
                       worker_cpu_id,
                       std::ref(samples),
                       max_samples);

    // 워커가 준비될 때까지 대기
    while (!g_started.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 1) 먼저 freq_A로 고정 (선택 사항)
    std::vector<int> confA = dvfs.get_cpu_freqs_conf(freq_a);
    std::vector<int> confB = dvfs.get_cpu_freqs_conf(freq_b);
    dvfs.set_cpu_freq(confA);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 2) DVFS 스위칭: A <-> B 를 switch_iter번 반복
    std::vector<double> dvfs_req_times_ns;
    dvfs_req_times_ns.reserve(switch_iter);

    for (int i = 0; i < switch_iter; ++i) {
        // 짝수: A -> B, 홀수: B -> A (원하면 반대로도 가능)
        const std::vector<int>& target_conf = (i % 2 == 0) ? confB : confA;

        double t_req_ns = now_ns();
        g_dvfs_req_time_ns.store(t_req_ns, std::memory_order_release);
        dvfs_req_times_ns.push_back(t_req_ns);

        int ret = dvfs.set_cpu_freq(target_conf);
        if (ret != 0) {
            std::cerr << "[WARN] set_cpu_freq switch " << i
                      << " failed (ret=" << ret << ")\n";
        }

        // 스위칭 사이 간격 (예: 200ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 3) 실험 종료
   // 3) 실험 종료
    g_stop.store(true, std::memory_order_release);
    worker.join();

    std::cout << "Total samples collected: " << samples.size() << "\n";

    // ===============================
    // 결과를 파일로 저장
    // ===============================
    const char* output_filename = "worker_deltas.txt";
    std::ofstream ofs(output_filename);
    if (!ofs.is_open()) {
        std::cerr << "[ERROR] Failed to open " << output_filename << "\n";
        return 1;
    }

    ofs << "# t_ns\t dt_us\n";
    // 여러 번 DVFS 요청 시각도 주석으로 기록
    for (size_t i = 0; i < dvfs_req_times_ns.size(); ++i) {
        ofs << "# dvfs_req_time_ns[" << i << "] = "
            << dvfs_req_times_ns[i] << "\n";
    }

    for (const auto& s : samples) {
        ofs << s.t_ns << "\t" << s.dt_us << "\n";
    }

    ofs.close();
    std::cout << "[INFO] Saved samples to " << output_filename << "\n";

    // 간단한 통계: 전체 dt 평균 / 최대 보기
    double sum = 0.0;
    double max_dt = 0.0;
    for (const auto& s : samples) {
        sum += s.dt_us;
        if (s.dt_us > max_dt) max_dt = s.dt_us;
    }
    double mean_dt = (samples.empty() ? 0.0 : sum / samples.size());

    std::cout << "Mean dt  = " << mean_dt << " us\n";
    std::cout << "Max dt   = " << max_dt  << " us\n";
#pragma endregion // DVFS_JITTER_TEST
    }

    return 0;
}