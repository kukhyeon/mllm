/**
 * @file llama3_eval.cpp
 * @author Jinhwi Kim (kjh2159@dgist.ac.kr)
 * @version 0.1
 * @date 2025-05-13
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "cmdline.h"
#include "models/llama3/modeling_llama3.hpp"
#include "models/llama3/tokenization_llama3.hpp"
#include "processor/PostProcess.hpp"
#include "utils/utils.h"
#include "utils/json.hpp"

using namespace mllm;
using namespace std;
using json = nlohmann::json;


std::string remove_invalid_utf8(const std::string& input) {
    
    std::string output;
    size_t i = 0, len = input.size();

    while (i < len) {
        unsigned char c0 = static_cast<unsigned char>(input[i]);
        size_t seq_len = 0;
        uint32_t codepoint = 0;

        // Determine sequence length and initialize codepoint accumulator
        if (c0 <= 0x7F) {
            // ASCII
            seq_len = 1;
            codepoint = c0;
        }
        else if ((c0 >> 5) == 0x6) {
            // 110xxxxx => 2‐byte
            seq_len = 2;
            codepoint = c0 & 0x1F;
        }
        else if ((c0 >> 4) == 0xE) {
            // 1110xxxx => 3‐byte
            seq_len = 3;
            codepoint = c0 & 0x0F;
        }
        else if ((c0 >> 3) == 0x1E) {
            // 11110xxx => 4‐byte
            seq_len = 4;
            codepoint = c0 & 0x07;
        }
        else {
            // Invalid leading byte
            ++i;
            continue;
        }

        // Not enough bytes left?
        if (i + seq_len > len) {
            // Drop the rest
            break;
        }

        // Validate continuation bytes and build codepoint
        bool valid = true;
        for (size_t j = 1; j < seq_len; ++j) {
            unsigned char cx = static_cast<unsigned char>(input[i + j]);
            if ((cx >> 6) != 0x2) { // must be 10xxxxxx
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cx & 0x3F);
        }

        // Reject overlong encodings, surrogates, out-of-range
        if (valid) {
            if ((seq_len == 2 && codepoint < 0x80) || (seq_len == 3 && codepoint < 0x800) ||
                (seq_len == 4 && codepoint < 0x10000) ||
                (codepoint > 0x10FFFF) ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            {
                valid = false;
            }
        }

        if (valid) {
            // Copy the valid sequence
            output.append(input, i, seq_len);
            i += seq_len;
        }
        else {
            // Skip only the leading byte and retry
            ++i;
        }
    }

    return output;
}


bool is_valid_utf8(const std::string& str) {
    int c, i, ix, n;
    for (i = 0, ix = str.length(); i < ix; i++) {
        c = (unsigned char) str[i];
        if (c <= 0x7F) n = 0;
        else if ((c & 0xE0) == 0xC0) n = 1;
        else if ((c & 0xF0) == 0xE0) n = 2;
        else if ((c & 0xF8) == 0xF0) n = 3;
        else return false;
        for (int j = 0; j < n && i < ix; j++) {
            if ((++i == ix) || ((str[i] & 0xC0) != 0x80))
                return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    std::iostream::sync_with_stdio(false);

    cmdline::parser cmdParser;
    cmdParser.add<string>("vocab", 'v', "specify mllm tokenizer model path", false, "../vocab/llama3_tokenizer.model");
    cmdParser.add<string>("model", 'm', "specify mllm model path", false, "../models/llama-3.2-1b-instruct_q4_k.mllm");
    cmdParser.add<string>("billion", 'b', "[1B | 3B ]", false, "1B");
    cmdParser.add<string>("query-path", 'q', "specify query path of csv", false, "../alpaca_eval.json");
    cmdParser.add<int>("limits", 'l', "max KV cache size", false, 2048);
    cmdParser.add<int>("thread", 't', "num of threads", false, 16);
    cmdParser.add<int>("limit", 'L', "num of queries", false, -1);
    cmdParser.add<int>("start", 's', "num of queries", false, 0);
    cmdParser.parse_check(argc, argv);

    string vocab_path = cmdParser.get<string>("vocab");
    string model_path = cmdParser.get<string>("model");
    string model_billion = cmdParser.get<string>("billion");
    const std::string query_path = cmdParser.get<string>("query-path");
    cout << query_path << endl;
    int tokens_limit = cmdParser.get<int>("limits");
    CPUBackend::cpu_threads = cmdParser.get<int>("thread");
    const int qa_limit = cmdParser.get<int>("limit"); // length of limit
    const int qa_start = cmdParser.get<int>("start");
    int qa_now = 0; // qa_start
    const string output_path = "AlpacaEval_mllm_LLaMA3.2_1B_result.json";

    // Model Configuration
    Llama3Config config(tokens_limit, model_billion);
    auto tokenizer = LLama3Tokenizer(vocab_path);
    config.cache_limit = tokens_limit;
    auto model = Llama3Model(config);
    model.load(model_path);
    
    vector<string> ans;
    std::ifstream file(query_path);
    json j;
    file >> j;

    // max: 805
    const int qa_limit_idx = qa_limit==-1 ? 
                j.size()-1  :  MIN(qa_limit+qa_start, j.size())-1;
    cout << "from " << qa_start << " to " << qa_limit_idx << endl;

    // qa_start = 202;
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
            if (!not_end) { return false; }
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
        cout << answer << endl;
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
            const std::string& question = item["instruction"];
            string a = ans[qa_now-qa_start];
            a = remove_invalid_utf8(a);

            json pair;
            pair["question"] = question;
            pair["answer"] = a;
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

    return 0;
}
