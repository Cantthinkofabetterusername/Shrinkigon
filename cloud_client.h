#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <atomic>
#include <string>
#include <vector>

#include "mesh_data.h"
#include "optimization_algorithms.h"

struct CloudResponse {
    bool ok = false;
    std::string code;
    std::string token;
    std::string username;
    std::string language;
    bool speedigon = false;
    std::string payload;
};

class CloudClient {
public:
    static void globalInit();
    static void globalCleanup();
    static bool isConfigured();
    static bool isSpeedigonConfigured();

    static CloudResponse registerAccount(
        const std::string& username,
        const std::string& password,
        const std::string& languageCode
    );

    static CloudResponse login(
        const std::string& username,
        const std::string& password
    );

    static CloudResponse fetchProfile(const std::string& token);

    static CloudResponse updateLanguage(
        const std::string& token,
        const std::string& languageCode
    );

    static CloudResponse purchaseSpeedigon(const std::string& token);

    static CloudResponse optimizeRemote(
        const std::string& token,
        const std::vector<MeshData>& meshes,
        const DecimationConfig& config,
        std::vector<MeshData>& result,
        const std::atomic<bool>* cancelFlag = nullptr
    );
};

#endif