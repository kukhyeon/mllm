#ifndef DVFS_H
#define DVFS_H

#include "device.h"
#include "utils.h"

#include <stdlib.h>

#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <sstream>

class Collector;

class Collector : public Device {
private:
    // pixel9
    // BIG: thermal/thermal_zone0
    // MID: thermal/thermal_zone1
    static const std::map<std::string, std::vector<std::string>> thermal_zones_cpu;
public:
    explicit Collector(const std::string& device_name);
    double collect_high_temp();

};

class DVFS : public Device {
private:
    static const std::map<std::string, std::map<int, std::vector<int>>> cpufreq;
    static const std::map<std::string, std::vector<int>> ddrfreq;
    static const std::map<std::string, std::vector<std::string>> empty_thermal;

public:
    std::string output_filename;
    int cur_ram_clk = 0; // Save current RAM Clock
public:
    explicit DVFS(const std::string& device_name);

    const std::map<int, std::vector<int>>& get_cpu_freq() const;
    const std::vector<int>& get_ddr_freq() const;
	const std::vector<std::string>& get_empty_thermal() const;

	int set_cpu_freq(const std::vector<int>&);
    int unset_cpu_freq();
    int set_ram_freq(const int freq_idx);
    int unset_ram_freq();
    void enforce_ram_freq() const; // Encofre reset RAM freq for prime-latfloor, this code only run on S25 or Snapdragon
    std::vector<int> get_cpu_freqs_conf(int prime_cpu_index);

    Collector get_collector() { return Collector(this->get_device_name()); }
};

#endif //DVFS_H