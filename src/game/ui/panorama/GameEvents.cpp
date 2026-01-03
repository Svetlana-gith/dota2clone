#include "GameEvents.h"
#include "CPanel2D.h"
#include <algorithm>

namespace Panorama {

// ============ CGameEventData Implementation ============

i32 CGameEventData::GetInt(const std::string& key, i32 defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        try {
            return std::any_cast<i32>(it->second);
        } catch (...) {}
    }
    return defaultVal;
}

f32 CGameEventData::GetFloat(const std::string& key, f32 defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        try {
            return std::any_cast<f32>(it->second);
        } catch (...) {
            // Try int conversion
            try {
                return static_cast<f32>(std::any_cast<i32>(it->second));
            } catch (...) {}
        }
    }
    return defaultVal;
}

std::string CGameEventData::GetString(const std::string& key, const std::string& defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        try {
            return std::any_cast<std::string>(it->second);
        } catch (...) {}
    }
    return defaultVal;
}

bool CGameEventData::GetBool(const std::string& key, bool defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        try {
            return std::any_cast<bool>(it->second);
        } catch (...) {}
    }
    return defaultVal;
}

// ============ CGameEvents Implementation ============

CGameEvents& CGameEvents::Instance() {
    static CGameEvents instance;
    return instance;
}

i32 CGameEvents::Subscribe(const std::string& eventName, GameEventHandler handler) {
    i32 id = m_nextSubscriptionId++;
    m_subscriptions.push_back({id, eventName, handler});
    return id;
}

void CGameEvents::Unsubscribe(i32 subscriptionId) {
    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
            [subscriptionId](const Subscription& s) { return s.id == subscriptionId; }),
        m_subscriptions.end());
}

void CGameEvents::UnsubscribeAll(const std::string& eventName) {
    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
            [&eventName](const Subscription& s) { return s.eventName == eventName; }),
        m_subscriptions.end());
}

void CGameEvents::DispatchEvent(const std::string& eventName, const CGameEventData& data) {
    // Make a copy of subscriptions in case handlers modify the list
    auto subs = m_subscriptions;
    
    for (const auto& sub : subs) {
        if (sub.eventName == eventName) {
            sub.handler(data);
        }
    }
}

void CGameEvents::SendCustomGameEventToServer(const std::string& eventName, const CGameEventData& data) {
    // In a real implementation, this would serialize the event and send it over the network
    // For now, just dispatch locally for testing
    DispatchEvent("Server_" + eventName, data);
}

void CGameEvents::SendCustomGameEventToAllClients(const std::string& eventName, const CGameEventData& data) {
    // Server-side: would broadcast to all connected clients
    DispatchEvent("Client_" + eventName, data);
}

void CGameEvents::SendCustomGameEventToClient(const std::string& eventName, i32 playerId, const CGameEventData& data) {
    // Server-side: would send to specific client
    // For now, just dispatch locally
    DispatchEvent("Client_" + eventName, data);
}

// ============ CUIEvents Implementation ============

CUIEvents& CUIEvents::Instance() {
    static CUIEvents instance;
    return instance;
}

void CUIEvents::RegisterEventHandler(const std::string& eventName, CPanel2D* panel, EventHandler handler) {
    m_handlers.push_back({eventName, panel, handler});
}

void CUIEvents::UnregisterEventHandler(const std::string& eventName, CPanel2D* panel) {
    m_handlers.erase(
        std::remove_if(m_handlers.begin(), m_handlers.end(),
            [&](const UIEventHandler& h) { 
                return h.eventName == eventName && h.panel == panel; 
            }),
        m_handlers.end());
}

void CUIEvents::DispatchEvent(const std::string& eventName, CPanel2D* panel, const PanelEvent& event) {
    // Dispatch to panel and bubble up to ancestors
    CPanel2D* current = panel;
    
    while (current) {
        for (const auto& handler : m_handlers) {
            if (handler.eventName == eventName && handler.panel == current) {
                PanelEvent e = event;
                e.currentTarget = current;
                handler.handler(e);
                
                if (!e.bubbles) return;
            }
        }
        current = current->GetParent();
    }
}

} // namespace Panorama
