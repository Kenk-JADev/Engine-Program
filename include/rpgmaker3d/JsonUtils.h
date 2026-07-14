#pragma once
// Lightweight JSON helpers - no external dependency
// Handles simple JSON used by RPG Maker 3D (objects, arrays, strings, numbers, bools)

#include <string>
#include <vector>
#include <cctype>
#include <sstream>

namespace rpg {
namespace JsonUtils {

// Escape string for JSON output
inline std::string Escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

inline std::string Unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i+1];
            switch (n) {
                case '\"': out += '\"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    // Simple \uXXXX - ignore complex unicode, replace with ?
                    if (i + 5 < s.size()) {
                        out += '?';
                        i += 4;
                    }
                    break;
                }
                default: out += n; break;
            }
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

inline void SkipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
}

// Find key: looks for "key" (quoted). Returns true and sets keyPos to position of opening quote of key
inline bool FindKey(const std::string& json, const std::string& key, size_t from, size_t& keyPos) {
    std::string quoted = "\"" + key + "\"";
    size_t pos = json.find(quoted, from);
    if (pos == std::string::npos) return false;
    keyPos = pos;
    return true;
}

// Find colon after keyPos
inline bool FindColon(const std::string& json, size_t keyPos, size_t& colonPos) {
    colonPos = json.find(':', keyPos);
    return colonPos != std::string::npos;
}

// Parse value start after colon
inline size_t ValueStart(const std::string& json, size_t colonPos) {
    size_t p = colonPos + 1;
    SkipWhitespace(json, p);
    return p;
}

// Extract string value given position at opening quote. Returns string and sets endPos to after closing quote
inline bool ExtractString(const std::string& json, size_t startPos, std::string& out, size_t& endPos) {
    size_t pos = startPos;
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '\"') return false;
    size_t i = pos + 1;
    std::string raw;
    while (i < json.size()) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            raw += c;
            raw += json[i+1];
            i += 2;
            continue;
        }
        if (c == '\"') {
            out = Unescape(raw);
            endPos = i + 1;
            return true;
        }
        raw += c;
        ++i;
    }
    return false;
}

// Try parse string value for a key
inline bool TryParseString(const std::string& json, const std::string& key, size_t from, std::string& out) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    size_t end;
    if (!ExtractString(json, vs, out, end)) return false;
    return true;
}

inline bool TryParseBool(const std::string& json, const std::string& key, size_t from, bool& out) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    if (json.compare(vs, 4, "true") == 0) { out = true; return true; }
    if (json.compare(vs, 5, "false") == 0) { out = false; return true; }
    return false;
}

inline bool TryParseInt(const std::string& json, const std::string& key, size_t from, int& out) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    size_t end = vs;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end]=='-' || json[end]=='+')) ++end;
    if (end == vs) return false;
    try {
        out = std::stoi(json.substr(vs, end - vs));
        return true;
    } catch (...) { return false; }
}

inline bool TryParseFloat(const std::string& json, const std::string& key, size_t from, float& out) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    size_t end = vs;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end]=='-' || json[end]=='+' || json[end]=='.' || json[end]=='e' || json[end]=='E')) ++end;
    if (end == vs) return false;
    try {
        out = std::stof(json.substr(vs, end - vs));
        return true;
    } catch (...) { return false; }
}

// Generic: parse int at position (not by key)
inline bool ParseIntAt(const std::string& json, size_t pos, int& out, size_t& nextPos) {
    SkipWhitespace(json, pos);
    size_t end = pos;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end]=='-' || json[end]=='+')) ++end;
    if (end == pos) return false;
    try {
        out = std::stoi(json.substr(pos, end - pos));
        nextPos = end;
        return true;
    } catch (...) { return false; }
}

// Extract object that starts at '{' -> returns object string including braces and sets end position after '}'
inline bool ExtractObject(const std::string& json, size_t startPos, std::string& outObj, size_t& endPos) {
    size_t pos = startPos;
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{') return false;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    size_t i = pos;
    for (; i < json.size(); ++i) {
        char c = json[i];
        if (escape) { escape = false; continue; }
        if (c == '\\' && inString) { escape = true; continue; }
        if (c == '\"') { inString = !inString; continue; }
        if (inString) continue;
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) {
                outObj = json.substr(pos, i - pos + 1);
                endPos = i + 1;
                return true;
            }
        }
    }
    return false;
}

