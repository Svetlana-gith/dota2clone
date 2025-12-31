#include "GameMode.h"
#include "world/World.h"
#include "world/EntityManager.h"
#include "world/CreepSystem.h"
#include "ui/EditorCamera.h"
#include "core/MathUtils.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace WorldEditor {

void GameMode::draw(World& world) {
    if (!gameModeActive_) {
        return;
    }
    
    // Main game mode window
    ImGui::Begin("Game Mode", &gameModeActive_, ImGuiWindowFlags_MenuBar);
    
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Exit Game Mode")) {
            gameModeActive_ = false;
        }
        ImGui::EndMenuBar();
    }
    
    // Time controls
    drawTimeControls();

    // Stop & reset (clears runtime units/projectiles and resets stats/time)
    if (ImGui::Button("Stop & Reset")) {
        stopAndReset(world);
        ImGui::End();
        return;
    }
    
    ImGui::Separator();
    
    // Statistics
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawStatsPanel(world);
    }
    
    // Creep info
    if (ImGui::CollapsingHeader("Creep Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawCreepInfo(world);
    }
    
    // Tower info
    if (ImGui::CollapsingHeader("Tower Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawTowerInfo(world);
    }
    
    ImGui::End();
}

void GameMode::stopAndReset(World& world) {
    // Reset local game mode state
    paused_ = false;
    timeScale_ = 1.0f;
    stats_ = GameStats{};
    followCreep_ = false;
    followedCreep_ = INVALID_ENTITY;

    auto& em = world.getEntityManager();
    auto& reg = em.getRegistry();

    // Remove all creeps (runtime)
    {
        Vector<Entity> toRemove;
        auto view = reg.view<CreepComponent>();
        for (auto e : view) {
            toRemove.push_back(e);
        }
        for (auto e : toRemove) {
            em.destroyEntity(e);
        }
    }

    // Remove all projectiles (runtime)
    {
        Vector<Entity> toRemove;
        auto view = reg.view<ProjectileComponent>();
        for (auto e : view) {
            toRemove.push_back(e);
        }
        for (auto e : toRemove) {
            em.destroyEntity(e);
        }
    }

    // Reset tower runtime cooldown state
    {
        Vector<Entity> towers;
        auto view = reg.view<TowerRuntimeComponent>();
        for (auto e : view) {
            towers.push_back(e);
        }
        for (auto e : towers) {
            em.removeComponent<TowerRuntimeComponent>(e);
        }
    }

    // Restore buildings/towers/base HP (so "reset" feels like a clean round)
    {
        auto view = reg.view<HealthComponent, ObjectComponent>();
        for (auto e : view) {
            auto& hp = view.get<HealthComponent>(e);
            const auto& obj = view.get<ObjectComponent>(e);
            if (obj.type == ObjectType::Tower || obj.type == ObjectType::Building || obj.type == ObjectType::Base) {
                hp.currentHealth = hp.maxHealth;
                hp.isDead = false;
            }
        }
    }

    // Reset creep simulation timers (so first wave starts cleanly after restart)
    if (auto* cs = static_cast<CreepSystem*>(world.getSystem("CreepSystem"))) {
        cs->resetSimulation();
    }

    // Finally disable game mode
    gameModeActive_ = false;
}

void GameMode::update(World& world, f32 deltaTime) {
    // Update statistics every frame (even when paused)
    updateStats(world);
    
    if (!gameModeActive_ || paused_) {
        return;
    }
    
    // Apply time scale
    f32 scaledDeltaTime = deltaTime * timeScale_;
    
    // Update game time
    stats_.gameTime += scaledDeltaTime;
    
    // Note: world.update() is called from main.cpp with scaledDeltaTime
    // This function just tracks statistics and game time
}

void GameMode::drawTimeControls() {
    ImGui::Text("Time Controls");
    
    // Pause/Resume button
    if (paused_) {
        if (ImGui::Button("Resume")) {
            paused_ = false;
        }
    } else {
        if (ImGui::Button("Pause")) {
            paused_ = true;
        }
    }
    
    ImGui::SameLine();
    
    // Time scale slider
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Speed", &timeScale_, 0.1f, 5.0f, "%.1fx")) {
        timeScale_ = std::clamp(timeScale_, 0.1f, 5.0f);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("1x")) {
        timeScale_ = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("2x")) {
        timeScale_ = 2.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("5x")) {
        timeScale_ = 5.0f;
    }
    
    // Game time display
    ImGui::Text("Game Time: %.1f seconds", stats_.gameTime);
}

