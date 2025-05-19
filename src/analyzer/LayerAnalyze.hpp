//
// Created by Jinhwi Kim on 25-05-19.
//

#ifndef MLLM_LAYER_ANALYZE
#define MLLM_LAYER_ANALYZE

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

// Helper function to split a string by a delimiter
static std::vector<std::string> split(const std::string& s, char delimiter){
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)){
        tokens.push_back(token);
    }
    return tokens;
}

class LayerInfo{
private:
    std::string original_name_data;
    std::vector<std::string> fragments;
    int major_group_order;
    int layer_index;
    int sub_group_order;
    int component_order;
    int param_order;

    // private helper method for parsing
    void parse();

public:
    // Constructor
    LayerInfo(const std::string& name) : 
        original_name_data(name), major_group_order(-1), // will be set by parser
        layer_index(-1), sub_group_order(-1), component_order(-1), param_order(-1)
    {
        fragments = split(name, '.');
        parse();
    }

    // Getter for the original name
    std::string getOriginalName() const;
    std::vector<std::string> getNameFragments() const;
    const int getMajorGroupOrder() const;
    const int getLayerIndex() const;
    const int getSubGroupOrder() const;
    bool operator<(const LayerInfo& other) const;

};

#endif