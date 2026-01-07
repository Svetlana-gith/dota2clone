#pragma once

#include "../../../core/Types.h"
#include <memory>
#include <string>

// Forward declarations
namespace Panorama {
    class CLabel;
    class CImage;
    class CButton;
    class CProgressBar;
    class CTextEntry;
    class CSlider;
    class CDropDown;
}

namespace Panorama {

/**
 * Factory class for creating common UI widgets
 */
class CPanelWidgets {
public:
    // Factory methods for creating widgets
    static std::shared_ptr<CLabel> CreateLabel(const std::string& text, f32 x, f32 y);
    static std::shared_ptr<CImage> CreateImage(const std::string& src, f32 x, f32 y, f32 w, f32 h);
    static std::shared_ptr<CButton> CreateButton(const std::string& text, f32 x, f32 y, f32 w, f32 h);
    static std::shared_ptr<CProgressBar> CreateProgressBar(f32 x, f32 y, f32 w, f32 h);
    static std::shared_ptr<CTextEntry> CreateTextEntry(f32 x, f32 y, f32 w, f32 h);
    static std::shared_ptr<CSlider> CreateSlider(f32 x, f32 y, f32 w, f32 h);
    static std::shared_ptr<CDropDown> CreateDropDown(f32 x, f32 y, f32 w, f32 h);
};

} // namespace Panorama