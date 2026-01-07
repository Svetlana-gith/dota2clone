#pragma once
/**
 * GameEvents - Event System for Panorama UI
 * Similar to Valve's GameEvents.Subscribe/SendCustomGameEventToServer
 */

#include "PanoramaTypes.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <any>

namespace Panorama {

// ============ Game Event Data ============

class CGameEventData {
public:
    CGameEventData() = default;
    
    void SetInt(const std::string& key, i32 value) { m_data[key] = value; }
    void SetFloat(const std::string& key, f32 value) { m_data[key] = value; }
    void SetString(const std::string& key, const std::string& value) { m_data[key] = value; }
    void SetBool(const std::string& key, bool value) { m_data[key] = value; }
    
    i32 GetInt(const std::string& key, i32 defaultVal = 0) const;
    f32 GetFloat(const std::string& key, f32 defaultVal = 0) const;
    std::string GetString(const std::string& key, const std::string& defaultVal = "") const;
    bool GetBool(const std::string& key, bool defaultVal = false) const;
    
    bool HasKey(const std::string& key) const { return m_data.find(key) != m_data.end(); }
    
private:
    std::unordered_map<std::string, std::any> m_data;
};

// ============ Game Events Manager ============

using GameEventHandler = std::function<void(const CGameEventData&)>;

class CGameEvents {
public:
    static CGameEvents& Instance();
    
    // Subscribe to game event (like Valve's GameEvents.Subscribe)
    i32 Subscribe(const std::string& eventName, GameEventHandler handler);
    
    // Unsubscribe from event
    void Unsubscribe(i32 subscriptionId);
    void UnsubscribeAll(const std::string& eventName);
    
    // Fire event locally (like Valve's $.DispatchEvent)
    void DispatchEvent(const std::string& eventName, const CGameEventData& data);
    
    // Send to server (like Valve's GameEvents.SendCustomGameEventToServer)
    void SendCustomGameEventToServer(const std::string& eventName, const CGameEventData& data);
    
    // Send to all clients (server-side, like Valve's CustomGameEventManager)
    void SendCustomGameEventToAllClients(const std::string& eventName, const CGameEventData& data);
    
    // Send to specific player
    void SendCustomGameEventToClient(const std::string& eventName, i32 playerId, const CGameEventData& data);
    
    // ============ Common Dota 2-style Events ============
    // These are examples of events you might fire:
    // - "dota_player_gained_level"
    // - "dota_player_learned_ability"
    // - "dota_player_take_tower_damage"
    // - "dota_player_kill"
    // - "dota_item_purchased"
    // - "dota_ability_used"
    // - "dota_hero_inventory_changed"
    // - "dota_player_update_hero_selection"
    
private:
    CGameEvents() = default;
    
    struct Subscription {
        i32 id;
        std::string eventName;
        GameEventHandler handler;
    };
    
    std::vector<Subscription> m_subscriptions;
    i32 m_nextSubscriptionId = 1;
};

// ============ UI Events (Panel-specific) ============

class CUIEvents {
public:
    static CUIEvents& Instance();
    
    // Register UI event handler (like Valve's $.RegisterEventHandler)
    void RegisterEventHandler(const std::string& eventName, CPanel2D* panel, EventHandler handler);
    
    // Unregister
    void UnregisterEventHandler(const std::string& eventName, CPanel2D* panel);
    
    // Dispatch UI event to panel and ancestors
    void DispatchEvent(const std::string& eventName, CPanel2D* panel, const PanelEvent& event);
    
    // ============ Common UI Events ============
    // - "Activated" - button clicked
    // - "Cancelled" - escape pressed
    // - "ContextMenu" - right click
    // - "DoubleClicked"
    // - "DragStart", "DragEnd", "DragDrop"
    // - "FocusChanged"
    // - "InputSubmit" - enter pressed in text field
    // - "ScrolledDown", "ScrolledUp"
    // - "SelectionChanged"
    // - "StyleClassesChanged"
    // - "TextChanged"
    
private:
    CUIEvents() = default;
    
    struct UIEventHandler {
        std::string eventName;
        CPanel2D* panel;
        EventHandler handler;
    };
    
    std::vector<UIEventHandler> m_handlers;
};

// ============ Convenience Macros/Functions ============

// Subscribe to game event (Valve-style API)
inline i32 GameEvents_Subscribe(const std::string& eventName, GameEventHandler handler) {
    return CGameEvents::Instance().Subscribe(eventName, handler);
}

// Fire custom event
inline void GameEvents_Fire(const std::string& eventName, const CGameEventData& data = {}) {
    CGameEvents::Instance().DispatchEvent(eventName, data);
}

// Send to server
inline void GameEvents_SendToServer(const std::string& eventName, const CGameEventData& data = {}) {
    CGameEvents::Instance().SendCustomGameEventToServer(eventName, data);
}

} // namespace Panorama
