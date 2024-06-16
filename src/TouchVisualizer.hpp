#include "src/devices/ITouch.hpp"
#include "src/render/Texture.hpp"
#include <cairo/cairo.h>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Shaders.hpp>
#include <unordered_map>

class Visualizer {
  public:
    Visualizer();
    ~Visualizer();
    void onRender();
    void damageAll();
    void damageFinger(int32_t id);

    void onTouchDown(ITouch::SDownEvent);
    void onTouchUp(ITouch::SUpEvent);
    void onTouchMotion(ITouch::SMotionEvent);

  private:
    CTexture texture;
    cairo_surface_t* cairoSurface;
    bool tempDamaged             = false;
    const int TOUCH_POINT_RADIUS = 15;
    std::unordered_map<int32_t, Vector2D> finger_positions;
    std::unordered_map<int32_t, Vector2D> prev_finger_positions;
};
