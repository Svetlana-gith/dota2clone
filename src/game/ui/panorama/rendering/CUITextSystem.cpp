#include "CUITextSystem.h"
#include "../core/CUIEngine.h"
#include "../widgets/CLabel.h"
#include "../widgets/CButton.h"
#include "../widgets/CTextEntry.h"

namespace Panorama {

CUITextSystem& CUITextSystem::Instance() {
    static CUITextSystem instance;
    return instance;
}

std::string CUITextSystem::ResolveTokenIfNeeded(const std::string& value) const {
    // Treat values starting with '#' as localization tokens.
    if (!value.empty() && value[0] == '#') {
        return CUIEngine::Instance().Localize(value);
    }
    return value;
}

void CUITextSystem::ApplyText(CPanel2D* panel) {
    if (!panel) return;

    // Prefer explicit loc attribute if present; otherwise use text attribute (which
    // itself may contain a token starting with '#').
    std::string raw;
    if (panel->HasAttribute("loc")) {
        raw = panel->GetAttribute("loc");
    } else if (panel->HasAttribute("text")) {
        raw = panel->GetAttribute("text");
    }

    if (!raw.empty()) {
        const std::string resolved = ResolveTokenIfNeeded(raw);

        if (auto* label = dynamic_cast<CLabel*>(panel)) {
            label->SetText(resolved);
        } else if (auto* button = dynamic_cast<CButton*>(panel)) {
            button->SetText(resolved);
        } else if (auto* entry = dynamic_cast<CTextEntry*>(panel)) {
            // Initial content only (apply once after creation).
            entry->SetText(resolved);
        }
    }

    // TextEntry placeholder support (optional).
    if (auto* entry = dynamic_cast<CTextEntry*>(panel)) {
        if (panel->HasAttribute("placeholder")) {
            entry->SetPlaceholder(ResolveTokenIfNeeded(panel->GetAttribute("placeholder")));
        }
    }
}

void CUITextSystem::ApplyTextRecursive(CPanel2D* root) {
    if (!root) return;
    ApplyText(root);
    for (const auto& child : root->GetChildren()) {
        ApplyTextRecursive(child.get());
    }
}

bool CUITextSystem::SetTextByID(CPanel2D* root, const std::string& id, const std::string& text) {
    if (!root) return false;
    CPanel2D* p = root->FindChildTraverse(id);
    if (!p) return false;

    if (auto* label = dynamic_cast<CLabel*>(p)) {
        label->SetText(text);
        return true;
    }
    if (auto* button = dynamic_cast<CButton*>(p)) {
        button->SetText(text);
        return true;
    }
    if (auto* entry = dynamic_cast<CTextEntry*>(p)) {
        entry->SetText(text);
        return true;
    }
    return false;
}

} // namespace Panorama

