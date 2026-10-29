#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <string>

#include "localization.h"

struct AppSettings {
    Language language = Language::English;
    std::string username;
    std::string token;
};

AppSettings loadAppSettings();
void saveAppSettings(const AppSettings& settings);

#endif