void GameMode::drawStatsPanel(World& world) {
    updateStats(world);
    
    ImGui::Text("Radiant (Team 1)");
    ImGui::BulletText("Creeps: %d", stats_.radiantCreeps);
    ImGui::BulletText("Towers: %d", stats_.radiantTowers);
    ImGui::BulletText("Buildings: %d", stats_.radiantBuildings);
    
    ImGui::Separator();
    
    ImGui::Text("Dire (Team 2)");
    ImGui::BulletText("Creeps: %d", stats_.direCreeps);
    ImGui::BulletText("Towers: %d", stats_.direTowers);
    ImGui::BulletText("Buildings: %d", stats_.direBuildings);
    
    ImGui::Separator();
    
    ImGui::Text("Total");
    ImGui::BulletText("Creeps Spawned: %d", stats_.totalCreepsSpawned);
    ImGui::BulletText("Creeps Killed: %d", stats_.totalCreepsKilled);
}

void GameMode::drawCreepInfo(World& world) {
    auto& reg = world.getEntityManager().getRegistry();
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    
    // Count creeps by team and state
    i32 radiantAlive = 0, radiantDead = 0;
    i32 direAlive = 0, direDead = 0;
    
    Vector<Entity> creeps;
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        creeps.push_back(entity);
        
        if (creep.teamId == 1) {
            if (creep.state == CreepState::Dead) {
                radiantDead++;
            } else {
                radiantAlive++;
            }
        } else if (creep.teamId == 2) {
            if (creep.state == CreepState::Dead) {
                direDead++;
            } else {
                direAlive++;
            }
        }
    }
    
    ImGui::Text("Radiant Creeps: %d alive, %d dead", radiantAlive, radiantDead);
    ImGui::Text("Dire Creeps: %d alive, %d dead", direAlive, direDead);
    
    // Show details for first few creeps
    if (ImGui::TreeNode("Creep Details")) {
        int shown = 0;
        for (Entity entity : creeps) {
            if (shown >= 10) {
                ImGui::Text("... and %d more", static_cast<int>(creeps.size()) - shown);
                break;
            }
            
            const auto& creep = reg.get<CreepComponent>(entity);
            const auto& transform = reg.get<TransformComponent>(entity);
            
            if (creep.state == CreepState::Dead) {
                continue;
            }
            
            const char* laneName = "Unknown";
            switch (creep.lane) {
                case CreepLane::Top: laneName = "Top"; break;
                case CreepLane::Middle: laneName = "Middle"; break;
                case CreepLane::Bottom: laneName = "Bottom"; break;
            }
            
            const char* stateName = "Unknown";
            switch (creep.state) {
                case CreepState::Idle: stateName = "Idle"; break;
                case CreepState::Moving: stateName = "Moving"; break;
                case CreepState::Attacking: stateName = "Attacking"; break;
                case CreepState::Dead: stateName = "Dead"; break;
            }
            
            ImGui::Text("Team %d | %s Lane | %s | HP: %.0f/%.0f | Pos: (%.1f, %.1f, %.1f)",
                       creep.teamId, laneName, stateName,
                       creep.currentHealth, creep.maxHealth,
                       transform.position.x, transform.position.y, transform.position.z);
            
            shown++;
        }
        ImGui::TreePop();
    }
}

