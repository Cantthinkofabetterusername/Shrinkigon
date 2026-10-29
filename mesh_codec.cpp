#include "mesh_codec.h"

#include <cstdint>
#include <cstring>

namespace {

const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template <typename T>
void appendValue(std::vector<unsigned char>& buffer, T value) {
    unsigned char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template <typename T>
bool readValue(const std::vector<unsigned char>& buffer, size_t& offset, T& value) {
    if (offset + sizeof(T) > buffer.size()) {
        return false;
    }

    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    offset += sizeof(T);

    return true;
}

}

std::string base64Encode(const std::vector<unsigned char>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;

    while (i + 2 < data.size()) {
        unsigned int n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kBase64Chars[(n >> 18) & 63];
        out += kBase64Chars[(n >> 12) & 63];
        out += kBase64Chars[(n >> 6) & 63];
        out += kBase64Chars[n & 63];
        i += 3;
    }

    if (i + 1 == data.size()) {
        unsigned int n = data[i] << 16;
        out += kBase64Chars[(n >> 18) & 63];
        out += kBase64Chars[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == data.size()) {
        unsigned int n = (data[i] << 16) | (data[i + 1] << 8);
        out += kBase64Chars[(n >> 18) & 63];
        out += kBase64Chars[(n >> 12) & 63];
        out += kBase64Chars[(n >> 6) & 63];
        out += '=';
    }

    return out;
}

bool base64Decode(const std::string& input, std::vector<unsigned char>& out) {
    int table[256];

    for (int i = 0; i < 256; ++i) {
        table[i] = -1;
    }

    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kBase64Chars[i])] = i;
    }

    out.clear();
    out.reserve(input.size() * 3 / 4);

    unsigned int buffer = 0;
    int bits = 0;

    for (unsigned char c : input) {
        if (c == '=') {
            break;
        }

        int value = table[c];

        if (value < 0) {
            return false;
        }

        buffer = (buffer << 6) | static_cast<unsigned int>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buffer >> bits) & 0xFF));
        }
    }

    return true;
}

std::vector<unsigned char> serializeMeshes(const std::vector<MeshData>& meshes) {
    std::vector<unsigned char> buffer;

    appendValue<std::uint32_t>(buffer, static_cast<std::uint32_t>(meshes.size()));

    for (const auto& mesh : meshes) {
        appendValue<std::uint32_t>(buffer, static_cast<std::uint32_t>(mesh.vertices.size()));
        appendValue<std::uint32_t>(buffer, static_cast<std::uint32_t>(mesh.indices.size()));

        for (const auto& vertex : mesh.vertices) {
            appendValue<float>(buffer, vertex.pos.x);
            appendValue<float>(buffer, vertex.pos.y);
            appendValue<float>(buffer, vertex.pos.z);
            appendValue<float>(buffer, vertex.norm.x);
            appendValue<float>(buffer, vertex.norm.y);
            appendValue<float>(buffer, vertex.norm.z);
            appendValue<float>(buffer, vertex.u);
            appendValue<float>(buffer, vertex.v);
        }

        for (unsigned int index : mesh.indices) {
            appendValue<std::uint32_t>(buffer, static_cast<std::uint32_t>(index));
        }
    }

    return buffer;
}

bool deserializeMeshes(const std::vector<unsigned char>& buffer, std::vector<MeshData>& result) {
    size_t offset = 0;
    std::uint32_t meshCount = 0;

    if (!readValue(buffer, offset, meshCount)) {
        return false;
    }

    if (meshCount > buffer.size()) {
        return false;
    }

    result.assign(meshCount, MeshData());

    for (std::uint32_t m = 0; m < meshCount; ++m) {
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;

        if (!readValue(buffer, offset, vertexCount) || !readValue(buffer, offset, indexCount)) {
            return false;
        }

        size_t needed = static_cast<size_t>(vertexCount) * 32 + static_cast<size_t>(indexCount) * 4;

        if (offset + needed > buffer.size()) {
            return false;
        }

        MeshData& mesh = result[m];
        mesh.vertices.assign(vertexCount, Vertex());
        mesh.indices.assign(indexCount, 0);

        for (std::uint32_t i = 0; i < vertexCount; ++i) {
            Vertex& vertex = mesh.vertices[i];

            readValue(buffer, offset, vertex.pos.x);
            readValue(buffer, offset, vertex.pos.y);
            readValue(buffer, offset, vertex.pos.z);
            readValue(buffer, offset, vertex.norm.x);
            readValue(buffer, offset, vertex.norm.y);
            readValue(buffer, offset, vertex.norm.z);
            readValue(buffer, offset, vertex.u);
            readValue(buffer, offset, vertex.v);
        }

        for (std::uint32_t i = 0; i < indexCount; ++i) {
            std::uint32_t index = 0;

            readValue(buffer, offset, index);

            if (index >= vertexCount) {
                return false;
            }

            mesh.indices[i] = index;
        }
    }

    return true;
}
