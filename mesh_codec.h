#ifndef MESH_CODEC_H
#define MESH_CODEC_H

#include <string>
#include <vector>

#include "mesh_data.h"

std::string base64Encode(const std::vector<unsigned char>& data);
bool base64Decode(const std::string& input, std::vector<unsigned char>& out);
std::vector<unsigned char> serializeMeshes(const std::vector<MeshData>& meshes);
bool deserializeMeshes(const std::vector<unsigned char>& buffer, std::vector<MeshData>& result);

#endif
