#ifndef COMMON_H
#define COMMON_H

// This common file is for IGNITE technique parameters

#include <vector>
#include <string>

struct ignite_params {
    // resource plane
    double time_slot = 0.5; // s
    double temp_threshold = 80.0; // Celsius
    std::vector<double> temp_history = {}; // temperature history
    int temp_cap = 10; // max length of temperature history
    double temp_alpha = 0.6; // for EMA
    int max_cpu_clk_idx = 0; // fixed by device
    int cur_cpu_clk_idx = 0; // dynamic
    int max_ram_clk_idx = 0; // fixed by device
    int cur_ram_clk_idx = 0; // dynamic

    // llm plane
    int phase_pause = 0; // ms
    int token_pause = 0; // ms
    int layer_pause = 0; // ms
    int query_interval = 0; // ms
    bool prefill_phase = true; // prefill phase or not
    double prefill_speed = 0.0; // tokens/s
    double decode_speed = 0.0; // tokens/s
};

#endif // COMMON_H