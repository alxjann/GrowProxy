#include "Packets/TextPacket.h"

TextPacket::TextPacket(string_view text) : m_text(text) {
    string_view rest = m_text;
    while (!rest.empty()) {
        size_t newline = rest.find('\n');
        string_view line = rest.substr(0, newline);
        rest = newline == string_view::npos ? string_view() : rest.substr(newline + 1);

        // tolerate "\r\n", a leading "|" ("|text|hello") and a trailing "|"
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (!line.empty() && line.front() == '|')
            line.remove_prefix(1);
        if (!line.empty() && line.back() == '|')
            line.remove_suffix(1);

        size_t bar = line.find('|');
        if (bar == string_view::npos || bar == 0)
            continue;

        m_fields.emplace_back(line.substr(0, bar), line.substr(bar + 1));
    }
}

string_view TextPacket::Get(string_view key) const {
    for (const auto& [k, v] : m_fields) {
        if (k == key)
            return v;
    }
    return {};
}

bool TextPacket::Has(string_view key) const {
    for (const auto& field : m_fields) {
        if (field.first == key)
            return true;
    }
    return false;
}
