/**
 * @file demo_qwen3.cpp
 * @author hyh
 * @version 0.1
 * @date 2024-05-11
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "cmdline.h"
#include "models/qwen3/configuration_qwen3.hpp"
#include "models/qwen3/modeling_qwen3.hpp"
#include "models/qwen3/tokenization_qwen3.hpp"
#include "utils/json.hpp"
#include "utils/files.hpp"
#include "hardware/utils.h"

using namespace mllm;
using json = nlohmann::json;

std::string replaceFirst(std::string& str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    if (str.substr(pos, from.length()) == from) {
        str.replace(pos, from.length(), to);
    }
    return str;
}

int main(int argc, char **argv) {
    std::iostream::sync_with_stdio(false);
    cmdline::parser cmdParser;

    // arg parser: BASIC
    cmdParser.add<string>("vocab", 'v', "specify mllm tokenizer model path", false, "../vocab/qwen_vocab.mllm");
    cmdParser.add<string>("merge", 'e', "specify mllm merge file path", false, "../vocab/qwen_merges.txt");
    cmdParser.add<string>("model", 'm', "specify mllm model path", false, "../models/qwen-3-0.6b-q4_k.mllm");
    cmdParser.add<string>("billion", 'b', "[0.6B | 4B |]", false, "0.6B");
    cmdParser.add<int>("limits", 'l', "max KV cache size", false, 800);
    cmdParser.add<int>("thread", 't', "num of threads", false, 4);
    
    // arg parser: For Stream
    cmdParser.add<bool>("interface", 'i', "print inference interface", false, true);
    cmdParser.add<int>("start", 's', "starting num of queries", false, -1);
    cmdParser.add<int>("length", 'L', "num of queries", false, -1);
    cmdParser.add<bool>("save", 'S', "save query-answer pairs with json", false, true);
    
    // arg parser: For Evaluation
    cmdParser.add<string>("query-path", 'q', "specify query path of csv", false, "../alpaca_eval.json");
    cmdParser.add<string>("output-path", 'o', "specify output path of json", false, "AlpacaEval_mllm_Qwen3_1.7B_q4k_result.json");

    // arg parser: For Parsing
    cmdParser.parse_check(argc, argv);

// Now, retrieve the arguments
    // variable initialization: BASIC
    string vocab_path = cmdParser.get<string>("vocab");
    string merge_path = cmdParser.get<string>("merge");
    string model_path = cmdParser.get<string>("model");
    string model_billion = cmdParser.get<string>("billion");
    int tokens_limit = cmdParser.get<int>("limits");
    CPUBackend::cpu_threads = cmdParser.get<int>("thread");

    // variable initialization: For Stream
    const bool interface = cmdParser.get<bool>("interface");
    const int qa_start = cmdParser.get<int>("start");
    const int qa_len = cmdParser.get<int>("length");
    const string query_path = cmdParser.get<string>("query-path");
    string output_path = cmdParser.get<string>("output-path"); //"HotpotQA_mllm_result_Qwen"+model_billion+".json";
    const bool is_query_save = cmdParser.get<bool>("save");
    int qa_now = qa_start;
    int qa_limit = 0;
    string output_hard;
    string output_infer;
    string output_qa;

    // variable initialization: For File Naming (rename with timestamp)
    output_path = add_timestamp_to_filename(output_path);

    // Model Configuration
    auto tokenizer = QWen3Tokenizer(vocab_path, merge_path, false, false);
    QWen3Config config(tokens_limit, model_billion, RoPEType::HFHUBROPE);
    auto model = QWen3ForCausalLM(config);
    model.load(model_path);

    // Prepare to store answers
    vector<string> ans;
    std::ifstream file(query_path);
    json j; file >> j;
    const int qa_limit_idx = qa_limit==-1 ? 
                j.size()-1  :  MIN(qa_limit+qa_start, j.size())-1;
    cout << "from " << qa_start << " to " << qa_limit_idx << endl;


    // INFERENCE LOOP
    for (const auto& item : j) {
        if (qa_now > qa_limit_idx){ break; }
        if (qa_now < qa_start) { ++qa_now; continue; } // skip

        cout << qa_now << endl; 
        string question = item["instruction"];
        string answer = "";
        auto input_str = tokenizer.apply_chat_template(question);
        auto input_tensor = tokenizer.tokenize(input_str);
        std::cout << "[Q] " << question << std::endl;
        std::cout << "[A] " << std::flush;

        size_t max_new_tokens = tokens_limit - input_tensor.sequence();
        LlmTextGeneratorOpts opt{
            .max_new_tokens = max_new_tokens,
            .do_sample = false,
            .temperature = 0.0F,
        };
        model.generate(input_tensor, opt, [&](unsigned int out_token) -> bool {
            auto out_string = tokenizer.detokenize({out_token});
            auto [not_end, output_string] = tokenizer.postprocess(out_string);
            if (!not_end) { model.clear_kvcache(); return false; }
            std::cout << output_string << std::flush;
            output_string.erase(std::remove(output_string.begin(), output_string.end(), '\0'), output_string.end());
            answer += output_string;
            return true;
        });
        std::cout << "\n\n";
        // auto profile_res = model.profiling("Inference");
        // auto prefill_speed = profile_res[1];
        // auto prefill_token_size = profile_res[3];
        // if (file_.is_open()) {
        //     file_ << uuid << ',' << prefill_token_size<< ',' << prefill_speed << ',' << problem << '\n';
        // }
        //cout << answer << endl;
        ans.push_back(answer); // accummulate answers
        model.clear_kvcache();
        ++qa_now;
    }

    std::cout << "out\n";
    // store answers
    json qa_array = json::array();
    qa_now = 0;
    for (const auto& item : j) {
        if (qa_now > qa_limit_idx){ break; }
        if (qa_now < qa_start) { ++qa_now; continue; } // skip
        
        try {
            string a = qa_start == -1 ? ans[qa_now] : ans[qa_now-qa_start];
            a = remove_invalid_utf8(a);

            json pair;
            pair["question"] = item["instruction"];
            pair["answer"] = a;
            pair["answerKey"] = item["answerKey"];
            qa_array.push_back(pair);
        } catch (const std::exception& e) {
            std::cerr << "Error while adding QA pair at index: " << qa_now-qa_start  << " [dataset index: " << qa_now << "] "<< " --- " << e.what() << "\n";
        }
        ++qa_now;
    }

    std::ofstream output_file(output_path); // open file stream
    if (!output_file) {
        std::cerr << "Error: Could not open file for writing: " << output_path << std::endl;
        return 1;
    }

    output_file << qa_array.dump(4); // pretty print with indent=4
    output_file.close(); // close file stream
    std::cout << "Saved " << ans.size() << " QA pairs to " << output_path << std::endl;
}
