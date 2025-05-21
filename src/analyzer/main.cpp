//
// Created by Jinhwi Kim on 25-05-19.
//


// #include "ParamWriter.hpp"
#include "cmdline.h"
#include "ParamLoader.hpp"
#include "LayerAnalyze.hpp"
// #include "backends/cpu/quantize/QuantizeQ4.hpp"
// #include "backends/cpu/quantize/QuantizeQ8.hpp"
#include <iostream>
#include <string>


#define MIN(a,b) (a<b ? a:b)


int main(int argc, char** argv){
    // std::iostream::sync_with_stdio(false);
    cmdline::parser cmdParser;

    // arg parser
    cmdParser.add<std::string>("file", 'f', "specify mllm model file", false, "models/qwen-1.5-0.5b-q4_k.mllm");
    cmdParser.parse_check(argc, argv);

    // variable initialization
    std::string model_file = cmdParser.get<std::string>("file");
    
    // model load
    mllm::ParamLoader paramLoader(model_file, true);
    std::cout << "Read File: " << model_file << std::endl
              << "Pass through " << paramLoader.getParamSize() << " Operaters\n";

    // LayerInfo
    std::vector<LayerInfo> layerInfos;
    std::vector<std::string> param_names = paramLoader.getParamNames();
    for (auto n : param_names) layerInfos.emplace_back(n);

    // Sort Layers
    std::sort(layerInfos.begin(), layerInfos.end());

    // Analysis
    std::vector<std::string> fragment;
    int major_group_order_tmp = -99;
    int layer_index_tmp = -99;
    // #define TEST

    size_t blk_num = 0;
    for (auto l : layerInfos){
#ifdef TEST
        std::cout << l.getOriginalName() << " | " << l.getMajorGroupOrder() << " | " << l.getLayerIndex() << std::endl;
        continue;
#endif

        // Print Major Group Layer
        fragment = l.getNameFragments();
        int mid_frag_idx = MIN(fragment.size(), 3);
        if (major_group_order_tmp != l.getMajorGroupOrder()) {
            // print layer
            std::cout << std::endl;
            for (std::size_t i=0; i<mid_frag_idx; ++i){
                std::cout << fragment[i]; // ex) model.layers.0
                if (i == mid_frag_idx-1) break;
                std::cout << '.';
            }
            
            // reallocate temporary variable 
            major_group_order_tmp = l.getMajorGroupOrder();
            layer_index_tmp = l.getLayerIndex();
            
        } else if (layer_index_tmp != l.getLayerIndex()){
            // print layer
            std::cout << std::endl;
            for (std::size_t i=0; i<mid_frag_idx; ++i){
                std::cout << fragment[i]; // ex) model.layers.0
                if (i == mid_frag_idx-1) break;
                std::cout << '.';
            }

            // reallocate temporary variable 
            layer_index_tmp = l.getLayerIndex();
        }


        // Load Parameters of the Corresponding Layer
        std::tuple<std::uint8_t *, std::uint64_t> param = paramLoader.load(l.getOriginalName()); // data (ptr), length
        
        // Print Operator and Layer Information
        // Specifying operator
	std::string str = " -- ";
        for (std::size_t i=mid_frag_idx; i<fragment.size(); ++i){
            str += fragment[i]; // ex) 
            if (i == fragment.size()-1) break;
            str += '.';
        }

	std::cout << std::endl << std::right << std::setw(4) << blk_num;
        std::cout << std::left << std::setw(36) << str;

        // Print Lyaer Information: dType | Size
        std::cout << " \t\t" <<
            DataTypeName(paramLoader.getDataType(l.getOriginalName())) << "\t" << 
            (double) (std::get<std::uint64_t>(param)) / (double)(1024) << " kB"; //" LENGTH";
	
	++blk_num;
    }
    std::cout << std::endl;

    return 0;
}
