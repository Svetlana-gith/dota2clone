#pragma once

#include "../core/CPanel2D.h"
#include <queue>
#include <memory>
#include <string>

namespace Panorama {

struct Notification {
    std::string message;
    f32 duration = 3.0f;
    f32 timeRemaining = 3.0f;
};

class CNotificationManager : public CPanel2D {
public:
    CNotificationManager();
    virtual ~CNotificationManager() = default;

    void ShowNotification(const std::string& message, f32 duration = 3.0f);
    void ShowKillFeed(const std::string& killer, const std::string& victim, const std::string& ability = "");
    virtual void Update(f32 deltaTime) override;

private:
    std::queue<Notification> m_notifications;
    std::shared_ptr<CPanel2D> m_currentNotification;
    
    void ShowNextNotification();
};

} // namespace Panorama