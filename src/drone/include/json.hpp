// json.hpp -- a tiny, dependency-free JSON library for the drone body.
//
// This is deliberately small: it supports exactly what the brain's HTTP
// contract needs (objects, arrays, strings, numbers, bools, null) with correct
// string escaping and UTF-8 handling. It is not a general-purpose library, but
// it is self-contained, so the body builds with plain g++ and no external deps.
//
// Parsing throws std::runtime_error on malformed input; brain_client catches
// that and converts it into a safe fallback proposal.
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <cmath>

namespace mj {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool bval = false;
    double num = 0.0;
    std::string sval;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj;  // insertion-ordered

    Value() = default;

    static Value Bool(bool b)              { Value v; v.type = Type::Bool;   v.bval = b; return v; }
    static Value Num(double n)             { Value v; v.type = Type::Number; v.num = n;  return v; }
    static Value Str(const std::string& s) { Value v; v.type = Type::String; v.sval = s; return v; }
    static Value Array()                   { Value v; v.type = Type::Array;  return v; }
    static Value Object()                  { Value v; v.type = Type::Object; return v; }

    bool is_null()   const { return type == Type::Null; }
    bool is_object() const { return type == Type::Object; }
    bool is_array()  const { return type == Type::Array; }
    bool is_string() const { return type == Type::String; }
    bool is_number() const { return type == Type::Number; }
    bool is_bool()   const { return type == Type::Bool; }

    // object helpers
    void set(const std::string& k, const Value& val) {
        for (auto& kv : obj) if (kv.first == k) { kv.second = val; return; }
        obj.emplace_back(k, val);
    }
    const Value* find(const std::string& k) const {
        for (auto& kv : obj) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    // array helper
    void push_back(const Value& val) { arr.push_back(val); }

    // typed getters with defaults (never throw)
    std::string as_string(const std::string& def = "") const { return type == Type::String ? sval : def; }
    double      as_number(double def = 0.0)            const { return type == Type::Number ? num  : def; }
    bool        as_bool(bool def = false)              const { return type == Type::Bool   ? bval : def; }

    std::string dump() const { std::ostringstream os; write(os); return os.str(); }

    void write(std::ostringstream& os) const {
        switch (type) {
            case Type::Null:   os << "null"; break;
            case Type::Bool:   os << (bval ? "true" : "false"); break;
            case Type::Number:
                if (std::isnan(num) || std::isinf(num)) { os << "0"; }
                else { std::ostringstream t; t.precision(15); t << num; os << t.str(); }
                break;
            case Type::String: write_string(os, sval); break;
            case Type::Array:
                os << '[';
                for (size_t i = 0; i < arr.size(); ++i) { if (i) os << ','; arr[i].write(os); }
                os << ']';
                break;
            case Type::Object:
                os << '{';
                for (size_t i = 0; i < obj.size(); ++i) {
                    if (i) os << ',';
                    write_string(os, obj[i].first);
                    os << ':';
                    obj[i].second.write(os);
                }
                os << '}';
                break;
        }
    }

    static void write_string(std::ostringstream& os, const std::string& s) {
        static const char* hex = "0123456789abcdef";
        os << '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"':  os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\b': os << "\\b";  break;
                case '\f': os << "\\f";  break;
                case '\n': os << "\\n";  break;
                case '\r': os << "\\r";  break;
                case '\t': os << "\\t";  break;
                default:
                    if (c < 0x20) os << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                    else          os << static_cast<char>(c);
            }
        }
        os << '"';
    }
};

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s), i_(0) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        skip_ws();
        if (i_ != s_.size()) err("trailing characters");
        return v;
    }

private:
    const std::string& s_;
    size_t i_;

    [[noreturn]] void err(const std::string& m) { throw std::runtime_error("json: " + m); }
    void skip_ws() { while (i_ < s_.size()) { char c = s_[i_]; if (c==' '||c=='\t'||c=='\n'||c=='\r') ++i_; else break; } }
    char peek() { if (i_ >= s_.size()) err("unexpected end"); return s_[i_]; }
    char get()  { if (i_ >= s_.size()) err("unexpected end"); return s_[i_++]; }

    Value parse_value() {
        skip_ws();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return Value::Str(parse_string());
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        err(std::string("unexpected character '") + c + "'");
        return Value();  // unreachable
    }

    Value parse_object() {
        Value v = Value::Object();
        get();  // '{'
        skip_ws();
        if (peek() == '}') { get(); return v; }
        while (true) {
            skip_ws();
            if (peek() != '"') err("expected string key");
            std::string key = parse_string();
            skip_ws();
            if (get() != ':') err("expected ':'");
            v.set(key, parse_value());
            skip_ws();
            char c = get();
            if (c == '}') break;
            if (c != ',') err("expected ',' or '}'");
        }
        return v;
    }

    Value parse_array() {
        Value v = Value::Array();
        get();  // '['
        skip_ws();
        if (peek() == ']') { get(); return v; }
        while (true) {
            v.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ']') break;
            if (c != ',') err("expected ',' or ']'");
        }
        return v;
    }

    Value parse_bool() {
        if (s_.compare(i_, 4, "true")  == 0) { i_ += 4; return Value::Bool(true); }
        if (s_.compare(i_, 5, "false") == 0) { i_ += 5; return Value::Bool(false); }
        err("invalid literal");
        return Value();
    }

    Value parse_null() {
        if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return Value(); }
        err("invalid literal");
        return Value();
    }

    Value parse_number() {
        size_t start = i_;
        if (peek() == '-') get();
        while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        if (i_ < s_.size() && s_[i_] == '.') { ++i_; while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_; }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        }
        std::string tok = s_.substr(start, i_ - start);
        double d = 0.0;
        try { d = std::stod(tok); } catch (...) { err("invalid number"); }
        return Value::Num(d);
    }

    static void append_utf8(std::string& out, uint32_t cp) {
        if      (cp <= 0x7F)   { out.push_back(static_cast<char>(cp)); }
        else if (cp <= 0x7FF)  { out.push_back(static_cast<char>(0xC0 | (cp >> 6)));  out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp <= 0xFFFF) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else                   { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }

    unsigned hex4() {
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = get(); v <<= 4;
            if      (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else err("bad \\u escape");
        }
        return v;
    }

    std::string parse_string() {
        get();  // opening quote
        std::string out;
        while (true) {
            if (i_ >= s_.size()) err("unterminated string");
            char c = s_[i_++];
            if (c == '"') break;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = hex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate
                            if (get() != '\\' || get() != 'u') err("expected low surrogate");
                            uint32_t lo = hex4();
                            if (lo < 0xDC00 || lo > 0xDFFF) err("invalid low surrogate");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: err("bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
};

inline Value parse(const std::string& text) { Parser p(text); return p.parse(); }

}  // namespace mj
