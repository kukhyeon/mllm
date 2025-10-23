#ifndef HARD_UTILS_H
#define HARD_UTILS_H

#include <vector>
#include <string>

std::vector<std::string> parseCSVLine(const std::string& line);
std::vector<std::vector<std::string>> readCSV(const std::string& filename);
std::string joinPaths(const std::string& path1, const std::string& path2);
std::string replace(std::string origin, std::string target, std::string destination);

// internal static functions
std::vector<std::string> split_string(const std::string& str);
std::string execute_cmd(const char* cmd);

// throttling detection support
std::string apply_sudo_and_get(std::string command);

#endif // HARD_UTILS_H
