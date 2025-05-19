//
// Created by Jinhwi Kim on 25-05-19.
//

#include "LayerAnalyze.hpp"

// private function
void LayerInfo::parse(){
    std::vector<std::string> parts = split(original_name_data, '.');

    if (parts.empty()) return;

    // Intialize with default values indicating 
    // "unknown" or "lowest priority" if not set
    major_group_order = 99;
    layer_index = -1;
    sub_group_order = 99;
    component_order = 99;
    param_order = 99; // e.g. 0 for bias, 1 for weight

    if (parts[0] == "lm_head") {
        major_group_order = 3;
        if (parts.size() > 1 && parts[1] == "weight") param_order = 1;
    } else if (parts[0] == "model") {
        if (parts.size() < 2) return;

        if (parts[1] == "embed_tokens") {
            major_group_order = 0;
            if (parts.size() > 2 && parts[2] == "weight") param_order = 1;
        } else if (parts[1] == "norm") {
            major_group_order = 2;
            if (parts.size() > 2 && parts[2] == "weight") param_order = 1;
        } else if (parts[1] == "layers") {
            major_group_order = 1;
            if (parts.size() < 3) return;
            try {
                layer_index = std::stoi(parts[2]);
            } catch (const std::invalid_argument& ia) {
                // Handle error or set a default
                layer_index = -1; 
            }

            if (parts.size() < 4) return;
            const std::string& sub_component_type = parts[3];
            if (sub_component_type == "input_layernorm") {
                sub_group_order = 0;
                if (parts.size() > 4 && parts[4] == "weight") param_order = 1;
            } else if (sub_component_type == "self_attn") {
                sub_group_order = 1;
                if (parts.size() < 5) return;
                const std::string& attn_comp = parts[4];
                if (attn_comp == "q_proj") component_order = 0;
                else if (attn_comp == "k_proj") component_order = 1;
                else if (attn_comp == "v_proj") component_order = 2;
                else if (attn_comp == "o_proj") component_order = 3;
                    
                if (parts.size() > 5) {
                    if (parts[5] == "bias") param_order = 0;
                    else if (parts[5] == "weight") param_order = 1;
                }
            } else if (sub_component_type == "post_attention_layernorm") {
                sub_group_order = 2;
                if (parts.size() > 4 && parts[4] == "weight") param_order = 1;
            } else if (sub_component_type == "mlp") {
                sub_group_order = 3;
                if (parts.size() < 5) return;
                const std::string& mlp_comp = parts[4];
                if (mlp_comp == "gate_proj") component_order = 0; // FFN_gate
                else if (mlp_comp == "up_proj") component_order = 1;   // FFN_up
                else if (mlp_comp == "down_proj") component_order = 2; // FFN_down

                if (parts.size() > 5 && parts[5] == "weight") param_order = 1;
            }
        }
    }
}


// public function
std::string LayerInfo::getOriginalName() const{
    return original_name_data;
}

std::vector<std::string> LayerInfo::getNameFragments() const{
    return fragments;
}

const int LayerInfo::getMajorGroupOrder() const {
    return major_group_order;
}

const int LayerInfo::getLayerIndex() const {
    return layer_index;
}

const int LayerInfo::getSubGroupOrder() const {
    return sub_group_order;
}

bool LayerInfo::operator<(const LayerInfo& other) const{
    if (major_group_order != other.major_group_order) return major_group_order < other.major_group_order;
    if (layer_index != other.layer_index) return layer_index < other.layer_index;
    if (sub_group_order != other.sub_group_order) return sub_group_order < other.sub_group_order;
    if (component_order != other.component_order) return component_order < other.component_order;
    if (param_order != other.param_order) return param_order < other.param_order;

    // Fallback comparison, should ideally not be reached if parsing is comprehensive
    return original_name_data < other.original_name_data;
}