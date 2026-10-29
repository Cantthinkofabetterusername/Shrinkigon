#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <future>
#include <string>

#include "app_settings.h"
#include "cloud_client.h"

class SettingsUI {
public:
    void init();
    void update();
    void drawSidebarButton();
    void drawWindows();
    void showTimeoutNotice(bool speedigonWasActive);
    void showLimitNotice(unsigned int triangles, unsigned int limit);

    bool isLoggedIn() const;
    bool isSpeedigonActive() const;
    const std::string& token() const;

private:
    enum class AuthAction {
        None,
        Login,
        Register,
        Refresh,
        Restore,
        Purchase
    };

    void startAuth(AuthAction action, std::future<CloudResponse>&& future);
    void applyAuthResponse(const CloudResponse& response);
    void changeLanguage(Language language);
    void logoutLocal();
    void persist();
    void onSpeedigonButton();
    void submitAccountForm();

    void drawSettingsWindow();
    void drawAccountWindow();
    void drawSpeedigonLoginWindow();
    void drawSpeedigonInfoWindow();
    void drawTimeoutWindow();
    void drawLimitWindow();

    AppSettings settings_;

    bool loggedIn_ = false;
    bool owned_ = false;
    bool active_ = false;
    std::string username_;
    std::string token_;

    bool showSettings_ = false;
    bool showAccount_ = false;
    bool showSpeedigonLogin_ = false;
    bool showSpeedigonInfo_ = false;
    bool registerMode_ = false;
    bool showTimeout_ = false;
    bool showLimit_ = false;
    unsigned int limitTriangles_ = 0;
    unsigned int limitValue_ = 0;
    bool timeoutSpeedigon_ = false;

    char userBuffer_[64] = "";
    char passwordBuffer_[128] = "";
    char confirmBuffer_[128] = "";

    std::string statusMessage_;
    bool statusIsError_ = false;

    bool authBusy_ = false;
    AuthAction pendingAction_ = AuthAction::None;
    std::future<CloudResponse> authFuture_;
    std::future<CloudResponse> languageFuture_;
};

#endif