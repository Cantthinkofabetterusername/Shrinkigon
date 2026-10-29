#include "settings_ui.h"

#include <chrono>
#include <cstring>

#include "imgui.h"

namespace {

bool isValidUsername(const std::string& name) {
    if (name.size() < 3 || name.size() > 32) {
        return false;
    }

    for (unsigned char c : name) {
        bool allowed =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

const char* errorText(const std::string& code) {
    std::string key = "err." + code;
    const char* text = tr(key.c_str());

    if (key == text) {
        return tr("err.unknown");
    }

    return text;
}

void centerNextWindow(float width, float height) {
    ImVec2 center(
        ImGui::GetIO().DisplaySize.x * 0.5f,
        ImGui::GetIO().DisplaySize.y * 0.5f
    );

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

}

void SettingsUI::init() {
    CloudClient::globalInit();

    settings_ = loadAppSettings();
    setLanguage(settings_.language);

    if (!settings_.token.empty() && CloudClient::isConfigured()) {
        username_ = settings_.username;
        token_ = settings_.token;

        std::string token = token_;

        startAuth(
            AuthAction::Restore,
            std::async(std::launch::async, [token]() {
                return CloudClient::fetchProfile(token);
            })
        );
    }
}

void SettingsUI::update() {
    if (!authBusy_ || !authFuture_.valid()) {
        return;
    }

    if (authFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        CloudResponse response = authFuture_.get();
        applyAuthResponse(response);
    }
}

void SettingsUI::startAuth(AuthAction action, std::future<CloudResponse>&& future) {
    pendingAction_ = action;
    authFuture_ = std::move(future);
    authBusy_ = true;
}

void SettingsUI::applyAuthResponse(const CloudResponse& response) {
    AuthAction action = pendingAction_;

    authBusy_ = false;
    pendingAction_ = AuthAction::None;

    if (response.ok) {
        loggedIn_ = true;
        owned_ = response.speedigon;

        if (!response.token.empty()) {
            token_ = response.token;
        }

        if (!response.username.empty()) {
            username_ = response.username;
        }

        if (!owned_) {
            active_ = false;
        } else {
            showSpeedigonInfo_ = false;
        }

        bool validLanguage = false;
        Language language = languageFromCode(response.language, &validLanguage);

        if (validLanguage && (action == AuthAction::Login || action == AuthAction::Restore)) {
            setLanguage(language);
        }

        persist();

        if (action == AuthAction::Purchase) {
            statusMessage_.clear();
        }

        if (action == AuthAction::Login || action == AuthAction::Register) {
            statusMessage_ = tr("account.signed_in_ok");
            statusIsError_ = false;
            std::memset(passwordBuffer_, 0, sizeof(passwordBuffer_));
            std::memset(confirmBuffer_, 0, sizeof(confirmBuffer_));
        }

        return;
    }

    if (response.code == "unauthorized") {
        logoutLocal();
    }

    if (action != AuthAction::Restore) {
        statusMessage_ = errorText(response.code);
        statusIsError_ = true;
    }
}

void SettingsUI::changeLanguage(Language language) {
    setLanguage(language);
    persist();

    if (loggedIn_ && !token_.empty()) {
        std::string token = token_;
        std::string code = languageCode(language);

        languageFuture_ = std::async(std::launch::async, [token, code]() {
            return CloudClient::updateLanguage(token, code);
        });
    }
}

void SettingsUI::logoutLocal() {
    loggedIn_ = false;
    owned_ = false;
    active_ = false;
    username_.clear();
    token_.clear();
    persist();
}

void SettingsUI::persist() {
    settings_.language = getLanguage();
    settings_.username = username_;
    settings_.token = token_;
    saveAppSettings(settings_);
}

bool SettingsUI::isLoggedIn() const {
    return loggedIn_;
}

bool SettingsUI::isSpeedigonActive() const {
    return loggedIn_ && owned_ && active_;
}

const std::string& SettingsUI::token() const {
    return token_;
}

void SettingsUI::onSpeedigonButton() {
    if (!loggedIn_) {
        showSpeedigonLogin_ = true;
    } else if (!owned_) {
        statusMessage_.clear();
        showSpeedigonInfo_ = true;
    } else {
        active_ = !active_;
    }
}

void SettingsUI::drawSidebarButton() {
    if (ImGui::Button(tr("btn.settings"))) {
        showSettings_ = true;
    }

    if (isSpeedigonActive()) {
        ImGui::SameLine();
        ImGui::Text("%s", tr("speedigon.active_label"));
    }

    ImGui::Spacing();
}

void SettingsUI::drawWindows() {
    drawSettingsWindow();
    drawAccountWindow();
    drawSpeedigonLoginWindow();
    drawSpeedigonInfoWindow();
    drawTimeoutWindow();
    drawLimitWindow();
}

void SettingsUI::showLimitNotice(unsigned int triangles, unsigned int limit) {
    limitTriangles_ = triangles;
    limitValue_ = limit;
    showLimit_ = true;
}

void SettingsUI::drawLimitWindow() {
    if (!showLimit_) {
        return;
    }

    centerNextWindow(640.0f, 340.0f);

    std::string title = std::string(tr("limit.title")) + "###LimitWindow";

    if (ImGui::Begin(title.c_str(), &showLimit_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped("%s", tr("limit.text"));
        ImGui::Spacing();
        ImGui::Text("%s: %u", tr("limit.model_triangles"), limitTriangles_);
        ImGui::Text("%s: %u", tr("limit.local_limit"), limitValue_);
        ImGui::Spacing();

        if (ImGui::Button(tr("timeout.open_settings"))) {
            showSettings_ = true;
            showLimit_ = false;
        }

        ImGui::SameLine();

        if (ImGui::Button(tr("btn.ok"))) {
            showLimit_ = false;
        }
    }

    ImGui::End();
}

void SettingsUI::showTimeoutNotice(bool speedigonWasActive) {
    timeoutSpeedigon_ = speedigonWasActive;
    showTimeout_ = true;
}

void SettingsUI::drawTimeoutWindow() {
    if (!showTimeout_) {
        return;
    }

    centerNextWindow(620.0f, 300.0f);

    std::string title = std::string(tr("timeout.title")) + "###TimeoutWindow";

    if (ImGui::Begin(title.c_str(), &showTimeout_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped(
            "%s",
            timeoutSpeedigon_ ? tr("timeout.text_speedigon") : tr("timeout.text")
        );

        ImGui::Spacing();

        if (!timeoutSpeedigon_ && ImGui::Button(tr("timeout.open_settings"))) {
            showSettings_ = true;
            showTimeout_ = false;
        }

        if (!timeoutSpeedigon_) {
            ImGui::SameLine();
        }

        if (ImGui::Button(tr("btn.ok"))) {
            showTimeout_ = false;
        }
    }

    ImGui::End();
}

void SettingsUI::drawSettingsWindow() {
    if (!showSettings_) {
        return;
    }

    centerNextWindow(560.0f, 330.0f);

    std::string title = std::string(tr("settings.title")) + "###SettingsWindow";

    if (ImGui::Begin(title.c_str(), &showSettings_, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button(tr("settings.account"))) {
            showAccount_ = true;
            ImGui::SetWindowFocus("###AccountWindow");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("%s", tr("settings.language"));

        if (ImGui::BeginCombo("##LanguageCombo", languageNativeName(getLanguage()))) {
            for (int i = 0; i < kLanguageCount; ++i) {
                Language language = static_cast<Language>(i);
                bool selected = language == getLanguage();

                if (ImGui::Selectable(languageNativeName(language), selected)) {
                    changeLanguage(language);
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const char* label = isSpeedigonActive()
            ? tr("settings.speedigon_deactivate")
            : tr("settings.speedigon_activate");

        if (ImGui::Button(label)) {
            onSpeedigonButton();
        }
    }

    ImGui::End();
}

void SettingsUI::submitAccountForm() {
    std::string username = userBuffer_;
    std::string password = passwordBuffer_;
    std::string confirm = confirmBuffer_;

    statusIsError_ = true;

    if (registerMode_) {
        if (!isValidUsername(username) || password.size() < 8) {
            statusMessage_ = errorText("invalid_input");
            return;
        }

        if (password != confirm) {
            statusMessage_ = errorText("password_mismatch");
            return;
        }
    } else if (username.empty() || password.empty()) {
        statusMessage_ = errorText("invalid_credentials");
        return;
    }

    statusMessage_.clear();

    if (registerMode_) {
        std::string code = languageCode(getLanguage());

        startAuth(
            AuthAction::Register,
            std::async(std::launch::async, [username, password, code]() {
                return CloudClient::registerAccount(username, password, code);
            })
        );
    } else {
        startAuth(
            AuthAction::Login,
            std::async(std::launch::async, [username, password]() {
                return CloudClient::login(username, password);
            })
        );
    }
}

void SettingsUI::drawAccountWindow() {
    if (!showAccount_) {
        return;
    }

    centerNextWindow(580.0f, 470.0f);

    std::string title = std::string(tr("account.title")) + "###AccountWindow";

    if (ImGui::Begin(title.c_str(), &showAccount_, ImGuiWindowFlags_NoCollapse)) {
        if (loggedIn_) {
            ImGui::TextWrapped("%s: %s", tr("account.signed_in_as"), username_.c_str());
            ImGui::Spacing();
            ImGui::Text("%s", owned_ ? tr("account.speedigon_owned") : tr("account.speedigon_not_owned"));
            ImGui::Spacing();

            if (authBusy_) {
                ImGui::Text("%s", tr("account.working"));
            } else {
                if (ImGui::Button(tr("account.refresh"))) {
                    std::string token = token_;

                    startAuth(
                        AuthAction::Refresh,
                        std::async(std::launch::async, [token]() {
                            return CloudClient::fetchProfile(token);
                        })
                    );
                }

                ImGui::SameLine();

                if (ImGui::Button(tr("account.logout"))) {
                    logoutLocal();
                    statusMessage_.clear();
                }
            }
        } else {
            if (ImGui::BeginTabBar("##AuthTabs")) {
                if (ImGui::BeginTabItem(tr("account.login"))) {
                    registerMode_ = false;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(tr("account.create"))) {
                    registerMode_ = true;
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Spacing();
            ImGui::Text("%s", tr("account.username"));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##AccountUser", userBuffer_, IM_ARRAYSIZE(userBuffer_));

            ImGui::Text("%s", tr("account.password"));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText(
                "##AccountPassword",
                passwordBuffer_,
                IM_ARRAYSIZE(passwordBuffer_),
                ImGuiInputTextFlags_Password
            );

            if (registerMode_) {
                ImGui::Text("%s", tr("account.confirm"));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText(
                    "##AccountConfirm",
                    confirmBuffer_,
                    IM_ARRAYSIZE(confirmBuffer_),
                    ImGuiInputTextFlags_Password
                );
            }

            ImGui::Spacing();

            if (authBusy_) {
                ImGui::Text("%s", tr("account.working"));
            } else if (ImGui::Button(registerMode_ ? tr("account.create") : tr("account.login"))) {
                submitAccountForm();
            }
        }

        if (!statusMessage_.empty()) {
            ImGui::Spacing();

            ImVec4 color = statusIsError_
                ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", statusMessage_.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}

void SettingsUI::drawSpeedigonLoginWindow() {
    if (!showSpeedigonLogin_) {
        return;
    }

    centerNextWindow(560.0f, 260.0f);

    std::string title = std::string("Speedigon") + "###SpeedigonLoginWindow";

    if (ImGui::Begin(title.c_str(), &showSpeedigonLogin_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped("%s", tr("speedigon.need_account"));
        ImGui::Spacing();

        if (ImGui::Button(tr("account.login"))) {
            registerMode_ = false;
            showAccount_ = true;
            showSpeedigonLogin_ = false;
        }

        ImGui::SameLine();

        if (ImGui::Button(tr("account.create"))) {
            registerMode_ = true;
            showAccount_ = true;
            showSpeedigonLogin_ = false;
        }
    }

    ImGui::End();
}

void SettingsUI::drawSpeedigonInfoWindow() {
    if (!showSpeedigonInfo_) {
        return;
    }

    centerNextWindow(640.0f, 420.0f);

    std::string title = std::string(tr("speedigon.about_title")) + "###SpeedigonInfoWindow";

    if (ImGui::Begin(title.c_str(), &showSpeedigonInfo_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextWrapped("%s", tr("speedigon.about_text"));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("%s", tr("speedigon.price"));
        ImGui::Spacing();

        if (authBusy_) {
            ImGui::Text("%s", tr("account.working"));
        } else {
            if (ImGui::Button(tr("speedigon.buy"))) {
                std::string token = token_;

                startAuth(
                    AuthAction::Purchase,
                    std::async(std::launch::async, [token]() {
                        return CloudClient::purchaseSpeedigon(token);
                    })
                );
            }

            ImGui::SameLine();

            if (ImGui::Button(tr("account.refresh"))) {
                std::string token = token_;

                startAuth(
                    AuthAction::Refresh,
                    std::async(std::launch::async, [token]() {
                        return CloudClient::fetchProfile(token);
                    })
                );
            }
        }

        if (!statusMessage_.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", statusMessage_.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}