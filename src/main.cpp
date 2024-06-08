#include "GestureManager.hpp"
#include "bindm/MouseDispatcherArg.hpp"
#include "globals.hpp"

#include <chrono>
#include <cstdlib>
#include <format>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <string>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

void hkOnTouchDown(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SDownEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchDown(ev);
}

void hkOnTouchUp(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SUpEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchUp(ev);
}

void hkOnTouchMove(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SMotionEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchMove(ev);
}

static void onPreConfigReload() {
    g_pGestureManager->internalBinds.clear();
}

void listInternalBinds(std::string) {
    Debug::log(LogLevel::LOG, "[hyprgrass] Listing internal binds:");
    for (const auto& bind : g_pGestureManager->internalBinds) {
        Debug::log(LogLevel::LOG, "[hyprgrass] | gesture: {}", bind.key);
        Debug::log(LogLevel::LOG, "[hyprgrass] |     dispatcher: {}", bind.handler);
        Debug::log(LogLevel::LOG, "[hyprgrass] |     arg: {}", bind.arg);
        Debug::log(LogLevel::LOG, "[hyprgrass] |     mouse: {}", bind.mouse);
        Debug::log(LogLevel::LOG, "[hyprgrass] |");
    }
}

Hyprlang::CParseResult onNewBind(const char* K, const char* V) {
    std::string v = V;
    auto vars     = CVarList(v, 4);
    Hyprlang::CParseResult result;

    if (vars.size() < 3) {
        result.setError("must have at least 3 fields: <empty>, <gesture_event>, <dispatcher>, [args]");
        return result;
    }

    if (!vars[0].empty()) {
        result.setError("MODIFIER keys not currently supported");
        return result;
    }

    const auto mouse          = std::string("hyprgrass-bindm") == K;
    const auto key            = vars[1];
    const auto dispatcher     = mouse ? "mouse" : vars[2];
    const auto dispatcherArgs = mouse ? vars[2] : vars[3];

    g_pGestureManager->internalBinds.emplace_back(SKeybind{
        .key     = key,
        .handler = dispatcher,
        .arg     = dispatcherArgs,
        .mouse   = mouse,
    });

    return result;
}

void brightnessctlBindm(std::string args) {
    static auto lastTriggered    = std::chrono::steady_clock::now();
    static Vector2D lastPosition = {0, 0};

    const auto arg = Hyprgrass::MouseDispatcherArgEncoding::decode(args);
    if (!arg.has_value()) {
        Debug::log(LogLevel::ERR, "could not decode args: {}, ErrInfo: {}", (int)arg.error(),
                   Hyprgrass::gArgDecodeErrInfo);
        return;
    }

    if (arg->event == Hyprgrass::EventType::BEGIN) {
        lastPosition = {0, 0};
    }

    if (arg->event != Hyprgrass::EventType::UPDATE) {
        return;
    }

    const auto now  = std::chrono::steady_clock::now();
    const auto diff = now - lastTriggered;
    if (diff < std::chrono::milliseconds(100)) {
        return;
    }

    system(std::format("brightnessctl set {}%", lastPosition.y > arg->y ? 5 : -5).c_str());
    lastTriggered = now;
    lastPosition  = {arg->x, arg->y};
}

std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchDownHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchUpHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchMoveHook;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                                Hyprlang::CConfigValue((Hyprlang::INT)3));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_edge",
                                Hyprlang::CConfigValue((Hyprlang::STRING) "d"));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity",
                                Hyprlang::CConfigValue((Hyprlang::FLOAT)1.0));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:long_press_delay",
                                Hyprlang::CConfigValue((Hyprlang::INT)400));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel",
                                Hyprlang::CConfigValue((Hyprlang::INT)0));
#pragma GCC diagnostic pop

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprgrass-bind", onNewBind, Hyprlang::SHandlerOptions{});
    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprgrass-bindm", onNewBind, Hyprlang::SHandlerOptions{});
    static auto P0 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    HyprlandAPI::addDispatcher(PHANDLE, "touchBind", [&](std::string args) {
        HyprlandAPI::addNotification(
            PHANDLE, "[hyprgrass] touchBind dispatcher deprecated, use the hyprgrass-bind keyword instead",
            CColor(0.8, 0.2, 0.2, 1.0), 5000);
        g_pGestureManager->touchBindDispatcher(args);
    });
    HyprlandAPI::addDispatcher(PHANDLE, "__hyprgrass_bindm:brightnessctl", brightnessctlBindm);

    HyprlandAPI::addDispatcher(PHANDLE, "hyprgrass:debug:binds", listInternalBinds);

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CColor(0.8, 0.7, 0.26, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchDown", hkOnTouchDown);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchUp", hkOnTouchUp);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchMove", hkOnTouchMove);

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<GestureManager>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", "0.6"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
