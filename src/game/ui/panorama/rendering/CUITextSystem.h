#pragma once
/**
 * CUITextSystem - Applies text-related attributes to panels
 *
 * Goal: separate "element creation" from "text creation".
 * - Layout/panel factories only build hierarchy + store attributes.
 * - This system reads attributes like `text`, `placeholder`, `loc` and applies them
 *   to the correct panel types (Label/Button/TextEntry), with optional localization.
 */

#include "../core/CPanel2D.h"
#include <string>

namespace Panorama {

class CUITextSystem {
public:
    static CUITextSystem& Instance();

    // Apply text attributes to a whole subtree once (e.g. after loading a layout).
    void ApplyTextRecursive(CPanel2D* root);

    // Apply text attributes to a single panel.
    void ApplyText(CPanel2D* panel);

    // Convenience: set text by ID (looks up inside the provided root).
    bool SetTextByID(CPanel2D* root, const std::string& id, const std::string& text);

private:
    CUITextSystem() = default;

    std::string ResolveTokenIfNeeded(const std::string& value) const;
};

} // namespace Panorama

