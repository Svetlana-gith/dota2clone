#include "CNotificationManager.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CLabel.h"

namespace Panorama {

CNotificationManager::CNotificationManager() {
    // Notification manager doesn't need child panels initially
}

void CNotificationManager::ShowNotification(const std::string& message, f32 duration) {
    Notification notification;
    notification.message = message;
    notification.duration = duration;
    notification.timeRemaining = duration;
    
    m_notifications.push(notification);
    
    // If no current notification is showing, show this one immediately
    if (!m_currentNotification || !m_currentNotification->IsVisible()) {
        ShowNextNotification();
    }
}

void CNotificationManager::ShowKillFeed(const std::string& killer, const std::string& victim, const std::string& ability) {
    std::string message = killer + " killed " + victim;
    if (!ability.empty()) {
        message += " with " + ability;
    }
    ShowNotification(message, 5.0f); // Kill feed notifications last longer
}

void CNotificationManager::Update(f32 deltaTime) {
    CPanel2D::Update(deltaTime);
    
    if (m_currentNotification && m_currentNotification->IsVisible()) {
        if (!m_notifications.empty()) {
            m_notifications.front().timeRemaining -= deltaTime;
            
            if (m_notifications.front().timeRemaining <= 0) {
                m_notifications.pop();
                m_currentNotification->SetVisible(false);
                
                // Show next notification if available
                if (!m_notifications.empty()) {
                    ShowNextNotification();
                }
            }
        }
    }
}

void CNotificationManager::ShowNextNotification() {
    if (!m_notifications.empty()) {
        if (!m_currentNotification) {
            m_currentNotification = CPanelWidgets::CreateLabel("", 10, 10);
            AddChild(m_currentNotification);
        }
        
        // TODO: Set notification text when CLabel text setting is implemented
        m_currentNotification->SetVisible(true);
    }
}

} // namespace Panorama