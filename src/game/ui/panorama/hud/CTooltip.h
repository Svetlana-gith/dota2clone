#pragma once

#include "../core/CPanel2D.h"

namespace Panorama {

class CTooltip : public CPanel2D {
public:
    CTooltip();
    virtual ~CTooltip() = default;

    void ShowTooltip(const std::string& title, const std::string& description, f32 x, f32 y);
    void HideTooltip();
    void UpdatePosition(f32 x, f32 y);

private:
    std::shared_ptr<CPanel2D> m_titleLabel;
    std::shared_ptr<CPanel2D> m_descriptionLabel;
    std::shared_ptr<CPanel2D> m_background;
};

} // namespace Panorama