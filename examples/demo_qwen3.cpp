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

using namespace mllm;

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
    cmdParser.add<string>("vocab", 'v', "specify mllm tokenizer model path", false, "../vocab/qwen_vocab.mllm");
    cmdParser.add<string>("merge", 'e', "specify mllm merge file path", false, "../vocab/qwen_merges.txt");
    cmdParser.add<string>("model", 'm', "specify mllm model path", false, "../models/qwen-3-0.6b-q4_k.mllm");
    cmdParser.add<string>("billion", 'b', "[0.6B | 4B |]", false, "0.6B");
    cmdParser.add<int>("limits", 'l', "max KV cache size", false, 800);
    cmdParser.add<int>("thread", 't', "num of threads", false, 4);
    cmdParser.parse_check(argc, argv);

    string vocab_path = cmdParser.get<string>("vocab");
    string merge_path = cmdParser.get<string>("merge");
    string model_path = cmdParser.get<string>("model");
    string model_billion = cmdParser.get<string>("billion");
    int tokens_limit = cmdParser.get<int>("limits");
    CPUBackend::cpu_threads = cmdParser.get<int>("thread");

    auto tokenizer = QWen3Tokenizer(vocab_path, merge_path, false, false);
    QWen3Config config(tokens_limit, model_billion, RoPEType::HFHUBROPE);
    auto model = QWen3ForCausalLM(config);
    model.load(model_path);

    vector<string> in_strs = {
        "Hello, who are you?",
        "What can you do?",
        "Please introduce Daegu Gyeongbuk Institute of Science and Tenchnology.",
    };
    for (int i = 0; i < in_strs.size(); ++i) {
        auto input_str = tokenizer.apply_chat_template(in_strs[i]);
        auto input_tensor = tokenizer.tokenize(input_str);
        std::cout << "[Q] " << in_strs[i] << std::endl;
        std::cout << "[A] " << std::flush;

        LlmTextGeneratorOpts opt{
            .max_new_tokens = 2000,
            .do_sample = false,
            .temperature = 0.0F,
        };
        model.generate(input_tensor, opt, [&](unsigned int out_token) -> bool {
            auto out_string = tokenizer.detokenize({out_token});
            auto [not_end, output_string] = tokenizer.postprocess(out_string);
            if (!not_end) {
                model.clear_kvcache();
                return false;
            }
            std::cout << replaceFirst(output_string, "\n\n", "") << std::flush;
            return true;
        });
        
        std::cout << "\n";
        auto profile_res = model.profiling("Inference");
    }
}
