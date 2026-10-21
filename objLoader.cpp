#define NOMINMAX
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include <Windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include "tiny_obj_loader.h"

using namespace DirectX;

// PDF と同じ頂点レイアウト。
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float tx, ty, tz;
    float nx, ny, nz;
};

// 三角形の位置と UV から接線を計算する。
XMFLOAT3 CalculateTangent(const Vertex& v0, const Vertex& v1, const Vertex& v2)
{
    const XMVECTOR p0 = XMVectorSet(v0.x, v0.y, v0.z, 0.0f);
    const XMVECTOR e1 = XMVectorSubtract(XMVectorSet(v1.x, v1.y, v1.z, 0.0f), p0);
    const XMVECTOR e2 = XMVectorSubtract(XMVectorSet(v2.x, v2.y, v2.z, 0.0f), p0);
    const float du1 = v1.u - v0.u, dv1 = v1.v - v0.v;
    const float du2 = v2.u - v0.u, dv2 = v2.v - v0.v;
    const float det = du1 * dv2 - du2 * dv1;

    XMVECTOR normal = XMVectorSet(v0.nx, v0.ny, v0.nz, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(normal)) < 1e-16f)
        normal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    normal = XMVector3Normalize(normal);

    XMVECTOR tangent = XMVectorZero();
    if (std::fabs(det) >= 1e-8f)
    {
        tangent = XMVectorScale(
            XMVectorSubtract(XMVectorScale(e1, dv2), XMVectorScale(e2, dv1)),
            1.0f / det);
        // Gram-Schmidt 法で法線方向の成分を取り除く。
        tangent = XMVectorSubtract(tangent,
            XMVectorScale(normal, XMVectorGetX(XMVector3Dot(normal, tangent))));
    }
    // UV がない場合や退化している場合も、ゼロ除算を避ける。
    if (XMVectorGetX(XMVector3LengthSq(tangent)) < 1e-16f)
    {
        const XMVECTOR axis = std::fabs(XMVectorGetX(normal)) < 0.9f
            ? XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)
            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        tangent = XMVectorSubtract(axis,
            XMVectorScale(normal, XMVectorGetX(XMVector3Dot(normal, axis))));
    }
    XMFLOAT3 result;
    XMStoreFloat3(&result, XMVector3Normalize(tangent));
    return result;
}

std::vector<Vertex> modelVertices;
std::vector<uint32_t> modelIndices;

bool LoadOBJ(const char* filename,
    std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
    vertices.clear();
    indices.clear();
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    if (!reader.ParseFromFile(filename, config))
    {
        std::cerr << "OBJ load failed: " << filename << '\n' << reader.Error() << '\n';
        return false;
    }
    if (!reader.Warning().empty())
        std::cerr << "OBJ warning: " << reader.Warning() << '\n';

    const auto& attrib = reader.GetAttrib();
    // 失敗した場合、呼び出し元に途中のデータを残さない。
    std::vector<Vertex> loadedVertices;
    std::vector<uint32_t> loadedIndices;
    for (const auto& shape : reader.GetShapes())
    {
        for (const auto& idx : shape.mesh.indices)
        {
            if (idx.vertex_index < 0 ||
                static_cast<size_t>(idx.vertex_index) >= attrib.vertices.size() / 3 ||
                (idx.texcoord_index >= 0 &&
                    static_cast<size_t>(idx.texcoord_index) >= attrib.texcoords.size() / 2) ||
                (idx.normal_index >= 0 &&
                    static_cast<size_t>(idx.normal_index) >= attrib.normals.size() / 3) ||
                loadedVertices.size() >= (std::numeric_limits<uint32_t>::max)())
            {
                std::cerr << "Invalid OBJ index: " << filename << '\n';
                return false;
            }
            Vertex v{};
            const size_t p = 3 * static_cast<size_t>(idx.vertex_index);
            v.x = attrib.vertices[p];
            v.y = attrib.vertices[p + 1];
            v.z = attrib.vertices[p + 2];
            v.r = v.g = v.b = v.a = 1.0f;
            if (idx.texcoord_index >= 0)
            {
                const size_t t = 2 * static_cast<size_t>(idx.texcoord_index);
                v.u = attrib.texcoords[t];
                v.v = 1.0f - attrib.texcoords[t + 1]; // DirectX 用に V を反転
            }
            if (idx.normal_index >= 0)
            {
                const size_t n = 3 * static_cast<size_t>(idx.normal_index);
                v.nx = attrib.normals[n];
                v.ny = attrib.normals[n + 1];
                v.nz = attrib.normals[n + 2];
            }
            else
                v.nz = 1.0f;
            loadedIndices.push_back(static_cast<uint32_t>(loadedVertices.size()));
            loadedVertices.push_back(v);
        }
    }
    if (loadedVertices.empty() || loadedIndices.size() % 3 != 0)
    {
        std::cerr << "OBJ has no valid triangles: " << filename << '\n';
        return false;
    }
    for (size_t i = 0; i + 2 < loadedVertices.size(); i += 3)
    {
        const XMFLOAT3 tangent = CalculateTangent(
            loadedVertices[i], loadedVertices[i + 1], loadedVertices[i + 2]);
        for (size_t j = 0; j < 3; ++j)
        {
            loadedVertices[i + j].tx = tangent.x;
            loadedVertices[i + j].ty = tangent.y;
            loadedVertices[i + j].tz = tangent.z;
        }
    }
    vertices.swap(loadedVertices);
    indices.swap(loadedIndices);
    return true;
}

void PrintModel(const std::string& filename)
{
    std::cout << "\nModel " << std::filesystem::path(filename).stem().string() << '\n'
              << "Vertices: " << modelVertices.size()
              << "  Indices: " << modelIndices.size()
              << "  Triangles: " << modelIndices.size() / 3 << '\n';
    for (const auto& v : modelVertices)
    {
        std::cout << "x :" << v.x << " y :" << v.y << " z :" << v.z
                  << " r :" << v.r << " g :" << v.g << " b :" << v.b << " a :" << v.a
                  << " u :" << v.u << " v :" << v.v
                  << " nx :" << v.nx << " ny :" << v.ny << " nz :" << v.nz
                  << " tx :" << v.tx << " ty :" << v.ty << " tz :" << v.tz << '\n';
    }
    for (size_t i = 0; i + 2 < modelIndices.size(); i += 3)
        std::cout << "[ " << modelIndices[i] << ' ' << modelIndices[i + 1]
                  << ' ' << modelIndices[i + 2] << " ]\n";
}

std::filesystem::path DefaultModelPath(const char* name)
{
    const auto relative = std::filesystem::path("Models") / name;
    if (std::filesystem::exists(relative))
        return relative;
    // 別の作業フォルダーから起動した場合は exe の隣を探す。
    wchar_t executable[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
        return std::filesystem::path(executable).parent_path() / relative;
    return relative;
}

int main(int argc, char* argv[])
{
    std::cout << std::fixed << std::setprecision(6);
    std::vector<std::string> filenames;
    if (argc > 1)
        filenames.assign(argv + 1, argv + argc);
    else
        filenames = { DefaultModelPath("cube.obj").string(),
                      DefaultModelPath("sphere.obj").string() };
    bool success = true;
    for (const auto& filename : filenames)
    {
        if (LoadOBJ(filename.c_str(), modelVertices, modelIndices))
            PrintModel(filename);
        else
            success = false;
    }
    return success ? 0 : 1;
}
