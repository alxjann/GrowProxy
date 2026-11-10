#include "Utils/Strings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

string_view Strings::Trim(string_view text) {
    const char* whitespace = " \t\r\n";
    size_t begin = text.find_first_not_of(whitespace);
    if (begin == string_view::npos)
        return {};
    size_t end = text.find_last_not_of(whitespace);
    return text.substr(begin, end - begin + 1);
}

string Strings::ToLower(string_view text) {
    string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

bool Strings::StartsWith(string_view text, string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

string Strings::StripColorCodes(string_view text) {
    string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '`' && i + 1 < text.size())
            ++i;
        else
            out += text[i];
    }
    return out;
}

std::optional<int32> Strings::ToInt(string_view text) {
    text = Trim(text);
    if (!text.empty() && text[0] == '+')
        text.remove_prefix(1);

    int32 value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc() || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

std::optional<float> Strings::ToFloat(string_view text) {
    string copy(Trim(text));
    char* end = nullptr;
    float value = std::strtof(copy.c_str(), &end);
    if (copy.empty() || *end != '\0')
        return std::nullopt;
    return value;
}
