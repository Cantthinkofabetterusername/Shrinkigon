#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <string>

enum class Language {
    English = 0,
    Spanish = 1,
    German = 2,
    Italian = 3
};

constexpr int kLanguageCount = 4;

void setLanguage(Language language);
Language getLanguage();
const char* languageCode(Language language);
const char* languageNativeName(Language language);
Language languageFromCode(const std::string& code, bool* valid = nullptr);
const char* tr(const char* key);

#endif
