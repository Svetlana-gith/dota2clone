#include "GameMode.h"
#include "world/World.h"
#include "world/EntityManager.h"
#include "world/CreepSystem.h"
#include "world/CreepSpawnSystem.h"
#include "world/HeroSystem.h"
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

    // Visual options
    ImGui::Checkbox("Show Tower Range", &showTowerRange_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Ability Indicators", &showAbilityIndicators_);

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
    if (auto* cs = dynamic_cast<CreepSpawnSystem*>(world.getSystem("CreepSpawnSystem"))) {
        cs->resetGame();
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
    
    // IMPORTANT: Clip all drawing to viewport bounds
    const ImVec2 clipMin = viewportRectMin;
    const ImVec2 clipMax = ImVec2(viewportRectMin.x + viewportSize.x, viewportRectMin.y + viewportSize.y);
    drawList->PushClipRect(clipMin, clipMax, true);
    
    // Lambda for world-to-screen projection
    auto project = [&](const Vec3& worldPos, Vec2& outScreen) -> bool {
        const Vec4 clip = viewProj * Vec4(worldPos, 1.0f);
        if (clip.w <= 0.0001f || !std::isfinite(clip.w)) return false;
        const Vec3 ndc = Vec3(clip) / clip.w;
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z)) return false;
        if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
        outScreen.x = (ndc.x + 1.0f) * 0.5f * viewportSize.x + viewportRectMin.x;
        outScreen.y = (1.0f - ndc.y) * 0.5f * viewportSize.y + viewportRectMin.y;
        return true;
    };
    
    // Helper to check if screen position is inside viewport
    auto isInViewport = [&](const Vec2& screenPos) -> bool {
        return screenPos.x >= clipMin.x && screenPos.x <= clipMax.x &&
               screenPos.y >= clipMin.y && screenPos.y <= clipMax.y;
    };
    
    // Draw tower attack range circles
    if (showTowerRange_) {
        auto towerView = reg.view<ObjectComponent, TransformComponent>();
        for (auto entity : towerView) {
            const auto& obj = towerView.get<ObjectComponent>(entity);
            if (obj.type != ObjectType::Tower) continue;
            
            const auto& transform = towerView.get<TransformComponent>(entity);
            
            // Draw range circle on ground
            const int segments = 32;
            const f32 range = obj.attackRange;
            ImU32 rangeColor = (obj.teamId == 1) ? IM_COL32(50, 200, 50, 80) : IM_COL32(200, 50, 50, 80);
            
            for (int i = 0; i < segments; i++) {
                f32 a1 = (f32)i / segments * 2.0f * 3.14159f;
                f32 a2 = (f32)(i + 1) / segments * 2.0f * 3.14159f;
                
                Vec3 p1 = transform.position + Vec3(std::cos(a1) * range, 0.1f, std::sin(a1) * range);
                Vec3 p2 = transform.position + Vec3(std::cos(a2) * range, 0.1f, std::sin(a2) * range);
                
                Vec2 s1, s2;
                if (project(p1, s1) && project(p2, s2)) {
                    drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), rangeColor, 2.0f);
                }
            }
        }
    }
    
    // Draw hero HP/MP bars and ability indicators
    auto heroView = reg.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        const auto& hero = heroView.get<HeroComponent>(entity);
        const auto& transform = heroView.get<TransformComponent>(entity);
        
        if (hero.state == HeroState::Dead) continue;
        
        Vec3 barPos = transform.position + Vec3(0.0f, 4.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPos, screenPos)) continue;
        
        // Hero name
        ImVec2 nameSize = ImGui::CalcTextSize(hero.heroName.c_str());
        drawList->AddText(ImVec2(screenPos.x - nameSize.x * 0.5f, screenPos.y - 40), 
            IM_COL32(255, 255, 255, 255), hero.heroName.c_str());
        
        // HP bar (larger for heroes)
        const f32 barWidth = 80.0f;
        const f32 barHeight = 10.0f;
        f32 hpPct = hero.maxHealth > 0 ? hero.currentHealth / hero.maxHealth : 0;
        hpPct = std::clamp(hpPct, 0.0f, 1.0f);
        
        ImVec2 hpMin(screenPos.x - barWidth * 0.5f, screenPos.y - 25);
        ImVec2 hpMax(screenPos.x + barWidth * 0.5f, screenPos.y - 25 + barHeight);
        drawList->AddRectFilled(hpMin, hpMax, IM_COL32(0, 0, 0, 200));
        drawList->AddRectFilled(hpMin, ImVec2(hpMin.x + barWidth * hpPct, hpMax.y), 
            IM_COL32((u8)(255 * (1 - hpPct)), (u8)(255 * hpPct), 0, 255));
        drawList->AddRect(hpMin, hpMax, IM_COL32(255, 255, 255, 255));
        
        // MP bar (blue, smaller)
        f32 mpPct = hero.maxMana > 0 ? hero.currentMana / hero.maxMana : 0;
        mpPct = std::clamp(mpPct, 0.0f, 1.0f);
        
        ImVec2 mpMin(screenPos.x - barWidth * 0.5f, screenPos.y - 12);
        ImVec2 mpMax(screenPos.x + barWidth * 0.5f, screenPos.y - 12 + 6);
        drawList->AddRectFilled(mpMin, mpMax, IM_COL32(0, 0, 0, 200));
        drawList->AddRectFilled(mpMin, ImVec2(mpMin.x + barWidth * mpPct, mpMax.y), 
            IM_COL32(50, 100, 200, 255));
        
        // Level indicator
        char lvlText[16];
        snprintf(lvlText, sizeof(lvlText), "Lv%d", hero.level);
        drawList->AddText(ImVec2(screenPos.x - barWidth * 0.5f - 25, screenPos.y - 25), 
            IM_COL32(255, 215, 0, 255), lvlText);
        
        // Draw ability range indicator if casting
        if (hero.currentCastingAbility >= 0 && hero.currentCastingAbility < 6) {
            const auto& ability = hero.abilities[hero.currentCastingAbility];
            if (ability.data.radius > 0) {
                // Draw AoE indicator
                const int segs = 24;
                f32 r = ability.data.radius;
                Vec3 center = hero.targetPosition;
                
                for (int i = 0; i < segs; i++) {
                    f32 a1 = (f32)i / segs * 2.0f * 3.14159f;
                    f32 a2 = (f32)(i + 1) / segs * 2.0f * 3.14159f;
                    
                    Vec3 p1 = center + Vec3(std::cos(a1) * r, 0.2f, std::sin(a1) * r);
                    Vec3 p2 = center + Vec3(std::cos(a2) * r, 0.2f, std::sin(a2) * r);
                    
                    Vec2 s1, s2;
                    if (project(p1, s1) && project(p2, s2)) {
                        drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), 
                            IM_COL32(255, 100, 100, 200), 2.0f);
                    }
                }
            }
        }
    }
    
    // Draw HP bars for creeps
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        const auto& transform = creepView.get<TransformComponent>(entity);
        
        if (creep.state == CreepState::Dead) continue;
        
        Vec3 barPosition = transform.position + Vec3(0.0f, 3.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPosition, screenPos)) continue;
        
        // Bar dimensions
        const f32 barWidth = 60.0f;
        const f32 barHeight = 8.0f;
        
        ImVec2 barMin(screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2);
        ImVec2 barMax(screenPos.x + barWidth * 0.5f, screenPos.y - 2);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 200));
        
        f32 hpPct = std::clamp(creep.currentHealth / creep.maxHealth, 0.0f, 1.0f);
        ImU32 hpColor = IM_COL32((u8)(255 * (1 - hpPct)), (u8)(255 * hpPct), 0, 255);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * hpPct, barMax.y), hpColor);
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255));
    }
    
    // Draw HP bars for towers and buildings
    auto objectView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : objectView) {
        const auto& objComp = objectView.get<ObjectComponent>(entity);
        const auto& transform = objectView.get<TransformComponent>(entity);
        
        if (objComp.type != ObjectType::Tower && objComp.type != ObjectType::Building && objComp.type != ObjectType::Base)
            continue;
        
        if (!reg.all_of<HealthComponent>(entity)) continue;
        const auto& health = reg.get<HealthComponent>(entity);
        if (health.isDead) continue;
        
        Vec3 barPosition = transform.position + Vec3(0.0f, 8.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPosition, screenPos)) continue;
        
        const f32 barWidth = 80.0f;
        const f32 barHeight = 10.0f;
        
        ImVec2 barMin(screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2);
        ImVec2 barMax(screenPos.x + barWidth * 0.5f, screenPos.y - 2);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 200));
        
        f32 hpPct = std::clamp(health.currentHealth / health.maxHealth, 0.0f, 1.0f);
        ImU32 hpColor = IM_COL32((u8)(255 * (1 - hpPct)), (u8)(255 * hpPct), 0, 255);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * hpPct, barMax.y), hpColor);
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255));
        
        // HP text
        char hpText[32];
        snprintf(hpText, sizeof(hpText), "%.0f/%.0f", health.currentHealth, health.maxHealth);
        ImVec2 textSize = ImGui::CalcTextSize(hpText);
        drawList->AddText(ImVec2(screenPos.x - textSize.x * 0.5f, screenPos.y - barHeight - textSize.y - 4), 
            IM_COL32(255, 255, 255, 255), hpText);
    }
    
    // Pop clip rect at the end
    drawList->PopClipRect();
}

