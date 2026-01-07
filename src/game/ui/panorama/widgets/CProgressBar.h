#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"

namespace Panorama {

class CUIRenderer;

/**
 * Progress bar widget for showing completion percentage
 */
class CProgressBar : public CPanel2D {
public:
    CProgressBar();
    CProgressBar(const std::string& id);
    
    // Value management
    void SetValue(f32 value);
    f32 GetValue() const { return m_value; }
    void SetRange(f32 min, f32 max) { m_min = min; m_max = max; }
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    f32 m_value = 0.0f;
    f32 m_min = 0.0f;
    f32 m_max = 100.0f;
};

} // namespace Panorama