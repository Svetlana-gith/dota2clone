#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <functional>

namespace Panorama {

class CUIRenderer;

/**
 * Slider widget for numeric input
 */
class CSlider : public CPanel2D {
public:
    CSlider();
    CSlider(const std::string& id);
    
    // Value management
    void SetValue(f32 value);
    f32 GetValue() const { return m_value; }
    void SetRange(f32 min, f32 max);
    void SetStep(f32 step) { m_step = step; }
    void SetVertical(bool vertical) { m_vertical = vertical; }
    
    // Events
    void SetOnValueChanged(std::function<void(f32)> handler) { m_onValueChanged = handler; }
    
    // Input events
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    f32 m_value = 0.0f;
    f32 m_min = 0.0f;
    f32 m_max = 100.0f;
    f32 m_step = 0.0f;
    bool m_vertical = false;
    bool m_dragging = false;
    std::function<void(f32)> m_onValueChanged;
};

} // namespace Panorama