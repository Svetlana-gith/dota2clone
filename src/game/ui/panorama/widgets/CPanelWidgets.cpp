#include "CPanelWidgets.h"
#include "CLabel.h"
#include "CImage.h"
#include "CButton.h"
#include "CProgressBar.h"
#include "CTextEntry.h"
#include "CSlider.h"
#include "CDropDown.h"
#include "../core/CPanel2D.h"
#include "../rendering/CUIRenderer.h"
#include <algorithm>
#include <cmath>

// Prevent Windows min/max macros
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panorama {

// ============ CPanelWidgets Factory Methods ============

std::shared_ptr<CLabel> CPanelWidgets::CreateLabel(const std::string& text, f32 x, f32 y) {
    auto label = std::make_shared<CLabel>(text);
    label->GetStyle().x = Length::Px(x);
    label->GetStyle().y = Length::Px(y);
    return label;
}

std::shared_ptr<CImage> CPanelWidgets::CreateImage(const std::string& src, f32 x, f32 y, f32 w, f32 h) {
    auto image = std::make_shared<CImage>(src);
    image->GetStyle().x = Length::Px(x);
    image->GetStyle().y = Length::Px(y);
    image->GetStyle().width = Length::Px(w);
    image->GetStyle().height = Length::Px(h);
    return image;
}

std::shared_ptr<CButton> CPanelWidgets::CreateButton(const std::string& text, f32 x, f32 y, f32 w, f32 h) {
    auto button = std::make_shared<CButton>(text);
    button->GetStyle().x = Length::Px(x);
    button->GetStyle().y = Length::Px(y);
    button->GetStyle().width = Length::Px(w);
    button->GetStyle().height = Length::Px(h);
    return button;
}

std::shared_ptr<CProgressBar> CPanelWidgets::CreateProgressBar(f32 x, f32 y, f32 w, f32 h) {
    auto progressBar = std::make_shared<CProgressBar>();
    progressBar->GetStyle().x = Length::Px(x);
    progressBar->GetStyle().y = Length::Px(y);
    progressBar->GetStyle().width = Length::Px(w);
    progressBar->GetStyle().height = Length::Px(h);
    return progressBar;
}

std::shared_ptr<CTextEntry> CPanelWidgets::CreateTextEntry(f32 x, f32 y, f32 w, f32 h) {
    auto textEntry = std::make_shared<CTextEntry>();
    textEntry->GetStyle().x = Length::Px(x);
    textEntry->GetStyle().y = Length::Px(y);
    textEntry->GetStyle().width = Length::Px(w);
    textEntry->GetStyle().height = Length::Px(h);
    return textEntry;
}

std::shared_ptr<CSlider> CPanelWidgets::CreateSlider(f32 x, f32 y, f32 w, f32 h) {
    auto slider = std::make_shared<CSlider>();
    slider->GetStyle().x = Length::Px(x);
    slider->GetStyle().y = Length::Px(y);
    slider->GetStyle().width = Length::Px(w);
    slider->GetStyle().height = Length::Px(h);
    return slider;
}

std::shared_ptr<CDropDown> CPanelWidgets::CreateDropDown(f32 x, f32 y, f32 w, f32 h) {
    auto dropDown = std::make_shared<CDropDown>();
    dropDown->GetStyle().x = Length::Px(x);
    dropDown->GetStyle().y = Length::Px(y);
    dropDown->GetStyle().width = Length::Px(w);
    dropDown->GetStyle().height = Length::Px(h);
    return dropDown;
}

} // namespace Panorama
