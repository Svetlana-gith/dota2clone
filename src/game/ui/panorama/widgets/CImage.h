#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <string>

namespace Panorama {

class CUIRenderer;

/**
 * Image widget for displaying textures/images
 */
class CImage : public CPanel2D {
public:
    CImage();
    CImage(const std::string& src, const std::string& id = "");
    
    // Image management
    void SetImage(const std::string& path);
    const std::string& GetImagePath() const { return m_imagePath; }
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    std::string m_imagePath;
};

} // namespace Panorama