void GameMode::drawTopBar(World& world, const Vec2& viewportSize, const ImVec2& viewportRectMin) {
    if (!gameModeActive_) {
        return;
    }
    
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // Top bar dimensions
    const f32 barHeight = 45.0f;
    const f32 portraitSize = 38.0f;
    const f32 portraitSpacing = 5.0f;
    const f32 timeBoxWidth = 80.0f;
    const i32 slotsPerTeam = 5;
    
    // Calculate positions
    const f32 centerX = viewportRectMin.x + viewportSize.x * 0.5f;
    const f32 topY = viewportRectMin.y + 5.0f;
    
    // Draw background bar
    ImVec2 barMin(viewportRectMin.x, topY);
    ImVec2 barMax(viewportRectMin.x + viewportSize.x, topY + barHeight);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(20, 20, 25, 220));
    drawList->AddLine(ImVec2(barMin.x, barMax.y), ImVec2(barMax.x, barMax.y), IM_COL32(60, 60, 70, 255), 2.0f);
    
    // Draw time in center
    i32 minutes = static_cast<i32>(stats_.gameTime) / 60;
    i32 seconds = static_cast<i32>(stats_.gameTime) % 60;
    char timeText[16];
    snprintf(timeText, sizeof(timeText), "%02d:%02d", minutes, seconds);
    
    ImVec2 timeBoxMin(centerX - timeBoxWidth * 0.5f, topY + 3);
    ImVec2 timeBoxMax(centerX + timeBoxWidth * 0.5f, topY + barHeight - 3);
    drawList->AddRectFilled(timeBoxMin, timeBoxMax, IM_COL32(40, 40, 50, 255));
    drawList->AddRect(timeBoxMin, timeBoxMax, IM_COL32(80, 80, 100, 255), 0, 0, 2.0f);
    
    ImVec2 timeTextSize = ImGui::CalcTextSize(timeText);
    drawList->AddText(ImVec2(centerX - timeTextSize.x * 0.5f, topY + (barHeight - timeTextSize.y) * 0.5f),
        IM_COL32(255, 255, 255, 255), timeText);
    
    // Collect heroes by team
    auto& reg = world.getEntityManager().getRegistry();
    auto heroView = reg.view<HeroComponent, TransformComponent>();
    
    Vector<Entity> radiantHeroes;  // Team 1
    Vector<Entity> direHeroes;     // Team 2
    
    for (auto entity : heroView) {
        const auto& hero = heroView.get<HeroComponent>(entity);
        if (hero.teamId == 1) {
            radiantHeroes.push_back(entity);
        } else if (hero.teamId == 2) {
            direHeroes.push_back(entity);
        }
    }
    
    // Draw Radiant (left side) - green tint
    f32 radiantStartX = centerX - timeBoxWidth * 0.5f - 20.0f - (slotsPerTeam * (portraitSize + portraitSpacing));
    
    for (i32 i = 0; i < slotsPerTeam; i++) {
        f32 slotX = radiantStartX + i * (portraitSize + portraitSpacing);
        f32 slotY = topY + (barHeight - portraitSize) * 0.5f;
        
        ImVec2 slotMin(slotX, slotY);
        ImVec2 slotMax(slotX + portraitSize, slotY + portraitSize);
        
        if (i < static_cast<i32>(radiantHeroes.size())) {
            Entity heroEntity = radiantHeroes[i];
            const auto& hero = reg.get<HeroComponent>(heroEntity);
            
            bool isDead = (hero.state == HeroState::Dead);
            
            // Portrait background
            ImU32 bgColor = isDead ? IM_COL32(40, 40, 40, 255) : IM_COL32(30, 80, 30, 255);
            drawList->AddRectFilled(slotMin, slotMax, bgColor);
            
            // HP bar under portrait
            f32 hpPct = hero.maxHealth > 0 ? std::clamp(hero.currentHealth / hero.maxHealth, 0.0f, 1.0f) : 0;
            ImVec2 hpMin(slotX, slotY + portraitSize - 4);
            ImVec2 hpMax(slotX + portraitSize, slotY + portraitSize);
            drawList->AddRectFilled(hpMin, hpMax, IM_COL32(0, 0, 0, 200));
            drawList->AddRectFilled(hpMin, ImVec2(hpMin.x + portraitSize * hpPct, hpMax.y), 
                IM_COL32(50, 200, 50, 255));
            
            // Hero initial/icon
            char heroInitial[4];
            snprintf(heroInitial, sizeof(heroInitial), "%c", hero.heroName.empty() ? '?' : hero.heroName[0]);
            ImVec2 textSize = ImGui::CalcTextSize(heroInitial);
            drawList->AddText(ImVec2(slotX + (portraitSize - textSize.x) * 0.5f, 
                slotY + (portraitSize - textSize.y) * 0.5f - 2),
                isDead ? IM_COL32(100, 100, 100, 255) : IM_COL32(255, 255, 255, 255), heroInitial);
            
            // Death timer overlay
            if (isDead && hero.respawnTimer > 0) {
                // Dark overlay
                drawList->AddRectFilled(slotMin, slotMax, IM_COL32(0, 0, 0, 180));
                
                // Respawn timer
                char timerText[8];
                snprintf(timerText, sizeof(timerText), "%.0f", hero.respawnTimer);
                ImVec2 timerSize = ImGui::CalcTextSize(timerText);
                drawList->AddText(ImVec2(slotX + (portraitSize - timerSize.x) * 0.5f,
                    slotY + (portraitSize - timerSize.y) * 0.5f),
                    IM_COL32(255, 80, 80, 255), timerText);
            }
            
            // Border
            drawList->AddRect(slotMin, slotMax, IM_COL32(50, 150, 50, 255), 0, 0, 2.0f);
        } else {
            // Empty slot
            drawList->AddRectFilled(slotMin, slotMax, IM_COL32(30, 35, 30, 200));
            drawList->AddRect(slotMin, slotMax, IM_COL32(50, 60, 50, 150), 0, 0, 1.0f);
        }
    }
    
    // Draw Dire (right side) - red tint
    f32 direStartX = centerX + timeBoxWidth * 0.5f + 20.0f;
    
    for (i32 i = 0; i < slotsPerTeam; i++) {
        f32 slotX = direStartX + i * (portraitSize + portraitSpacing);
        f32 slotY = topY + (barHeight - portraitSize) * 0.5f;
        
        ImVec2 slotMin(slotX, slotY);
        ImVec2 slotMax(slotX + portraitSize, slotY + portraitSize);
        
        if (i < static_cast<i32>(direHeroes.size())) {
            Entity heroEntity = direHeroes[i];
            const auto& hero = reg.get<HeroComponent>(heroEntity);
            
            bool isDead = (hero.state == HeroState::Dead);
            
            // Portrait background
            ImU32 bgColor = isDead ? IM_COL32(40, 40, 40, 255) : IM_COL32(80, 30, 30, 255);
            drawList->AddRectFilled(slotMin, slotMax, bgColor);
            
            // HP bar under portrait
            f32 hpPct = hero.maxHealth > 0 ? std::clamp(hero.currentHealth / hero.maxHealth, 0.0f, 1.0f) : 0;
            ImVec2 hpMin(slotX, slotY + portraitSize - 4);
            ImVec2 hpMax(slotX + portraitSize, slotY + portraitSize);
            drawList->AddRectFilled(hpMin, hpMax, IM_COL32(0, 0, 0, 200));
            drawList->AddRectFilled(hpMin, ImVec2(hpMin.x + portraitSize * hpPct, hpMax.y), 
                IM_COL32(200, 50, 50, 255));
            
            // Hero initial/icon
            char heroInitial[4];
            snprintf(heroInitial, sizeof(heroInitial), "%c", hero.heroName.empty() ? '?' : hero.heroName[0]);
            ImVec2 textSize = ImGui::CalcTextSize(heroInitial);
            drawList->AddText(ImVec2(slotX + (portraitSize - textSize.x) * 0.5f, 
                slotY + (portraitSize - textSize.y) * 0.5f - 2),
                isDead ? IM_COL32(100, 100, 100, 255) : IM_COL32(255, 255, 255, 255), heroInitial);
            
            // Death timer overlay
            if (isDead && hero.respawnTimer > 0) {
                // Dark overlay
                drawList->AddRectFilled(slotMin, slotMax, IM_COL32(0, 0, 0, 180));
                
                // Respawn timer
                char timerText[8];
                snprintf(timerText, sizeof(timerText), "%.0f", hero.respawnTimer);
                ImVec2 timerSize = ImGui::CalcTextSize(timerText);
                drawList->AddText(ImVec2(slotX + (portraitSize - timerSize.x) * 0.5f,
                    slotY + (portraitSize - timerSize.y) * 0.5f),
                    IM_COL32(255, 80, 80, 255), timerText);
            }
            
            // Border
            drawList->AddRect(slotMin, slotMax, IM_COL32(150, 50, 50, 255), 0, 0, 2.0f);
        } else {
            // Empty slot
            drawList->AddRectFilled(slotMin, slotMax, IM_COL32(35, 30, 30, 200));
            drawList->AddRect(slotMin, slotMax, IM_COL32(60, 50, 50, 150), 0, 0, 1.0f);
        }
    }
}

} // namespace WorldEditor
