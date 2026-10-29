#include <httplib.h>
#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mesh_chunking.h"
#include "mesh_codec.h"
#include "optimization_algorithms.h"

namespace {

constexpr int kServerTimeLimitSeconds = 50;
constexpr int kAuthCacheSeconds = 60;
constexpr size_t kChunkThresholdTriangles = 100000;
constexpr size_t kTrianglesPerChunk = 25000;
constexpr size_t kMaxPayloadBytes = 256u * 1024u * 1024u;

std::string authBaseUrl;
std::mutex jobMutex;
std::mutex cacheMutex;
std::map<std::string, std::chrono::steady_clock::time_point> validTokens;

size_t writeCallback(char* data, size_t size, size_t count, void* userData) {
    static_cast<std::string*>(userData)->append(data, size * count);

    return size * count;
}

size_t findValueStart(const std::string& body, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t position = body.find(pattern);

    while (position != std::string::npos) {
        size_t cursor = position + pattern.size();

        while (cursor < body.size() && (body[cursor] == ' ' || body[cursor] == '\n' || body[cursor] == '\t')) {
            ++cursor;
        }

        if (cursor < body.size() && body[cursor] == ':') {
            ++cursor;

            while (cursor < body.size() && (body[cursor] == ' ' || body[cursor] == '\n' || body[cursor] == '\t')) {
                ++cursor;
            }

            return cursor;
        }

        position = body.find(pattern, position + pattern.size());
    }

    return std::string::npos;
}

std::string jsonString(const std::string& body, const std::string& key) {
    size_t cursor = findValueStart(body, key);

    if (cursor == std::string::npos || cursor >= body.size() || body[cursor] != '"') {
        return "";
    }

    size_t end = body.find('"', cursor + 1);

    if (end == std::string::npos) {
        return "";
    }

    return body.substr(cursor + 1, end - cursor - 1);
}

double jsonNumber(const std::string& body, const std::string& key, double fallback) {
    size_t cursor = findValueStart(body, key);

    if (cursor == std::string::npos) {
        return fallback;
    }

    return std::atof(body.c_str() + cursor);
}

bool jsonBool(const std::string& body, const std::string& key, bool fallback) {
    size_t cursor = findValueStart(body, key);

    if (cursor == std::string::npos) {
        return fallback;
    }

    return body.compare(cursor, 4, "true") == 0;
}

enum class AuthResult {
    Ok,
    Unauthorized,
    NotOwned,
    Error
};

AuthResult checkToken(const std::string& token) {
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = validTokens.find(token);

        if (it != validTokens.end()) {
            if (std::chrono::steady_clock::now() < it->second) {
                return AuthResult::Ok;
            }

            validTokens.erase(it);
        }
    }

    CURL* curl = curl_easy_init();

    if (!curl) {
        return AuthResult::Error;
    }

    std::string url = authBaseUrl + "/me";
    std::string response;
    std::string header = "Authorization: Bearer " + token;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        return AuthResult::Error;
    }

    if (!jsonBool(response, "ok", false)) {
        return AuthResult::Unauthorized;
    }

    if (!jsonBool(response, "speedigon", false)) {
        return AuthResult::NotOwned;
    }

    std::lock_guard<std::mutex> lock(cacheMutex);
    validTokens[token] = std::chrono::steady_clock::now() + std::chrono::seconds(kAuthCacheSeconds);

    return AuthResult::Ok;
}

void reply(httplib::Response& res, int status, const std::string& body) {
    res.status = status;
    res.set_content(body, "application/json");
}

void replyError(httplib::Response& res, int status, const std::string& code) {
    reply(res, status, "{\"ok\":false,\"code\":\"" + code + "\"}");
}

