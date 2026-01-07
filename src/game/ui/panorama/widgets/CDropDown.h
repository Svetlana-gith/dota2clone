#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Panorama {

class CUIRenderer;

/**
 * Dropdown widget for selecting from a list of options
 */
class CDropDown : public CPanel2D {
public:
    struct Option {
        std::string id;
        std::string text;
    };

    CDropDown();
    CDropDown(const std::string& id);
    
    // Option management
    void AddOption(const std::string& id, const std::string& text);
    void RemoveOption(const std::string& id);
    void ClearOptions();
    void SetSelected(const std::string& id);
    const std::string& GetSelected() const { return m_selectedId; }
    
    // Events
    void SetOnSelectionChanged(std::function<void(const std::string&)> handler) { m_onSelectionChanged = handler; }
    
    // Input events
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    std::vector<Option> m_options;
    std::string m_selectedId;
    bool m_isOpen = false;
    std::function<void(const std::string&)> m_onSelectionChanged;
};

} // namespace Panorama