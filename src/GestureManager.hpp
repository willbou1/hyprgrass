#pragma once
#include "./gestures/Gestures.hpp"
#include "gestures/Shared.hpp"
#include "globals.hpp"
#include <functional>

#define private public
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#undef private

#include <list>
#include <vector>
#include <wayfire/touch/touch.hpp>
#include <wayland-server-core.h>

using DispatcherFn = std::function<void(std::string)>;

class GestureManager : public IGestureManager {
  public:
    uint32_t long_press_next_trigger_time;
    std::list<SKeybind> internalBinds;

    GestureManager();
    ~GestureManager();
    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchDown(ITouch::SDownEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchUp(ITouch::SUpEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchMove(ITouch::SMotionEvent e);

    void onLongPressTimeout(uint32_t time_msec);

    // workaround
    void touchBindDispatcher(std::string args);

  protected:
    SMonitorArea getMonitorArea() const override;
    bool handleCompletedGesture(const CompletedGestureEvent& gev) override;
    void handleCancelledGesture() override;

  private:
    std::vector<wlr_surface*> touchedSurfaces;
    CMonitor* m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;
    wl_event_source* long_press_timer;
    std::optional<DispatcherFn> active_custom_bindm;

    bool handleGestureBind(std::string bind, bool pressed);

    // converts wlr touch event positions (number between 0.0 to 1.0) to pixel position,
    // takes into consideration monitor size and offset
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    // reverse of wlrTouchEventPositionAsPixels
    Vector2D pixelPositionToPercentagePosition(wf::touch::point_t) const;
    bool handleWorkspaceSwipe(const GestureDirection direction);

    bool handleDragGesture(const DragGestureEvent& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGestureEvent& gev) override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override;
    void stopLongPressTimer() override;

    void sendCancelEventsToWindows() override;
};

inline std::unique_ptr<GestureManager> g_pGestureManager;