void GameMode::drawTowerInfo(World& world) {
    auto& reg = world.getEntityManager().getRegistry();
    auto towerView = reg.view<ObjectComponent, TransformComponent>();
    
    Vector<Entity> towers;
    Vector<Entity> buildings;
    
    for (auto entity : towerView) {
        const auto& objComp = towerView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::Tower) {
            towers.push_back(entity);
        } else if (objComp.type == ObjectType::Building) {
            buildings.push_back(entity);
        }
    }
    
    // Towers
    ImGui::Text("Towers: %d", static_cast<int>(towers.size()));
    if (ImGui::TreeNode("Tower Details")) {
        for (Entity entity : towers) {
            const auto& objComp = reg.get<ObjectComponent>(entity);
            const auto& transform = reg.get<TransformComponent>(entity);
            
            bool hasHealth = reg.all_of<HealthComponent>(entity);
            if (hasHealth) {
                const auto& health = reg.get<HealthComponent>(entity);
                ImGui::Text("Team %d | HP: %.0f/%.0f | Armor: %.1f | Pos: (%.1f, %.1f, %.1f) %s",
                           objComp.teamId, health.currentHealth, health.maxHealth, health.armor,
                           transform.position.x, transform.position.y, transform.position.z,
                           health.isDead ? "[DEAD]" : "");
            } else {
                ImGui::Text("Team %d | No Health Component | Pos: (%.1f, %.1f, %.1f)",
                           objComp.teamId,
                           transform.position.x, transform.position.y, transform.position.z);
            }
        }
        ImGui::TreePop();
    }
    
    ImGui::Separator();
    
    // Buildings
    ImGui::Text("Buildings: %d", static_cast<int>(buildings.size()));
    if (ImGui::TreeNode("Building Details")) {
        for (Entity entity : buildings) {
            const auto& objComp = reg.get<ObjectComponent>(entity);
            const auto& transform = reg.get<TransformComponent>(entity);
            
            bool hasHealth = reg.all_of<HealthComponent>(entity);
            if (hasHealth) {
                const auto& health = reg.get<HealthComponent>(entity);
                ImGui::Text("Team %d | HP: %.0f/%.0f | Armor: %.1f | Pos: (%.1f, %.1f, %.1f) %s",
                           objComp.teamId, health.currentHealth, health.maxHealth, health.armor,
                           transform.position.x, transform.position.y, transform.position.z,
                           health.isDead ? "[DEAD]" : "");
            } else {
                ImGui::Text("Team %d | No Health Component | Pos: (%.1f, %.1f, %.1f)",
                           objComp.teamId,
                           transform.position.x, transform.position.y, transform.position.z);
            }
        }
        ImGui::TreePop();
    }
}

void GameMode::updateStats(World& world) {
    auto& reg = world.getEntityManager().getRegistry();
    
    // Reset counters
    stats_.radiantCreeps = 0;
    stats_.direCreeps = 0;
    stats_.radiantTowers = 0;
    stats_.direTowers = 0;
    stats_.radiantBuildings = 0;
    stats_.direBuildings = 0;
    
    // Count creeps
    auto creepView = reg.view<CreepComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.state != CreepState::Dead) {
            if (creep.teamId == 1) {
                stats_.radiantCreeps++;
            } else if (creep.teamId == 2) {
                stats_.direCreeps++;
            }
        }
    }
    
    // Count towers and buildings
    auto objView = reg.view<ObjectComponent>();
    for (auto entity : objView) {
        const auto& objComp = objView.get<ObjectComponent>(entity);
        
        if (objComp.type == ObjectType::Tower) {
            if (objComp.teamId == 1) {
                stats_.radiantTowers++;
            } else if (objComp.teamId == 2) {
                stats_.direTowers++;
            }
        } else if (objComp.type == ObjectType::Building) {
            if (objComp.teamId == 1) {
                stats_.radiantBuildings++;
            } else if (objComp.teamId == 2) {
                stats_.direBuildings++;
            }
        }
    }
}