// Extract array that starts at '[' -> returns content inside? We'll return whole array string
inline bool ExtractArray(const std::string& json, size_t startPos, std::string& outArr, size_t& endPos) {
    size_t pos = startPos;
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '[') return false;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    size_t i = pos;
    for (; i < json.size(); ++i) {
        char c = json[i];
        if (escape) { escape = false; continue; }
        if (c == '\\' && inString) { escape = true; continue; }
        if (c == '\"') { inString = !inString; continue; }
        if (inString) continue;
        if (c == '[') depth++;
        else if (c == ']') {
            depth--;
            if (depth == 0) {
                outArr = json.substr(pos, i - pos + 1);
                endPos = i + 1;
                return true;
            }
        }
    }
    return false;
}

// Extract all top-level objects inside an array string "[ {...}, {...} ]" -> vector of object strings
inline std::vector<std::string> ExtractObjectsFromArray(const std::string& arrayJson) {
    std::vector<std::string> objs;
    size_t pos = 0;
    SkipWhitespace(arrayJson, pos);
    if (pos >= arrayJson.size() || arrayJson[pos] != '[') return objs;
    ++pos; // after [
    while (pos < arrayJson.size()) {
        SkipWhitespace(arrayJson, pos);
        if (pos >= arrayJson.size()) break;
        if (arrayJson[pos] == ']') break;
        if (arrayJson[pos] == ',') { ++pos; continue; }
        if (arrayJson[pos] == '{') {
            std::string obj;
            size_t end;
            if (ExtractObject(arrayJson, pos, obj, end)) {
                objs.push_back(obj);
                pos = end;
            } else {
                break;
            }
        } else {
            // Unexpected token, skip
            ++pos;
        }
    }
    return objs;
}

// Find array for a given key and return its raw string "[ ... ]"
inline bool FindArrayForKey(const std::string& json, const std::string& key, size_t from, std::string& outArray) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    size_t end;
    if (!ExtractArray(json, vs, outArray, end)) return false;
    return true;
}

// Find object for a given key
inline bool FindObjectForKey(const std::string& json, const std::string& key, size_t from, std::string& outObj) {
    size_t keyPos;
    if (!FindKey(json, key, from, keyPos)) return false;
    size_t colon;
    if (!FindColon(json, keyPos, colon)) return false;
    size_t vs = ValueStart(json, colon);
    size_t end;
    if (!ExtractObject(json, vs, outObj, end)) return false;
    return true;
}

// Parse Vec3 from "[x,y,z]" string - very tolerant
inline bool ParseVec3(const std::string& json, const std::string& key, size_t from, Vec3& out) {
    std::string arr;
    if (!FindArrayForKey(json, key, from, arr)) return false;
    // arr is like "[1, 2, 3]"
    // Remove brackets and split by comma
    size_t p1 = arr.find('[');
    size_t p2 = arr.rfind(']');
    if (p1 == std::string::npos || p2 == std::string::npos || p2 <= p1) return false;
    std::string inner = arr.substr(p1 + 1, p2 - p1 - 1);
    std::stringstream ss(inner);
    std::string token;
    float vals[3] = {0,0,0};
    int idx = 0;
    while (std::getline(ss, token, ',') && idx < 3) {
        try {
            // Trim
            size_t s = 0;
            while (s < token.size() && std::isspace(static_cast<unsigned char>(token[s]))) ++s;
            size_t e = token.size();
            while (e > s && std::isspace(static_cast<unsigned char>(token[e-1]))) --e;
            std::string trimmed = token.substr(s, e - s);
            if (!trimmed.empty())
                vals[idx] = std::stof(trimmed);
            ++idx;
        } catch (...) { ++idx; }
    }
    if (idx >= 2) {
        out = Vec3(vals[0], vals[1], vals[2]);
        return true;
    }
    return false;
}

} // namespace JsonUtils
} // namespace rpg
