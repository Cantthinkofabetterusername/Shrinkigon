#include "cloud_client.h"

#include <curl/curl.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cloud_config.h"
#include "mesh_codec.h"

namespace {

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);

    for (unsigned char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }

    return out;
}

size_t findKeyValueStart(const std::string& body, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t position = 0;

    while ((position = body.find(pattern, position)) != std::string::npos) {
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

        position += pattern.size();
    }

    return std::string::npos;
}

std::string jsonGetString(const std::string& body, const std::string& key) {
    size_t cursor = findKeyValueStart(body, key);

    if (cursor == std::string::npos || cursor >= body.size() || body[cursor] != '"') {
        return "";
    }

    ++cursor;
    std::string out;

    while (cursor < body.size() && body[cursor] != '"') {
        if (body[cursor] == '\\' && cursor + 1 < body.size()) {
            ++cursor;

            switch (body[cursor]) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                default: out += body[cursor];
            }
        } else {
            out += body[cursor];
        }

        ++cursor;
    }

    return out;
}

bool jsonGetBool(const std::string& body, const std::string& key) {
    size_t cursor = findKeyValueStart(body, key);

    if (cursor == std::string::npos) {
        return false;
    }

    return body.compare(cursor, 4, "true") == 0;
}

size_t writeCallback(char* data, size_t size, size_t count, void* userData) {
    std::string* target = static_cast<std::string*>(userData);
    target->append(data, size * count);

    return size * count;
}

int progressCallback(void* clientData, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const std::atomic<bool>* flag = static_cast<const std::atomic<bool>*>(clientData);

    return (flag && flag->load()) ? 1 : 0;
}

bool urlConfigured(const std::string& url) {
    return url.find("REPLACE_ME") == std::string::npos;
}

CloudResponse failure(const std::string& code) {
    CloudResponse response;
    response.ok = false;
    response.code = code;

    return response;
}

CloudResponse request(
    const std::string& method,
    const std::string& baseUrl,
    const std::string& path,
    const std::string& body,
    const std::string& token,
    long timeoutSeconds,
    const std::atomic<bool>* cancelFlag = nullptr
) {
    if (!urlConfigured(baseUrl)) {
        return failure("not_configured");
    }

    CURL* curl = curl_easy_init();

    if (!curl) {
        return failure("network");
    }

    std::string cleanBase = baseUrl;

    while (!cleanBase.empty() && cleanBase.back() == '/') {
        cleanBase.pop_back();
    }

    std::string url = cleanBase + path;
    std::string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Expect:");

    if (!token.empty()) {
        std::string auth = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (cancelFlag) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancelFlag);
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode result = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result == CURLE_ABORTED_BY_CALLBACK) {
        return failure("cancelled");
    }

    if (result != CURLE_OK) {
        return failure("network");
    }

    CloudResponse response;
    response.ok = jsonGetBool(responseBody, "ok");
    response.code = jsonGetString(responseBody, "code");
    response.token = jsonGetString(responseBody, "token");
    response.username = jsonGetString(responseBody, "username");
    response.language = jsonGetString(responseBody, "language");
    response.speedigon = jsonGetBool(responseBody, "speedigon");
    response.payload = jsonGetString(responseBody, "payload");

    if (!response.ok && response.code.empty()) {
        response.code = httpStatus >= 500 ? "server_error" : "unknown";
    }

    return response;
}

}

void CloudClient::globalInit() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void CloudClient::globalCleanup() {
    curl_global_cleanup();
}

bool CloudClient::isConfigured() {
    return urlConfigured(kCloudApiBaseUrl);
}

bool CloudClient::isSpeedigonConfigured() {
    return urlConfigured(kSpeedigonApiBaseUrl);
}

CloudResponse CloudClient::registerAccount(
    const std::string& username,
    const std::string& password,
    const std::string& languageCode
) {
    std::string body =
        "{\"username\":\"" + jsonEscape(username) +
        "\",\"password\":\"" + jsonEscape(password) +
        "\",\"language\":\"" + jsonEscape(languageCode) + "\"}";

    return request("POST", kCloudApiBaseUrl, "/register", body, "", kCloudAuthTimeoutSeconds);
}

CloudResponse CloudClient::login(
    const std::string& username,
    const std::string& password
) {
    std::string body =
        "{\"username\":\"" + jsonEscape(username) +
        "\",\"password\":\"" + jsonEscape(password) + "\"}";

    return request("POST", kCloudApiBaseUrl, "/login", body, "", kCloudAuthTimeoutSeconds);
}

CloudResponse CloudClient::fetchProfile(const std::string& token) {
    return request("GET", kCloudApiBaseUrl, "/me", "", token, kCloudAuthTimeoutSeconds);
}

CloudResponse CloudClient::updateLanguage(
    const std::string& token,
    const std::string& languageCode
) {
    std::string body = "{\"language\":\"" + jsonEscape(languageCode) + "\"}";

    return request("POST", kCloudApiBaseUrl, "/language", body, token, kCloudAuthTimeoutSeconds);
}

CloudResponse CloudClient::purchaseSpeedigon(const std::string& token) {
    return request("POST", kCloudApiBaseUrl, "/purchase", "{}", token, kCloudAuthTimeoutSeconds);
}

CloudResponse CloudClient::optimizeRemote(
    const std::string& token,
    const std::vector<MeshData>& meshes,
    const DecimationConfig& config,
    std::vector<MeshData>& result,
    const std::atomic<bool>* cancelFlag
) {
    std::string payload = base64Encode(serializeMeshes(meshes));

    auto boolText = [](bool value) {
        return value ? "true" : "false";
    };

    std::string body =
        "{\"config\":{"
        "\"targetTriangleCount\":" + std::to_string(config.targetTriangleCount) +
        ",\"reductionPercentage\":" + std::to_string(config.reductionPercentage) +
        ",\"maxDeformationError\":" + std::to_string(config.maxDeformationError) +
        ",\"preventNormalFlipping\":" + boolText(config.preventNormalFlipping) +
        ",\"preserveHardEdges\":" + boolText(config.preserveHardEdges) +
        ",\"hardEdgeAngleThreshold\":" + std::to_string(config.hardEdgeAngleThreshold) +
        ",\"lockBoundaryEdges\":" + boolText(config.lockBoundaryEdges) +
        ",\"preserveUVMapping\":" + boolText(config.preserveUVMapping) +
        "},\"payload\":\"" + payload + "\"}";

    CloudResponse response = request(
        "POST",
        kSpeedigonApiBaseUrl,
        "/optimize",
        body,
        token,
        kCloudOptimizeTimeoutSeconds,
        cancelFlag
    );

    if (!response.ok) {
        return response;
    }

    std::vector<unsigned char> decoded;
    std::vector<MeshData> optimized;

    if (
        !base64Decode(response.payload, decoded) ||
        !deserializeMeshes(decoded, optimized) ||
        optimized.size() != meshes.size()
    ) {
        return failure("server_error");
    }

    result = meshes;

    for (size_t i = 0; i < result.size(); ++i) {
        result[i].vertices = std::move(optimized[i].vertices);
        result[i].indices = std::move(optimized[i].indices);
    }

    return response;
}