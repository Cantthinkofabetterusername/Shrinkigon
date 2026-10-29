#include "app_settings.h"

#include <cstdlib>
#include <fstream>

namespace {

std::string settingsPath() {
    const char* home = std::getenv("HOME");

    if (home && home[0]) {
        return std::string(home) + "/.shrinkigon_settings";
    }

    return ".shrinkigon_settings";
}

}

AppSettings loadAppSettings() {
    AppSettings settings;
    std::ifstream file(settingsPath());

    if (!file) {
        return settings;
    }

    std::string line;

    while (std::getline(file, line)) {
        size_t separator = line.find('=');

        if (separator == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);

        if (key == "language") {
            bool valid = false;
            Language language = languageFromCode(value, &valid);

            if (valid) {
                settings.language = language;
            }
        } else if (key == "username") {
            settings.username = value;
        } else if (key == "token") {
            settings.token = value;
        }
    }

    return settings;
}

void saveAppSettings(const AppSettings& settings) {
    std::ofstream file(settingsPath(), std::ios::trunc);

    if (!file) {
        return;
    }

    file << "language=" << languageCode(settings.language) << "\n";
    file << "username=" << settings.username << "\n";
    file << "token=" << settings.token << "\n";
}
