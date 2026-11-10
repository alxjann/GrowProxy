#include "Packets/VariantList.h"

#include "Utils/ByteReader.h"

#include <cstring>

// Wire format: uint8 count, then per value: uint8 index, uint8 type, data.
namespace {

    enum WireType : uint8 {
        WIRE_FLOAT = 1,
        WIRE_STRING = 2, // uint32 length + bytes
        WIRE_VEC2 = 3,
        WIRE_VEC3 = 4,
        WIRE_UINT = 5,
        WIRE_INT = 9,
    };

    template <typename T> void Append(std::vector<uint8>& out, const T& value) {
        const uint8* bytes = (const uint8*)&value;
        out.insert(out.end(), bytes, bytes + sizeof(T));
    }

}

bool VariantList::Parse(const uint8* data, size_t size) {
    m_values.clear();

    ByteReader reader(data, size);
    uint8 count = reader.Read<uint8>();
    m_values.resize(count);

    for (uint8 i = 0; i < count && reader.Ok(); ++i) {
        reader.Read<uint8>(); // index, always equal to i
        switch (reader.Read<uint8>()) {
            case WIRE_FLOAT:
                m_values[i] = reader.Read<float>();
                break;

            case WIRE_STRING: {
                uint32 length = reader.Read<uint32>();
                string text(length <= reader.Remaining() ? length : 0, '\0');
                reader.ReadBytes(text.data(), length);
                m_values[i] = std::move(text);
                break;
            }

            case WIRE_VEC2:
                m_values[i] = reader.Read<Vec2f>();
                break;

            case WIRE_VEC3:
                m_values[i] = reader.Read<Vec3f>();
                break;

            case WIRE_UINT:
                m_values[i] = reader.Read<uint32>();
                break;

            case WIRE_INT:
                m_values[i] = reader.Read<int32>();
                break;

            default:
                return false;
        }
    }
    return reader.Ok();
}

std::vector<uint8> VariantList::Serialize() const {
    std::vector<uint8> out;
    out.push_back(0); // count, filled in below

    uint8 count = 0;
    for (size_t i = 0; i < m_values.size(); ++i) {
        const Variant& value = m_values[i];
        if (std::holds_alternative<std::monostate>(value))
            continue;

        out.push_back((uint8)i);

        if (auto* f = std::get_if<float>(&value)) {
            out.push_back(WIRE_FLOAT);
            Append(out, *f);
        } else if (auto* s = std::get_if<string>(&value)) {
            out.push_back(WIRE_STRING);
            Append(out, (uint32)s->size());
            out.insert(out.end(), s->begin(), s->end());
        } else if (auto* v2 = std::get_if<Vec2f>(&value)) {
            out.push_back(WIRE_VEC2);
            Append(out, *v2);
        } else if (auto* v3 = std::get_if<Vec3f>(&value)) {
            out.push_back(WIRE_VEC3);
            Append(out, *v3);
        } else if (auto* u = std::get_if<uint32>(&value)) {
            out.push_back(WIRE_UINT);
            Append(out, *u);
        } else if (auto* n = std::get_if<int32>(&value)) {
            out.push_back(WIRE_INT);
            Append(out, *n);
        }
        ++count;
    }

    out[0] = count;
    return out;
}

const string& VariantList::GetString(size_t index) const {
    static const string empty;
    if (index >= m_values.size())
        return empty;
    auto* s = std::get_if<string>(&m_values[index]);
    return s ? *s : empty;
}

int32 VariantList::GetInt(size_t index) const {
    if (index >= m_values.size())
        return 0;
    if (auto* n = std::get_if<int32>(&m_values[index]))
        return *n;
    if (auto* u = std::get_if<uint32>(&m_values[index]))
        return (int32)*u;
    return 0;
}

uint32 VariantList::GetUInt(size_t index) const {
    return (uint32)GetInt(index);
}