void handleOptimize(const httplib::Request& req, httplib::Response& res) {
    std::string header = req.get_header_value("Authorization");
    std::string token = header.rfind("Bearer ", 0) == 0 ? header.substr(7) : "";

    if (token.empty()) {
        replyError(res, 401, "unauthorized");
        return;
    }

    AuthResult auth = checkToken(token);

    if (auth == AuthResult::Unauthorized) {
        replyError(res, 401, "unauthorized");
        return;
    }

    if (auth == AuthResult::NotOwned) {
        replyError(res, 403, "not_owned");
        return;
    }

    if (auth == AuthResult::Error) {
        replyError(res, 502, "server_error");
        return;
    }

    std::unique_lock<std::mutex> lock(jobMutex, std::try_to_lock);

    if (!lock.owns_lock()) {
        replyError(res, 503, "busy");
        return;
    }

    auto requestStart = std::chrono::steady_clock::now();

    std::vector<unsigned char> decoded;
    std::vector<MeshData> meshes;

    if (!base64Decode(jsonString(req.body, "payload"), decoded) || !deserializeMeshes(decoded, meshes)) {
        replyError(res, 400, "invalid_input");
        return;
    }

    std::atomic<bool> cancel{false};
    std::atomic<bool> finished{false};

    DecimationConfig config;
    config.targetTriangleCount = static_cast<int>(jsonNumber(req.body, "targetTriangleCount", config.targetTriangleCount));
    config.reductionPercentage = static_cast<float>(jsonNumber(req.body, "reductionPercentage", config.reductionPercentage));
    config.maxDeformationError = static_cast<float>(jsonNumber(req.body, "maxDeformationError", config.maxDeformationError));
    config.preventNormalFlipping = jsonBool(req.body, "preventNormalFlipping", config.preventNormalFlipping);
    config.preserveHardEdges = jsonBool(req.body, "preserveHardEdges", config.preserveHardEdges);
    config.hardEdgeAngleThreshold = static_cast<float>(jsonNumber(req.body, "hardEdgeAngleThreshold", config.hardEdgeAngleThreshold));
    config.lockBoundaryEdges = jsonBool(req.body, "lockBoundaryEdges", config.lockBoundaryEdges);
    config.preserveUVMapping = jsonBool(req.body, "preserveUVMapping", config.preserveUVMapping);
    config.cancelFlag = &cancel;

    auto decodeEnd = std::chrono::steady_clock::now();

    std::vector<MeshData> output = meshes;

    unsigned int cores = std::max(1u, std::thread::hardware_concurrency());

    std::thread watchdog([&]() {
        for (int i = 0; i < kServerTimeLimitSeconds * 10 && !finished.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!finished.load()) {
            cancel = true;
        }
    });

    std::vector<size_t> smallMeshes;
    size_t chunkedCount = 0;
    unsigned long long totalTriangles = 0;

    for (size_t i = 0; i < meshes.size(); ++i) {
        size_t triangles = meshes[i].indices.size() / 3;
        totalTriangles += triangles;

        if (triangles >= kChunkThresholdTriangles && cores > 1) {
            if (cancel.load()) {
                break;
            }

            int chunkCount = static_cast<int>(
                std::min<size_t>(
                    std::max<size_t>(triangles / kTrianglesPerChunk, 2),
                    512
                )
            );

            DecimationConfig chunkConfig = config;
            chunkConfig.lockBoundaryEdges = true;

            output[i] = decimateInChunks(
                meshes[i],
                chunkCount,
                cores,
                [&chunkConfig](const MeshData& chunk) {
                    return MeshOptimizer::incrementalDecimation(chunk, chunkConfig);
                },
                &cancel
            );

            ++chunkedCount;
        } else {
            smallMeshes.push_back(i);
        }
    }

    std::atomic<size_t> nextSmall{0};

    unsigned int workerCount = std::max(
        1u,
        std::min<unsigned int>(cores, static_cast<unsigned int>(std::max<size_t>(smallMeshes.size(), 1)))
    );

    std::vector<std::thread> workers;

    for (unsigned int w = 0; w < workerCount; ++w) {
        workers.emplace_back([&]() {
            while (!cancel.load()) {
                size_t position = nextSmall.fetch_add(1);

                if (position >= smallMeshes.size()) {
                    break;
                }

                size_t index = smallMeshes[position];

                output[index] = MeshOptimizer::incrementalDecimation(meshes[index], config);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    finished = true;
    watchdog.join();

    if (cancel.load()) {
        replyError(res, 504, "timeout");
        return;
    }

    auto optimizeEnd = std::chrono::steady_clock::now();

    std::string encoded = base64Encode(serializeMeshes(output));

    auto requestEnd = std::chrono::steady_clock::now();

    auto seconds = [](std::chrono::steady_clock::time_point from, std::chrono::steady_clock::time_point to) {
        return std::chrono::duration<double>(to - from).count();
    };

    std::cout
        << "[optimize] meshes=" << meshes.size()
        << " triangles=" << totalTriangles
        << " chunked_meshes=" << chunkedCount
        << " cores=" << cores
        << " decode=" << seconds(requestStart, decodeEnd) << "s"
        << " optimize=" << seconds(decodeEnd, optimizeEnd) << "s"
        << " encode=" << seconds(optimizeEnd, requestEnd) << "s"
        << std::endl;

    reply(res, 200, "{\"ok\":true,\"payload\":\"" + encoded + "\"}");
}

}

int main() {
    const char* authUrl = std::getenv("SPEEDIGON_AUTH_URL");

    if (!authUrl || !authUrl[0]) {
        std::cerr << "SPEEDIGON_AUTH_URL is not set" << std::endl;
        return 1;
    }

    authBaseUrl = authUrl;

    while (!authBaseUrl.empty() && authBaseUrl.back() == '/') {
        authBaseUrl.pop_back();
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    httplib::Server server;

    server.set_payload_max_length(kMaxPayloadBytes);
    server.set_read_timeout(30, 0);
    server.set_write_timeout(30, 0);

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        reply(res, 200, "{\"ok\":true}");
    });

    server.Post("/optimize", handleOptimize);

    std::cout << "Speedigon server listening on port 8080" << std::endl;

    server.listen("0.0.0.0", 8080);

    curl_global_cleanup();

    return 0;
}