#pragma once

#include "Core/Types.h"

#include <utility>
#include <vector>

// "key|value" lines, as used by text messages, OnSpawn data and dialog returns.
class TextPacket {
public:
    explicit TextPacket(string_view text);

    // fields point into the owned text, so copying would leave them dangling
    TextPacket(const TextPacket&) = delete;
    TextPacket& operator=(const TextPacket&) = delete;

    // Value of the first field with this key, empty if missing.
    string_view Get(string_view key) const;
    bool Has(string_view key) const;

private:
    string m_text;
    std::vector<std::pair<string_view, string_view>> m_fields;
};
