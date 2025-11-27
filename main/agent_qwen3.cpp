/**
 * @file demo_qwen.cpp
 * @author Chenghua Wang (chenghua.wang.edu@gmail.com)
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "cmdline.h"
#include "models/qwen3/configuration_qwen3.hpp"
#include "models/qwen3/modeling_qwen3.hpp"
#include "models/qwen3/tokenization_qwen3.hpp"
#include "utils/json.hpp"
#include "hardware/dvfs.h"
#include "hardware/record.h"
#include "hardware/utils.h"
#include "common/common.h"

#include <cstdlib>
#include <thread>
#include <atomic>
#include <unistd.h>

using namespace mllm;
using namespace std;
using json = nlohmann::json;

std::atomic_bool sigterm(false);

std::string replaceFirst(std::string& str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    if (str.substr(pos, from.length()) == from) {
        str.replace(pos, from.length(), to);
    }
    return str;
}

void agent(struct ignite_params* params, /*to control*/ DVFS& dvfs, /*to monitor*/ Collector& collector, std::atomic<bool>& sigterm) {
    /* Here, variables for the algorithm! */
    std::size_t skip_tick = 0; bool skip = false; // action and skip
    const std::size_t monitor_interval_ms = 200; // ms
    const double target_temp = params->temp_threshold; // target temperature
    const int max_cpu_idx = params->max_cpu_clk_idx;
    const int max_ram_idx = params->max_ram_clk_idx;
    while (!sigterm.load()) {
        /* Here, agents run the algorithm! */                                                                           
        int now_cpu_idx = params->cur_cpu_clk_idx; // maybe, we can detect throttling through this info?
        int now_ram_idx = params->cur_ram_clk_idx; 
        double cur_temp = collector.collect_high_temp(); // collect the highest temperature from CPU cores!
        cur_temp = cur_temp > 10000.0 ? cur_temp / 1000.0 : cur_temp; // avoid millidegree

        if (cur_temp >= target_temp) {
            // should clock down
            now_cpu_idx = std::max(0, params->cur_cpu_clk_idx - 1);
            now_ram_idx = std::max(0, params->cur_cpu_clk_idx - 1);
        } else {
            // can clock up
            now_cpu_idx = std::min(max_cpu_idx, params->cur_cpu_clk_idx + 1);
            now_ram_idx = std::max(max_ram_idx, params->cur_cpu_clk_idx + 2);
        }

        // actual DVFS setting
        params->cur_cpu_clk_idx = now_cpu_idx;
        params->cur_ram_clk_idx = now_ram_idx;
        auto freq_config = dvfs.get_cpu_freqs_conf(now_cpu_idx);
        dvfs.set_cpu_freq(freq_config);
        dvfs.set_ram_freq(now_ram_idx);

        // wait for next monitoring
        std::this_thread::sleep_for(std::chrono::milliseconds((int)monitor_interval_ms));

        // std::size_t tick = 3;
        // double cur_temp = collector.collect_high_temp();
        // std::this_thread::sleep_for(std::chrono::milliseconds((int)(params->time_slot * 500)));
        // if (cur_temp < 0) {
        //     std::cerr << "[WARNING] Thermal zone not found. Skip this monitoring.\n";
        //     break;
        // }                                               
        // // collecting temperature
        // // temp_history: [ past <---> recent ]
        // if (params->temp_history.size() < params->temp_cap) {
        //     params->temp_history.push_back(cur_temp);
        //     continue;
        // } else {
        //     params->temp_history.erase(params->temp_history.begin());
        //     params->temp_history.push_back(cur_temp);
        // }
        
        // if (skip_tick == 0){ skip = false; }
        // if (skip) { skip_tick--; continue; }
        
        // // calculating average and standard deviation
        // double avg_temp = 0.0; double standard_deviation = 0.0; double temp = 0.0;
        // for (auto t : params->temp_history) { avg_temp += t;}
        // avg_temp /= params->temp_cap;
        // for (auto t : params->temp_history) { temp += (t - avg_temp) * (t - avg_temp);}
        // standard_deviation = sqrt(temp / params->temp_cap);
        // bool drop = true; double delta = 0.0; // tick is hysterisis
        // for (std::size_t i = params->temp_cap-1; i+tick >= params->temp_cap; --i){
        //     delta = params->temp_history[i] - params->temp_history[i-1];
        //     if ( delta >= 0 ){ // increase tendency
        //         drop = false;                                           
        //         //std::cout << std::flush << "<inc>";
        //         break;
        //     }
        // }
        
        // // DVFS control (action)
        // if (
        //     avg_temp
        //     //+ params->temp_alpha*standard_deviation
        //     < params->temp_threshold
        //     ||
        //     drop) {
        //     // Recovery || temp dropping
        //     params->cur_cpu_clk_idx = std::min(params->max_cpu_clk_idx, params->cur_cpu_clk_idx + 1); // step up 1
        //     //params->cur_ram_clk_idx = std::min(params->max_ram_clk_idx, params->cur_ram_clk_idx + 1); // step up 1
        //     //std::cout << std::flush << "<rec>"; // test
        //     skip = true; skip_tick = tick;
        // } else if (
        //     avg_temp
        //     //+ params->temp_alpha*standard_deviation
        //     >= params->temp_threshold)  {
        //     // Throttling
        //     params->cur_cpu_clk_idx = std::max(0, params->cur_cpu_clk_idx - 1); // step up 1
        //     //params->cur_ram_clk_idx = std::min(params->max_ram_clk_idx, params->cur_ram_clk_idx + 1); // step up 1
        //     //std::cout << std::flush << "<tht>"; // test
        //     skip = true; skip_tick = tick;
        // } else {
        //     // hold
        //     //std::cout << std::flush << "<hld>"; // test
        //     skip = false;
        //     continue;
        // }
        
        // // actual DVFS setting
        // auto freq_config = dvfs.get_cpu_freqs_conf(params->cur_cpu_clk_idx);
        // dvfs.set_cpu_freq(freq_config);
        // //dvfs.set_ram_freq(params->cur_ram_clk_idx);
    }
    std::cout << std::flush << "agent loop done\n"; // test
    return;
}

