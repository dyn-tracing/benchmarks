#include "common.h"

std::vector<std::string> split_by_string(const std::string& str, const char* ch) {
    std::vector<std::string> tokens;
    // https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c

    auto start = 0;
    auto end = str.find(ch);
    while (end != std::string::npos) {
        tokens.push_back(strip_from_the_end(str.substr(start, end-start), '\n'));
        start = end + strlen(ch);
        end = str.find(ch, start);
    }
    tokens.push_back(strip_from_the_end(str.substr(start, end), '\n'));

    return tokens;
}

std::string hex_str(const std::string &data, const int len) {
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
        s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }

    return s;
}

bool is_same_hex_str(const std::string &data, const std::string &compare) {
    constexpr int len = 8;
    for (int i = 0; i < len; ++i) {
        if (compare[2 * i] != hexmap[(data[i] & 0xF0) >> 4]) {
            return false;
        }

        if (compare[2 * i + 1]  != hexmap[data[i] & 0x0F]) {
            return false;
        }
    }
    return true;
}

std::string strip_from_the_end(std::string object, char stripper) {
    if (!object.empty() && object[object.length()-1] == stripper) {
        object.erase(object.length()-1);
    }
    return object;
}

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}