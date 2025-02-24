#pragma once

#include <string>
#include <algorithm>
#include <sstream>
#include <ctype.h>
#include <vector>
#include <random>
#include <map>
#include "wyhash_v5.h"

struct StringUtils {

    StringUtils() {

    }

    ~StringUtils() {

    }

    // Adapted from: http://stackoverflow.com/a/236180/131050
    static size_t split(const std::string& s, std::vector<std::string> & result, const std::string& delim,
                      const bool keep_empty = false, const size_t start_index = 0,
                      const size_t max_values = (std::numeric_limits<size_t>::max()-1)) {
        if (delim.empty()) {
            result.push_back(s);
            return s.size();
        }

        std::string::const_iterator substart = s.begin()+start_index, subend;
        size_t end_index = start_index;

        while (true) {
            subend = std::search(substart, s.end(), delim.begin(), delim.end());
            std::string temp(substart, subend);

            end_index += temp.size() + delim.size();
            temp = trim(temp);

            if (keep_empty || !temp.empty()) {
                result.push_back(temp);
            }

            if(result.size() == max_values) {
                break;
            }

            if (subend == s.end()) {
                break;
            }
            substart = subend + delim.size();
        }

        return std::min(end_index, s.size());
    }

    static std::string join(std::vector<std::string> vec, const std::string& delimiter, size_t start_index = 0) {
        std::stringstream ss;
        for(size_t i = start_index; i < vec.size(); i++) {
            if(i != start_index) {
                ss << delimiter;
            }
            ss << vec[i];
        }

        return ss.str();
    }

    // Adapted from: http://stackoverflow.com/a/36000453/131050
    static std::string & trim(std::string & str) {
        // right trim
        while (str.length () > 0 && (str [str.length ()-1] == ' ')) {
            str.erase (str.length ()-1, 1);
        }

        // left trim
        while (str.length () > 0 && (str [0] == ' ')) {
            str.erase (0, 1);
        }

        return str;
    }

    // URL decoding - adapted from: http://stackoverflow.com/a/32595923/131050

    static char from_hex(char ch) {
        return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
    }

    // Convert string of chars to its representative string of hex numbers
    static std::string str2hex(const std::string& str, bool capital = false);

    static std::string url_decode(const std::string& text) {
        char h;
        std::ostringstream escaped;
        escaped.fill('0');

        for (auto i = text.begin(), n = text.end(); i != n; ++i) {
            std::string::value_type c = (*i);

            if (c == '%') {
                if (i[1] && i[2]) {
                    h = from_hex(i[1]) << 4 | from_hex(i[2]);
                    escaped << h;
                    i += 2;
                }
            } else if (c == '+') {
                escaped << ' ';
            } else {
                escaped << c;
            }
        }

        return escaped.str();
    }

    // See: https://stackoverflow.com/a/19751887/131050
    static bool is_float(const std::string &s) {
        std::string::const_iterator it = s.begin();
        bool decimalPoint = false;
        size_t minSize = 0;
        if(s.size() > 0 && (s[0] == '-' || s[0] == '+')) {
            it++;
            minSize++;
        }

        while(it != s.end()){
            if(*it == '.') {
                if(!decimalPoint) decimalPoint = true;
                else break;
            } else if(!std::isdigit(*it) && ((*it!='f') || it+1 != s.end() || !decimalPoint)) {
                break;
            }
            ++it;
        }
        return s.size() > minSize && it == s.end();
    }

    // Adapted from: http://stackoverflow.com/a/2845275/131050
    static bool is_integer(const std::string &s) {
        if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) {
            return false;
        }

        char * p ;
        strtol(s.c_str(), &p, 10);
        return (*p == 0);
    }

    static bool is_positive_integer(const std::string& s) {
        return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
    }

    // Adapted from: http://stackoverflow.com/a/2845275/131050
    static bool is_uint64_t(const std::string &s) {
        if(s.empty()) {
            return false;
        }

        char * p ;
        unsigned long long ull = strtoull(s.c_str(), &p, 10);
        return (*p == 0) && ull <= std::numeric_limits<uint64_t>::max();
    }

    static bool is_int64_t(const std::string &s) {
        if(s.empty()) {
            return false;
        }

        char * p ;
        long long val = strtoll(s.c_str(), &p, 10);
        return (*p == 0) && val >= std::numeric_limits<int64_t>::min() && val <= std::numeric_limits<int64_t>::max();
    }

    static bool is_uint32_t(const std::string &s) {
        if(s.empty()) {
            return false;
        }

        char * p ;
        unsigned long ul = strtoul(s.c_str(), &p, 10);
        return (*p == 0) && ul <= std::numeric_limits<uint32_t>::max();
    }

    static bool is_int32_t(const std::string &s) {
        if(s.empty()) {
            return false;
        }

        char * p ;
        long val = strtol(s.c_str(), &p, 10);
        return (*p == 0) && val >= std::numeric_limits<int32_t>::min() && val <= std::numeric_limits<int32_t>::max();
    }

    static bool is_bool(std::string &s) {
        if(s.empty()) {
            return false;
        }

        StringUtils::tolowercase(s);
        return s == "true" || s == "false";
    }

    static void toupper(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    }

    static void tolowercase(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    }

    /* https://stackoverflow.com/a/34571089/131050 */
    static std::string base64_encode(const std::string &in) {
        std::string out;

        int val=0, valb=-6;
        for(unsigned char c : in) {
            val = (val<<8) + c;
            valb += 8;
            while (valb>=0) {
                out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
                valb-=6;
            }
        }

        if(valb>-6) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
        }
        while(out.size()%4) {
            out.push_back('=');
        }

        return out;
    }

    static std::string base64_decode(const std::string &in) {
        std::string out;

        std::vector<int> T(256,-1);
        for(int i=0; i<64; i++) {
            T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
        }

        int val=0, valb=-8;

        for(unsigned char c : in) {
            if (T[c] == -1) break;
            val = (val<<6) + T[c];
            valb += 6;
            if (valb>=0) {
                out.push_back(char((val>>valb)&0xFF));
                valb-=8;
            }
        }

        return out;
    }

    static std::string serialize_uint32_t(uint32_t num) {
        unsigned char bytes[4];
        bytes[0] = (unsigned char) ((num >> 24) & 0xFF);
        bytes[1] = (unsigned char) ((num >> 16) & 0xFF);
        bytes[2] = (unsigned char) ((num >> 8) & 0xFF);
        bytes[3] = (unsigned char) ((num & 0xFF));

        return std::string(bytes, bytes+4);
    }

    static uint32_t deserialize_uint32_t(std::string serialized_num) {
        uint32_t seq_id = ((serialized_num[0] & 0xFF) << 24) | ((serialized_num[1] & 0xFF) << 16) |
                          ((serialized_num[2] & 0xFF) << 8)  | (serialized_num[3] & 0xFF);
        return seq_id;
    }

    static uint64_t hash_wy(const void* key, uint64_t len) {
        uint64_t hash = wyhash(key, len, 0, _wyp);
        // reserve max() for use as a delimiter
        return hash != std::numeric_limits<uint64_t>::max() ? hash : (std::numeric_limits<uint64_t>::max()-1);
    }

    static std::string randstring(size_t length);

    static std::string hmac(const std::string& key, const std::string& msg);

    static std::string hash_sha256(const std::string& str);

    //static size_t unicode_length(const std::string& bytes);

    static bool begins_with(const std::string& str, const std::string& prefix) {
        return str.rfind(prefix, 0) == 0;
    }

    static std::map<std::string, std::string> parse_query_string(const std::string& query);
};