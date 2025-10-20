#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
  #define USE_LOCALTIME_S
#else
  #define USE_LOCALTIME_R
#endif

namespace fs = std::filesystem;

std::string add_timestamp_to_filename(const std::string& output_path) {
    fs::path p(output_path);

    // current local time
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef USE_LOCALTIME_S
    localtime_s(&tm, &tt);        // Windows
#else
    localtime_r(&tt, &tm);        // POSIX
#endif

    // YYMMDD_HHMM 
    std::ostringstream stamp;
    stamp << std::put_time(&tm, "%y%m%d_%H%M"); // e.g., 251020_1730

    // 새 파일명: <stem>_<YYMMDD_HHMM><ext>
    std::string new_name = p.stem().string() + "_" + stamp.str() + p.extension().string();
    fs::path new_path = p.parent_path() / new_name; // join path

    return new_path.string();
}

// Returns a new string with all invalid UTF-8 sequences removed.
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