int main(int argc, char **argv) {
    std::iostream::sync_with_stdio(false);
    cmdline::parser cmdParser;
    pid_t pid = getpid();
    ignite_params _params;

    // arg parser: BASIC
    cmdParser.add<string>("vocab", 'v', "specify mllm tokenizer model path", false, "../vocab/qwen3_vocab.mllm");
    cmdParser.add<string>("merge", 'e', "specify mllm merge file path", false, "../vocab/qwen3_merges.txt");
    cmdParser.add<string>("model", 'm', "specify mllm model path", false, "../models/qwen3-1.7b-q4_k.mllm");
    cmdParser.add<string>("billion", 'b', "[0.6B | 1.7B | 4B |]", false, "0.6B");
    cmdParser.add<string>("family", 'f', "[Qwen1.5 | Qwen2.5 |]", false, "Qwen1.5"); // not used in Qwen3
    cmdParser.add<int>("limits", 'l', "max KV cache size", false, 1024);
    cmdParser.add<int>("thread", 't', "num of threads", false, 4);
    cmdParser.add<bool>("strict", 0, "apply token limits to only output tokens", false, false);
    cmdParser.add<bool>("thinking", 'T', "enable thinking [1: thinking | 0: non-thinking]", false, 0);

    // arg parser: For Stream
    cmdParser.add<bool>("interface", 'i', "print inference interface", false, true);
    cmdParser.add<int>("start", 's', "starting num of queries", false, -1);
    cmdParser.add<int>("length", 'L', "num of queries", false, -1);
    cmdParser.add<string>("input", 'I', "input dataset path of csv. ex) ./dataset/input.csv", false, ".");
    cmdParser.add<string>("output", 'O', "output directory path. ex) ./output/", false, ".");
    cmdParser.add<bool>("save", 'S', "save query-answer pairs with json", false, true);

    // arg parser: For DVFS
    cmdParser.add<string>("device", 'D', "specify your android phone [Pixel9 | S24]", true, "");
    cmdParser.add<int>("cpu", 'c', "specify starting CPU clock index for CPU DVFS", true, 0);
    cmdParser.add<int>("ram", 'r', "specify starting RAM clock index for RAM DVFS", true, 0);

    // arg parser: For IGNITE techniques
    /* Unit [ms] */
    cmdParser.add<int>("phase-pause", 'p', "specify a pause time between phases (ms)", true, 0);
    cmdParser.add<int>("token-pause", 'P', "specify a pause time between generation tokens (ms)", false, 0);
    cmdParser.add<int>("layer-pause", 'y', "specify a pause time between self-attention layers during prefill (ms)", true, 0);
    /* Unit [s] */
    cmdParser.add<int>("query-interval", 'q', "specify an interval time between queries (s)", true, 0);
    cmdParser.parse_check(argc, argv);

    // variable initialization: BASIC
    string vocab_path = cmdParser.get<string>("vocab");
    string merge_path = cmdParser.get<string>("merge");
    string model_path = cmdParser.get<string>("model");
    string model_billion = cmdParser.get<string>("billion");
    int tokens_limit = cmdParser.get<int>("limits");
    CPUBackend::cpu_threads = cmdParser.get<int>("thread");
    bool strict_limit = cmdParser.get<bool>("strict");
    const bool enable_thinking = cmdParser.get<bool>("thinking");

    // variable initialization: For DVFS
    const string device_name = cmdParser.get<string>("device");
    _params.cur_cpu_clk_idx = cmdParser.get<int>("cpu"); const int start_cpu_clk_idx = _params.cur_cpu_clk_idx;
    _params.cur_ram_clk_idx = cmdParser.get<int>("ram"); const int start_ram_clk_idx= _params.cur_ram_clk_idx;

    // variable initialization: For Stream
    const bool interface = cmdParser.get<bool>("interface");
    const int qa_start = cmdParser.get<int>("start");
    const int qa_len = cmdParser.get<int>("length");
    const string input_path = cmdParser.get<string>("input");
    const string output_dir = cmdParser.get<string>("output"); //"HotpotQA_mllm_result_Qwen"+model_billion+".json";
    const bool is_query_save = cmdParser.get<bool>("save");
    int qa_now = qa_start;
    int qa_limit = 0;
    string output_hard;
    string output_infer;
    string output_qa;

    // variable initialization: For Pause Techniques
    _params.token_pause = cmdParser.get<int>("token-pause"); int token_pause = _params.token_pause;
    _params.phase_pause = cmdParser.get<int>("phase-pause"); int phase_pause = _params.phase_pause;
    _params.layer_pause = cmdParser.get<int>("layer-pause"); int layer_pause = _params.layer_pause;
    _params.query_interval = cmdParser.get<int>("query-interval") * 1000; int query_interval = _params.query_interval;

    // variable initialization: For File Naming
    output_hard = joinPaths(output_dir, "HotpotQA_mllm_Qwen3_" + model_billion + "_" + to_string(start_cpu_clk_idx) + "-" + to_string(start_ram_clk_idx) + "_resource_agent_hard.txt");
    output_infer = joinPaths(output_dir, "HotpotQA_mllm_Qwen3_" + model_billion + "_" + to_string(start_cpu_clk_idx) + "-" + to_string(start_ram_clk_idx) + "_resource_agent_infer.txt");
    output_qa = joinPaths(output_dir, "HotpotQA_mllm_Qwen3_" + model_billion + "_result.json");

    // variable initialization: For Thermal Throttling Detection
    std::string command = "su -c \"";                                                            // prefix
    command += "awk '{print \\$1/1000}' /sys/devices/system/cpu/cpu7/cpufreq/scaling_cur_freq;"; // command
    command += "\"";                                                                             // postfix

    // Model Configuration
    auto tokenizer = QWen3Tokenizer(vocab_path, merge_path, false, enable_thinking);
    QWen3Config config(
        strict_limit ? tokens_limit + 8192 : tokens_limit, 
        model_billion, RoPEType::HFHUBROPE);
    auto model = QWen3ForCausalLM(config);
    model.load(model_path);
    model.init_ignite_params(_params);  // for this, must turn on "IGNITE_USE_SYSTEM" option when building MLLM
                                        // see scripts/build.sh
    Module::thread_sleep = layer_pause; // set layer-pause time

    // QA Dataset Load
    vector<vector<string>> qa_list = readCSV(input_path); // qa load
    vector<string> ans;                                   // qa load

    // DVFS setting
    DVFS dvfs(device_name);
    dvfs.output_filename = output_hard; // dvfs.output_filename requires hardware recording output path
    cout << pid << "\r\n";
    vector<int> freq_config = dvfs.get_cpu_freqs_conf(start_cpu_clk_idx);
    for (auto f : freq_config) { cout << f << " "; }
    cout << "\r\n"; // to validate (print freq-configuration)

    // param setting for dvfs
    model.params.max_cpu_clk_idx = dvfs.get_cpu_freq().at(
        dvfs.get_cluster_indices().at(
            dvfs.get_cluster_indices().size() - 1
        )
    ).size() - 1;
    model.params.max_ram_clk_idx = dvfs.get_ddr_freq().size() - 1;

    // inference recording contents
    const vector<string> infer_record_names = {"sys_time", "load_time", "prefill_speed", "decode_speed", "prefill_token", "decode_token", "ttft"};
    write_file(infer_record_names, output_infer);

    // limit=-1 -> infinite query stream
    if (qa_len == -1) {
        qa_limit = qa_list.size();
    } else {
        qa_limit = MIN(qa_list.size(), qa_start + qa_len) - 1;
    }

    // Prepare collector
    auto collector = dvfs.get_collector();
    //agent(&model.params, dvfs, collector, sigterm); // for future

    // measurement start
    auto start_sys_time = chrono::system_clock::now();
    std::thread record_thread = std::thread(record_hard, std::ref(sigterm), dvfs);

    // Start DVFS setting is only meaningful in this agent case
    freq_config = dvfs.get_cpu_freqs_conf(start_cpu_clk_idx);
    dvfs.set_cpu_freq(freq_config);
    dvfs.set_ram_freq(start_ram_clk_idx);

    // start agent
    std::thread scheduler_agent = std::thread(agent, &model.params, std::ref(dvfs), std::ref(collector), std::ref(sigterm));

    while ((qa_now - qa_start) < qa_limit) {
        string question = qa_list[qa_now][1];
        string answer;
        int ft = 0; // first token
        auto now_sys_time = chrono::system_clock::now();

        // Prefill DVFS setting is meaningless in this agent case

        auto time1 = chrono::system_clock::now();
        auto input_str = tokenizer.apply_chat_template(question);
        auto input_tensor = tokenizer.tokenize(input_str);
        auto time2 = chrono::system_clock::now();
        // std::cout << chrono::duration_cast<chrono::microseconds>(time2 - time1).count() << " " << input_tensor.sequence() << std::endl; // test
        if (interface) {
            std::cout << "[Q] " << question << "\r\n";
            std::cout << "[A] " << std::flush;
        }

        // Inference option setting
        std::size_t max_new_tokens = strict_limit ? tokens_limit : tokens_limit - input_tensor.sequence();
        //size_t max_new_tokens = 256;
        LlmTextGeneratorOpts opt{
            .max_new_tokens = max_new_tokens,
            .do_sample = false,
            .temperature = 0.0F
        };

        // Inference (Prefill & Decode)
        model.generate(input_tensor, opt, [&](unsigned int out_token) -> bool {
            auto out_string = tokenizer.detokenize({out_token});
            // now prefill done (when ft==0)
            // phase clock control is meaningless in this agent case
            
            // phase pause
            if (ft == 0 && phase_pause > 0) {
                // std::cout << std::flush << "sleep\n"; // test
                this_thread::sleep_for(chrono::milliseconds(phase_pause));
            }

            // generation start
            auto [not_end, output_string] = tokenizer.postprocess(out_string);
            // interface support
            if (interface) {
                std::cout << replaceFirst(output_string, "\r\n\n", "") << std::flush;
                output_string.erase(std::remove(output_string.begin(), output_string.end(), '\0'), output_string.end());
            }

            // token pause
            if (!not_end) {
                return false;
            } else {
                // std::cout << std::flush << " tp "; // test
                this_thread::sleep_for(chrono::milliseconds(token_pause)); // token pause
            }
            
            answer += output_string;
            ft++;
            return true;
        });
        std::cout << "\r\n";

        // store data
        auto sys_time = chrono::duration_cast<chrono::milliseconds>(now_sys_time - start_sys_time).count();
        // proifle_res = { prefill_speed, decode_speed, input_token, output_token, ttft  }
        auto profile_res = model.profiling("Inference");
        profile_res.insert(profile_res.begin(), (double)sys_time / (double)1000.0);
        write_file(profile_res, output_infer); // store in real time

        // throttling detection is meaningless in this agent case

        // Reset
        if (is_query_save) { ans.push_back(answer); } // accummulate answers
        model.clear_kvcache();
        qa_now++;
        ft = 0;

        // Query-interval
        if((qa_now - qa_start) < qa_limit) this_thread::sleep_for(chrono::milliseconds(query_interval));
    }

    // measurement done
    sigterm = true;
    dvfs.unset_cpu_freq();
    dvfs.unset_ram_freq();
    record_thread.join();
    scheduler_agent.join();

    cout << "DONE\r\n";
    this_thread::sleep_for(chrono::milliseconds(1000));

    // post-measurement-processing
    if (!is_query_save) {
        // not save the result
        // system("input touchscreen keyevent 26");
        // system("input touchscreen keyevent 82");
        return 0;
    }

    /* STREAM RESULT */
    // store answers
    json qa_array = json::array();
    for (int i = 0; i < ans.size(); ++i) {
        json pair;
        pair["question"] = qa_list[qa_start + i][1];
    std::cout << model.params.temp_cap << std::endl; // test
    std::cout << model.params.temp_alpha << std::endl; // test
        pair["answer"] = ans[i];
        qa_array.push_back(pair);
    }

    std::ofstream output_file(output_qa); // open file stream
    if (!output_file) {
        std::cerr << "Error: Could not open file for writing: " << output_qa << std::endl;
        return 1;
    }

    output_file << qa_array.dump(4); // pretty print with indent=4
    output_file.close();             // close file stream
    std::cout << "Saved " << ans.size() << " QA pairs to " << output_qa << "\r\n";

    // termination notification
    // system("input touchscreen keyevent 26");
    // system("input touchscreen keyevent 82");
    return 0;
}