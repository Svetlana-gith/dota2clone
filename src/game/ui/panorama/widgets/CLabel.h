#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <string>

namespace Panorama {

class CUIRenderer;

/**
 * Label widget for displaying text
 */
class CLabel : public CPanel2D {
public:
    CLabel();
    CLabel(const std::string& text, const std::string& id = "");
    
    // Text management
    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }
    void SetLocString(const std::string& token);
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    std::string m_text;
    std::string m_locToken;
};

} // namespace Panorama