Entity GameMode::findNearestCreep(World& world, const Vec3& position) {
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = std::numeric_limits<f32>::max();
    
    auto& reg = world.getEntityManager().getRegistry();
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.state == CreepState::Dead) {
            continue;
        }
        
        const auto& transform = creepView.get<TransformComponent>(entity);
        Vec3 diff = transform.position - position;
        f32 dist = glm::length(diff);
        
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}

void GameMode::updateCameraFollow(World& world, EditorCamera& camera) {
    if (!followCreep_ || followedCreep_ == INVALID_ENTITY) {
        return;
    }
    
    if (!world.isValid(followedCreep_)) {
        followCreep_ = false;
        followedCreep_ = INVALID_ENTITY;
        return;
    }
    
    auto& reg = world.getEntityManager().getRegistry();
    if (!reg.all_of<TransformComponent>(followedCreep_)) {
        followCreep_ = false;
        followedCreep_ = INVALID_ENTITY;
        return;
    }
    
    const auto& transform = reg.get<TransformComponent>(followedCreep_);
    // Camera follow would be implemented here
    // For now, just track the creep position
}

void GameMode::drawUnitHealthBars(World& world, const Mat4& viewProj, const Vec2& viewportSize, const ImVec2& viewportRectMin) {
    if (!gameModeActive_) {
        return;
    }
    
    auto& reg = world.getEntityManager().getRegistry();
    // Use foreground draw list to draw on top of everything
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Clip to viewport so UI overlays don't draw outside the viewport.
    const ImVec2 clipMin = viewportRectMin;
    const ImVec2 clipMax = ImVec2(viewportRectMin.x + viewportSize.x, viewportRectMin.y + viewportSize.y);
    drawList->PushClipRect(clipMin, clipMax, true);

    auto project = [&](const Vec3& worldPos, Vec2& outScreen) -> bool {
        const Vec4 clip = viewProj * Vec4(worldPos, 1.0f);
        // Behind camera or invalid.
        if (clip.w <= 0.0001f || !std::isfinite(clip.w)) {
            return false;
        }
        const Vec3 ndc = Vec3(clip) / clip.w;
        // D3D depth range is [0..1] with *_ZO projections.
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z)) {
            return false;
        }
        if (ndc.z < 0.0f || ndc.z > 1.0f) {
            return false;
        }
        outScreen.x = (ndc.x + 1.0f) * 0.5f * viewportSize.x;
        outScreen.y = (1.0f - ndc.y) * 0.5f * viewportSize.y;
        return true;
    };
    
    // Draw HP bars for creeps
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        const auto& transform = creepView.get<TransformComponent>(entity);
        
        if (creep.state == CreepState::Dead) {
            continue;
        }
        
        // Position bar above unit
        Vec3 barPosition = transform.position + Vec3(0.0f, 3.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPosition, screenPos)) {
            continue;
        }
        
        // Convert to screen coordinates (add viewport offset)
        screenPos.x += viewportRectMin.x;
        screenPos.y += viewportRectMin.y;
        
        // Skip if behind camera or outside viewport
        if (screenPos.x < viewportRectMin.x - 100.0f || screenPos.x > viewportRectMin.x + viewportSize.x + 100.0f ||
            screenPos.y < viewportRectMin.y - 100.0f || screenPos.y > viewportRectMin.y + viewportSize.y + 100.0f) {
            continue;
        }
        
        // Bar dimensions
        const f32 barWidth = 60.0f;
        const f32 barHeight = 8.0f;
        const f32 barOffsetY = -barHeight - 2.0f;
        
        // Background bar (black outline)
        ImVec2 barMin(screenPos.x - barWidth * 0.5f, screenPos.y + barOffsetY);
        ImVec2 barMax(screenPos.x + barWidth * 0.5f, screenPos.y + barOffsetY + barHeight);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 200));
        
        // HP bar (red to green gradient)
        f32 hpPercent = (creep.maxHealth > 0.0001f) ? (creep.currentHealth / creep.maxHealth) : 0.0f;
        hpPercent = std::clamp(hpPercent, 0.0f, 1.0f);
        ImU32 hpColor = IM_COL32(
            static_cast<u8>(255.0f * (1.0f - hpPercent)),
            static_cast<u8>(255.0f * hpPercent),
            0,
            255
        );
        ImVec2 hpMax(barMin.x + barWidth * hpPercent, barMax.y);
        drawList->AddRectFilled(barMin, hpMax, hpColor);
        
        // Border
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
        
        // HP text
        char hpText[32];
        snprintf(hpText, sizeof(hpText), "%.0f/%.0f", creep.currentHealth, creep.maxHealth);
        ImVec2 textSize = ImGui::CalcTextSize(hpText);
        ImVec2 textPos(screenPos.x - textSize.x * 0.5f, screenPos.y + barOffsetY - textSize.y - 2.0f);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), hpText);
    }
    
    // Draw HP bars for towers and buildings
    auto objectView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : objectView) {
        const auto& objComp = objectView.get<ObjectComponent>(entity);
        const auto& transform = objectView.get<TransformComponent>(entity);
        
        // Only show bars for towers, buildings, and bases
        if (objComp.type != ObjectType::Tower && 
            objComp.type != ObjectType::Building && 
            objComp.type != ObjectType::Base) {
            continue;
        }
        
        if (!reg.all_of<HealthComponent>(entity)) {
            continue;
        }
        
        const auto& health = reg.get<HealthComponent>(entity);
        if (health.isDead) {
            continue;
        }
        
        // Position bar above building
        Vec3 barPosition = transform.position + Vec3(0.0f, 8.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPosition, screenPos)) {
            continue;
        }
        
        // Convert to screen coordinates (add viewport offset)
        screenPos.x += viewportRectMin.x;
        screenPos.y += viewportRectMin.y;
        
        // Skip if behind camera or outside viewport
        if (screenPos.x < viewportRectMin.x - 100.0f || screenPos.x > viewportRectMin.x + viewportSize.x + 100.0f ||
            screenPos.y < viewportRectMin.y - 100.0f || screenPos.y > viewportRectMin.y + viewportSize.y + 100.0f) {
            continue;
        }
        
        // Bar dimensions (larger for buildings)
        const f32 barWidth = 80.0f;
        const f32 barHeight = 10.0f;
        const f32 barOffsetY = -barHeight - 2.0f;
        
        // Background bar (black outline)
        ImVec2 barMin(screenPos.x - barWidth * 0.5f, screenPos.y + barOffsetY);
        ImVec2 barMax(screenPos.x + barWidth * 0.5f, screenPos.y + barOffsetY + barHeight);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 200));
        
        // HP bar (red to green gradient)
        f32 hpPercent = (health.maxHealth > 0.0001f) ? (health.currentHealth / health.maxHealth) : 0.0f;
        hpPercent = std::clamp(hpPercent, 0.0f, 1.0f);
        ImU32 hpColor = IM_COL32(
            static_cast<u8>(255.0f * (1.0f - hpPercent)),
            static_cast<u8>(255.0f * hpPercent),
            0,
            255
        );
        ImVec2 hpMax(barMin.x + barWidth * hpPercent, barMax.y);
        drawList->AddRectFilled(barMin, hpMax, hpColor);
        
        // Border
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
        
        // HP text
        char hpText[32];
        snprintf(hpText, sizeof(hpText), "%.0f/%.0f", health.currentHealth, health.maxHealth);
        ImVec2 textSize = ImGui::CalcTextSize(hpText);
        ImVec2 textPos(screenPos.x - textSize.x * 0.5f, screenPos.y + barOffsetY - textSize.y - 2.0f);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), hpText);
    }

    drawList->PopClipRect();
}

} // namespace WorldEditor
