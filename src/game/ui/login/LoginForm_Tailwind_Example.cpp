/**
 * LoginForm - Tailwind-Inspired Example
 * 
 * Демонстрация использования utility-first CSS подхода
 * для создания компонентов с переиспользуемыми классами
 */

#include "LoginForm.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"
#include "../panorama/widgets/CTextEntry.h"
#include "../panorama/widgets/CButton.h"

namespace Game {

// ============================================
// ПРИМЕР: Error Label с Tailwind-style классами
// ============================================

void LoginForm::CreateErrorLabel_TailwindStyle() {
    // Создаем label с ID для позиционирования
    m_errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");
    
    // Добавляем utility классы для стиля
    m_errorLabel->AddClass("bg-error");        // Полупрозрачный красный фон
    m_errorLabel->AddClass("border-l-3");      // Левая граница 3px
    m_errorLabel->AddClass("border-error");    // Красный цвет границы
    m_errorLabel->AddClass("rounded");         // Скругление 6px
    m_errorLabel->AddClass("text-error");      // Красный текст
    m_errorLabel->AddClass("text-base");       // Размер 14px
    m_errorLabel->AddClass("px-3");            // Padding horizontal 12px
    
    m_errorLabel->SetVisible(false);
    m_container->AddChild(m_errorLabel);
}

// ============================================
// ПРИМЕР: Input Field с Tailwind-style классами
// ============================================

void LoginForm::CreateUsernameInput_TailwindStyle() {
    // Label
    m_usernameLabel = std::make_shared<Panorama::CLabel>("USERNAME", "UsernameLabel");
    m_usernameLabel->AddClass("text-sm");      // 12px
    m_usernameLabel->AddClass("text-gray");    // Серый цвет
    m_usernameLabel->AddClass("font-medium");  // Font weight 500
    m_container->AddChild(m_usernameLabel);
    
    // Input
    m_usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
    m_usernameInput->AddClass("bg-input");     // Темный фон
    m_usernameInput->AddClass("border-2");     // Граница 2px
    m_usernameInput->AddClass("border-input"); // Цвет границы
    m_usernameInput->AddClass("rounded-md");   // Скругление 8px
    m_usernameInput->AddClass("px-4");         // Padding 16px
    m_usernameInput->AddClass("text-lg");      // Размер 18px
    m_usernameInput->AddClass("text-white");   // Белый текст
    m_usernameInput->SetPlaceholder("Enter your username");
    m_container->AddChild(m_usernameInput);
}

// ============================================
// ПРИМЕР: Primary Button с Tailwind-style классами
// ============================================

void LoginForm::CreatePrimaryButton_TailwindStyle() {
    m_primaryButton = std::make_shared<Panorama::CButton>("ENTER THE GAME", "PrimaryButton");
    m_primaryButton->AddClass("bg-gold");         // Золотой фон
    m_primaryButton->AddClass("rounded-md");      // Скругление 8px
    m_primaryButton->AddClass("text-lg");         // Размер 18px
    m_primaryButton->AddClass("font-semibold");   // Font weight 600
    m_primaryButton->AddClass("shadow");          // Тень
    
    m_primaryButton->SetOnActivate([this]() {
        if (m_onSubmit) m_onSubmit();
    });
    m_container->AddChild(m_primaryButton);
}

// ============================================
// ПРИМЕР: Secondary Button с Tailwind-style классами
// ============================================

void LoginForm::CreateSecondaryButton_TailwindStyle() {
    m_secondaryButton = std::make_shared<Panorama::CButton>("CREATE ACCOUNT", "SecondaryButton");
    m_secondaryButton->AddClass("bg-card");       // Темный фон
    m_secondaryButton->AddClass("border-2");      // Граница 2px
    m_secondaryButton->AddClass("border-cyan");   // Cyan граница
    m_secondaryButton->AddClass("rounded-md");    // Скругление 8px
    m_secondaryButton->AddClass("text-cyan");     // Cyan текст
    m_secondaryButton->AddClass("font-medium");   // Font weight 500
    
    m_secondaryButton->SetOnActivate([this]() {
        if (m_onCreateAccount) m_onCreateAccount();
    });
    m_container->AddChild(m_secondaryButton);
}

// ============================================
// ПРИМЕР: Form Title с Tailwind-style классами
// ============================================

void LoginForm::CreateFormTitle_TailwindStyle() {
    m_titleLabel = std::make_shared<Panorama::CLabel>("WELCOME BACK", "FormTitle");
    m_titleLabel->AddClass("text-2xl");        // 28px
    m_titleLabel->AddClass("text-white");      // Белый
    m_titleLabel->AddClass("font-semibold");   // Font weight 600
    m_titleLabel->AddClass("mb-4");            // Margin bottom 16px
    m_container->AddChild(m_titleLabel);
}

// ============================================
// ПРИМЕР: Success Message (новый компонент)
// ============================================

void LoginForm::ShowSuccessMessage_TailwindStyle(const std::string& message) {
    auto successLabel = std::make_shared<Panorama::CLabel>(message, "SuccessMessage");
    
    // Tailwind-style классы для success alert
    successLabel->AddClass("bg-success");      // Зеленый фон
    successLabel->AddClass("border-l-3");      // Левая граница
    successLabel->AddClass("border-success");  // Зеленая граница
    successLabel->AddClass("rounded");         // Скругление
    successLabel->AddClass("text-success");    // Зеленый текст
    successLabel->AddClass("text-base");       // 14px
    successLabel->AddClass("px-3");            // Padding
    successLabel->AddClass("py-2");            // Padding vertical
    successLabel->AddClass("shadow-sm");       // Легкая тень
    
    m_container->AddChild(successLabel);
}

// ============================================
// ПРИМЕР: Warning Banner (новый компонент)
// ============================================

void LoginForm::ShowWarningBanner_TailwindStyle(const std::string& message) {
    auto warningBanner = std::make_shared<Panorama::CLabel>(message, "WarningBanner");
    
    // Tailwind-style классы для warning
    warningBanner->AddClass("bg-warning");     // Желтый фон
    warningBanner->AddClass("border-2");       // Граница 2px
    warningBanner->AddClass("border-warning"); // Желтая граница
    warningBanner->AddClass("rounded-lg");     // Большое скругление
    warningBanner->AddClass("text-warning");   // Желтый текст
    warningBanner->AddClass("text-lg");        // 18px
    warningBanner->AddClass("font-medium");    // Font weight 500
    warningBanner->AddClass("p-4");            // Padding 16px
    warningBanner->AddClass("shadow-lg");      // Большая тень
    
    m_container->AddChild(warningBanner);
}

// ============================================
// ПРИМЕР: Composable Card Component
// ============================================

std::shared_ptr<Panorama::CPanel2D> CreateCard_TailwindStyle(const std::string& id) {
    auto card = std::make_shared<Panorama::CPanel2D>(id);
    
    // Tailwind-style классы для card
    card->AddClass("bg-card");           // Темный фон
    card->AddClass("rounded-lg");        // Скругление 12px
    card->AddClass("border");            // Граница 1px
    card->AddClass("border-gold-dim");   // Золотая граница (dim)
    card->AddClass("p-6");               // Padding 24px
    card->AddClass("shadow-lg");         // Большая тень
    card->AddClass("opacity-95");        // 95% прозрачность
    
    return card;
}

// ============================================
// ПРИМЕР: Responsive Spacing Helper
// ============================================

void ApplyResponsiveSpacing(std::shared_ptr<Panorama::CPanel2D> panel, bool isSmallScreen) {
    if (isSmallScreen) {
        panel->AddClass("p-2");   // Меньше padding на маленьких экранах
        panel->AddClass("text-sm"); // Меньше текст
    } else {
        panel->AddClass("p-4");   // Больше padding
        panel->AddClass("text-base"); // Нормальный текст
    }
}

// ============================================
// ПРИМЕР: Theme Variants
// ============================================

void ApplyThemeVariant(std::shared_ptr<Panorama::CButton> button, const std::string& variant) {
    if (variant == "primary") {
        button->AddClass("bg-gold");
        button->AddClass("text-dark");
        button->AddClass("font-semibold");
    } else if (variant == "secondary") {
        button->AddClass("bg-card");
        button->AddClass("border-2");
        button->AddClass("border-cyan");
        button->AddClass("text-cyan");
    } else if (variant == "danger") {
        button->AddClass("bg-error");
        button->AddClass("text-white");
        button->AddClass("font-semibold");
    }
    
    // Общие классы для всех вариантов
    button->AddClass("rounded-md");
    button->AddClass("px-4");
    button->AddClass("py-2");
    button->AddClass("shadow");
}

} // namespace Game
