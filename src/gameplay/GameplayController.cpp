#include "GameplayController.h"
#include "world/World.h"
#include "world/EntityManager.h"
#include "world/Components.h"
#include "world/HeroSystem.h"
#include "world/CreepSpawnSystem.h"
#include "world/TerrainRaycast.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace WorldEditor {

GameplayController::GameplayController() {
    // Setup default Dota-style camera (scaled for 16000x16000 map)
    camera_.yawDeg = -45.0f;
    camera_.pitchDeg = -45.0f;
    camera_.fovDeg = 60.0f;
    camera_.nearPlane = 1.0f;
    camera_.farPlane = 50000.0f;
    camera_.orthographic = false;
    camera_.lockTopDown = false;
    camera_.moveSpeed = 2500.0f;  // Scaled for 16000 map
    camera_.position = Vec3(8000.0f, 1200.0f, 8000.0f);  // Center of map, Dota-like height
}

GameplayController::~GameplayController() = default;

void GameplayController::setWorld(World* world) {
    world_ = world;
    
    if (world_) {
        // Try to find player hero
        if (auto* heroSystem = getHeroSystem()) {
            playerHero_ = heroSystem->getPlayerHero();
        }
    }
}

HeroSystem* GameplayController::getHeroSystem() const {
    if (!world_) return nullptr;
    return dynamic_cast<HeroSystem*>(world_->getSystem("HeroSystem"));
}

// ==================== Game State ====================

void GameplayController::startGame() {
    if (!world_) return;
    
    gameActive_ = true;
    paused_ = false;
    stats_ = GameplayStats{};
    
    world_->startGame();
    spdlog::info("GameplayController: Game started");
}

void GameplayController::stopGame() {
    gameActive_ = false;
    paused_ = false;
    spdlog::info("GameplayController: Game stopped");
}

void GameplayController::pauseGame() {
    paused_ = true;
}

void GameplayController::resumeGame() {
    paused_ = false;
}

void GameplayController::resetGame() {
    if (!world_) return;
    
    gameActive_ = false;
    paused_ = false;
    timeScale_ = 1.0f;
    stats_ = GameplayStats{};
    
    world_->resetGame();
    
    // Clear selection
    clearSelection();
    
    spdlog::info("GameplayController: Game reset");
}

// ==================== Update ====================

f32 GameplayController::update(f32 deltaTime, const GameplayInput& input) {
    if (!world_) return deltaTime;
    
    // Calculate scaled delta time
    f32 scaledDeltaTime = deltaTime;
    if (gameActive_ && !paused_) {
        scaledDeltaTime = deltaTime * timeScale_;
        stats_.gameTime += scaledDeltaTime;
    }
    
    // Always update camera
    updateCamera(deltaTime, input);
    
    // Update input handling (selection, commands)
    updateInput(input);
    
    // Update statistics
    updateStats();
    
    // Update selected unit info
    updateSelectedUnitInfo();
    
    // Store last input
    lastInput_ = input;
    
    return scaledDeltaTime;
}

void GameplayController::updateCameraOnly(f32 deltaTime, const GameplayInput& input) {
    updateCamera(deltaTime, input);
    lastInput_ = input;
}

// ==================== Camera ====================

void GameplayController::updateCamera(f32 deltaTime, const GameplayInput& input) {
    switch (cameraMode_) {
        case CameraMode::Free:
            // Use EditorCamera's built-in input handling
            if (hwnd_) {
                bool enableMouseLook = input.rightHeld;
                bool enableKeyboard = true;
                camera_.updateFromInput(hwnd_, deltaTime, enableMouseLook, enableKeyboard);
            }
            break;
            
        case CameraMode::RTS:
            // RTS-style: WASD pan + edge panning + scroll zoom
            updateKeyboardCamera(deltaTime, input);
            if (edgePanEnabled_ && input.mouseInViewport) {
                updateEdgePanning(deltaTime, input);
            }
            updateMouseCamera(deltaTime, input);
            break;
            
        case CameraMode::FollowHero:
            // Follow player hero
            updateCameraFollow(deltaTime);
            // Also allow manual camera adjustment with keyboard
            updateKeyboardCamera(deltaTime, input);
            break;
    }
}

void GameplayController::updateKeyboardCamera(f32 deltaTime, const GameplayInput& input) {
    // Get camera directions on XZ plane (Dota-style isometric)
    const float yaw = glm::radians(camera_.yawDeg);
    const Mat4 rotY = glm::rotate(Mat4(1.0f), yaw, Vec3(0.0f, 1.0f, 0.0f));
    const Vec3 panRight = Vec3(rotY * Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    const Vec3 panForward = Vec3(rotY * Vec4(0.0f, 0.0f, 1.0f, 0.0f));
    
    f32 speed = camera_.moveSpeed * (input.shiftHeld ? camera_.fastMultiplier : 1.0f);
    
    Vec3 move(0.0f);
    if (input.keys['W'] || input.keys[VK_UP])    move += panForward;
    if (input.keys['S'] || input.keys[VK_DOWN])  move -= panForward;
    if (input.keys['D'] || input.keys[VK_RIGHT]) move += panRight;
    if (input.keys['A'] || input.keys[VK_LEFT])  move -= panRight;
    
    float len = glm::length(move);
    if (len > 0.0001f) {
        camera_.position += (move / len) * speed * deltaTime;
    }
}

void GameplayController::updateEdgePanning(f32 deltaTime, const GameplayInput& input) {
    // Check if mouse is at screen edges
    f32 mouseX = input.mousePos.x;
    f32 mouseY = input.mousePos.y;
    
    // Use viewport bounds if available, otherwise screen bounds
    f32 left = input.viewportMin.x;
    f32 top = input.viewportMin.y;
    f32 right = input.viewportMax.x;
    f32 bottom = input.viewportMax.y;
    
    Vec3 panDir(0.0f);
    
    // Left edge
    if (mouseX < left + edgePanMargin_) {
        panDir.x -= 1.0f;
    }
    // Right edge
    if (mouseX > right - edgePanMargin_) {
        panDir.x += 1.0f;
    }
    // Top edge
    if (mouseY < top + edgePanMargin_) {
        panDir.z += 1.0f;
    }
    // Bottom edge
    if (mouseY > bottom - edgePanMargin_) {
        panDir.z -= 1.0f;
    }
    
    if (glm::length(panDir) > 0.0001f) {
        // Rotate pan direction by camera yaw
        const float yaw = glm::radians(camera_.yawDeg);
        const Mat4 rotY = glm::rotate(Mat4(1.0f), yaw, Vec3(0.0f, 1.0f, 0.0f));
        Vec3 worldPan = Vec3(rotY * Vec4(panDir.x, 0.0f, panDir.z, 0.0f));
        
        camera_.position += glm::normalize(worldPan) * edgePanSpeed_ * deltaTime;
    }
}

void GameplayController::updateMouseCamera(f32 deltaTime, const GameplayInput& input) {
    // Middle mouse drag to pan
    if (input.keys[VK_MBUTTON]) {
        const float yaw = glm::radians(camera_.yawDeg);
        const Mat4 rotY = glm::rotate(Mat4(1.0f), yaw, Vec3(0.0f, 1.0f, 0.0f));
        const Vec3 panRight = Vec3(rotY * Vec4(1.0f, 0.0f, 0.0f, 0.0f));
        const Vec3 panForward = Vec3(rotY * Vec4(0.0f, 0.0f, 1.0f, 0.0f));
        
        camera_.position -= panRight * input.mouseDelta.x * 0.5f;
        camera_.position += panForward * input.mouseDelta.y * 0.5f;
    }
    
    // Scroll wheel to zoom
    if (std::abs(input.scrollDelta) > 0.01f) {
        // Move camera along its forward direction
        Vec3 forward = camera_.getForwardLH();
        camera_.position += forward * input.scrollDelta * 10.0f;
        
        // Clamp height
        camera_.position.y = std::clamp(camera_.position.y, 20.0f, 500.0f);
    }
}

void GameplayController::updateCameraFollow(f32 deltaTime) {
    if (playerHero_ == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(playerHero_)) return;
    if (!reg.all_of<TransformComponent>(playerHero_)) return;
    
    const auto& transform = reg.get<TransformComponent>(playerHero_);
    
    // Calculate camera offset
    Vec3 forward = camera_.getForwardLH();
    f32 distance = 100.0f;
    f32 height = 80.0f;
    
    Vec3 targetPos = transform.position - forward * distance;
    targetPos.y = height;
    
    // Smooth follow
    f32 smoothness = 5.0f;
    camera_.position = glm::mix(camera_.position, targetPos, smoothness * deltaTime);
}

void GameplayController::focusOnPosition(const Vec3& position) {
    // Calculate camera position to look at target (scaled for 16000x16000 map)
    Vec3 forward = camera_.getForwardLH();
    f32 distance = 1500.0f;  // Scaled for 16000 map
    f32 height = 1200.0f;    // Dota-like camera height
    
    camera_.position = position - forward * distance;
    camera_.position.y = height;
}

void GameplayController::focusOnEntity(Entity entity) {
    if (!world_ || entity == INVALID_ENTITY) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(entity)) return;
    if (!reg.all_of<TransformComponent>(entity)) return;
    
    const auto& transform = reg.get<TransformComponent>(entity);
    focusOnPosition(transform.position);
}

Mat4 GameplayController::getViewProjectionMatrix(f32 aspectRatio) const {
    return camera_.getViewProjLH_ZO(aspectRatio);
}

// ==================== Selection ====================

void GameplayController::selectEntity(Entity entity) {
    selectedEntity_ = entity;
    updateSelectedUnitInfo();
    
    if (entity != INVALID_ENTITY) {
        spdlog::debug("GameplayController: Selected entity {}", static_cast<u32>(entity));
    }
}

void GameplayController::clearSelection() {
    selectedEntity_ = INVALID_ENTITY;
    selectedUnitInfo_ = SelectedUnitInfo{};
}

void GameplayController::updateSelectedUnitInfo() {
    selectedUnitInfo_ = SelectedUnitInfo{};
    
    if (selectedEntity_ == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(selectedEntity_)) {
        selectedEntity_ = INVALID_ENTITY;
        return;
    }
    
    selectedUnitInfo_.entity = selectedEntity_;
    
    // Get name
    if (reg.all_of<NameComponent>(selectedEntity_)) {
        selectedUnitInfo_.name = reg.get<NameComponent>(selectedEntity_).name;
    }
    
    // Check for hero
    if (reg.all_of<HeroComponent>(selectedEntity_)) {
        const auto& hero = reg.get<HeroComponent>(selectedEntity_);
        selectedUnitInfo_.isHero = true;
        selectedUnitInfo_.teamId = hero.teamId;
        selectedUnitInfo_.currentHealth = hero.currentHealth;
        selectedUnitInfo_.maxHealth = hero.maxHealth;
        selectedUnitInfo_.currentMana = hero.currentMana;
        selectedUnitInfo_.maxMana = hero.maxMana;
        selectedUnitInfo_.name = hero.heroName;
    }
    // Check for creep
    else if (reg.all_of<CreepComponent>(selectedEntity_)) {
        const auto& creep = reg.get<CreepComponent>(selectedEntity_);
        selectedUnitInfo_.isCreep = true;
        selectedUnitInfo_.teamId = creep.teamId;
        selectedUnitInfo_.currentHealth = creep.currentHealth;
        selectedUnitInfo_.maxHealth = creep.maxHealth;
        selectedUnitInfo_.name = "Creep";
    }
    // Check for tower/building
    else if (reg.all_of<ObjectComponent>(selectedEntity_)) {
        const auto& obj = reg.get<ObjectComponent>(selectedEntity_);
        selectedUnitInfo_.teamId = obj.teamId;
        
        if (obj.type == ObjectType::Tower) {
            selectedUnitInfo_.isTower = true;
            selectedUnitInfo_.name = "Tower";
        } else if (obj.type == ObjectType::Building || obj.type == ObjectType::Base) {
            selectedUnitInfo_.isBuilding = true;
            selectedUnitInfo_.name = "Building";
        }
        
        if (reg.all_of<HealthComponent>(selectedEntity_)) {
            const auto& health = reg.get<HealthComponent>(selectedEntity_);
            selectedUnitInfo_.currentHealth = health.currentHealth;
            selectedUnitInfo_.maxHealth = health.maxHealth;
        }
    }
}

// ==================== Commands ====================

void GameplayController::commandMoveTo(const Vec3& position) {
    Entity target = (selectedEntity_ != INVALID_ENTITY) ? selectedEntity_ : playerHero_;
    if (target == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(target)) return;
    
    // Only heroes can receive move commands
    if (reg.all_of<HeroComponent>(target)) {
        if (auto* heroSystem = getHeroSystem()) {
            heroSystem->moveToPosition(target, position);
            spdlog::debug("GameplayController: Move command to ({}, {}, {})", 
                         position.x, position.y, position.z);
        }
    }
}

void GameplayController::commandAttackMove(const Vec3& position) {
    Entity target = (selectedEntity_ != INVALID_ENTITY) ? selectedEntity_ : playerHero_;
    if (target == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(target)) return;
    
    if (reg.all_of<HeroComponent>(target)) {
        if (auto* heroSystem = getHeroSystem()) {
            HeroCommand cmd;
            cmd.type = HeroCommand::Type::AttackMove;
            cmd.targetPosition = position;
            heroSystem->issueCommand(target, cmd);
        }
    }
}

void GameplayController::commandAttackTarget(Entity targetEntity) {
    Entity attacker = (selectedEntity_ != INVALID_ENTITY) ? selectedEntity_ : playerHero_;
    if (attacker == INVALID_ENTITY || targetEntity == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(attacker) || !reg.valid(targetEntity)) return;
    
    if (reg.all_of<HeroComponent>(attacker)) {
        if (auto* heroSystem = getHeroSystem()) {
            heroSystem->attackTarget(attacker, targetEntity);
        }
    }
}

void GameplayController::commandCastAbility(i32 abilityIndex, const Vec3& targetPos, Entity targetEntity) {
    Entity caster = (selectedEntity_ != INVALID_ENTITY) ? selectedEntity_ : playerHero_;
    if (caster == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(caster)) return;
    
    if (reg.all_of<HeroComponent>(caster)) {
        if (auto* heroSystem = getHeroSystem()) {
            heroSystem->castAbility(caster, abilityIndex, targetPos, targetEntity);
        }
    }
}

void GameplayController::commandStop() {
    Entity target = (selectedEntity_ != INVALID_ENTITY) ? selectedEntity_ : playerHero_;
    if (target == INVALID_ENTITY || !world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    if (!reg.valid(target)) return;
    
    if (reg.all_of<HeroComponent>(target)) {
        if (auto* heroSystem = getHeroSystem()) {
            heroSystem->stopHero(target);
        }
    }
}

// ==================== Input Handling ====================

void GameplayController::updateInput(const GameplayInput& input) {
    // Handle left click (selection)
    if (input.leftClick && !lastInput_.leftClick) {
        handleLeftClick(input);
    }
    
    // Handle right click (commands)
    if (input.rightClick && !lastInput_.rightClick) {
        handleRightClick(input);
    }
    
    // Handle ability keys
    handleAbilityKeys(input);
    
    // Handle stop key (S)
    if (input.keys['S'] && !lastInput_.keys['S'] && !input.keys['W']) {
        // S without WASD movement = stop command
        // (S with movement is handled in camera)
    }
    
    // Handle hold position (H)
    if (input.keys['H'] && !lastInput_.keys['H']) {
        commandStop();
    }
}

void GameplayController::handleLeftClick(const GameplayInput& input) {
    if (!input.mouseInViewport) return;
    
    // Try to pick an entity
    Entity picked = pickEntityAt(input.mousePos);
    if (picked != INVALID_ENTITY) {
        selectEntity(picked);
    } else {
        // Click on ground - don't clear selection in RTS mode
        if (cameraMode_ == CameraMode::Free) {
            clearSelection();
        }
    }
}

void GameplayController::handleRightClick(const GameplayInput& input) {
    if (!input.mouseInViewport) return;
    if (!gameActive_) return;
    if (!world_) return;
    
    // Try to pick an entity first
    Entity picked = pickEntityAt(input.mousePos);
    
    if (picked != INVALID_ENTITY) {
        auto& reg = world_->getEntityManager().getRegistry();
        
        // Check if it's an enemy
        i32 myTeamId = 1;  // Default
        if (playerHero_ != INVALID_ENTITY && reg.valid(playerHero_) && 
            reg.all_of<HeroComponent>(playerHero_)) {
            myTeamId = reg.get<HeroComponent>(playerHero_).teamId;
        }
        
        i32 targetTeamId = 0;
        if (reg.all_of<HeroComponent>(picked)) {
            targetTeamId = reg.get<HeroComponent>(picked).teamId;
        } else if (reg.all_of<CreepComponent>(picked)) {
            targetTeamId = reg.get<CreepComponent>(picked).teamId;
        } else if (reg.all_of<ObjectComponent>(picked)) {
            targetTeamId = reg.get<ObjectComponent>(picked).teamId;
        }
        
        if (targetTeamId != 0 && targetTeamId != myTeamId) {
            // Attack enemy
            commandAttackTarget(picked);
            return;
        }
    }
    
    // No enemy picked - move to ground position
    Vec3 worldPos;
    if (screenToWorld(input.mousePos, worldPos)) {
        if (input.keys['A']) {
            // A + right-click = attack-move
            commandAttackMove(worldPos);
        } else {
            commandMoveTo(worldPos);
        }
    }
}

void GameplayController::handleAbilityKeys(const GameplayInput& input) {
    if (!gameActive_) return;
    
    // Q, W, E, R for abilities 0-3
    // D, F for abilities 4-5
    const int abilityKeys[] = { 'Q', 'W', 'E', 'R', 'D', 'F' };
    
    for (int i = 0; i < 6; i++) {
        int key = abilityKeys[i];
        if (input.keys[key] && !lastInput_.keys[key]) {
            // Get target position from mouse
            Vec3 targetPos;
            if (screenToWorld(input.mousePos, targetPos)) {
                Entity targetEntity = pickEntityAt(input.mousePos);
                commandCastAbility(i, targetPos, targetEntity);
            }
        }
    }
}

// ==================== World Queries ====================

bool GameplayController::screenToWorld(const Vec2& screenPos, Vec3& worldPos) const {
    if (!world_) return false;
    
    // Get terrain from world
    auto& reg = world_->getEntityManager().getRegistry();
    auto view = reg.view<TerrainComponent>();
    
    for (auto entity : view) {
        const auto& terrain = view.get<TerrainComponent>(entity);
        
        // Simple ground plane intersection at y=0 for now
        // TODO: Use proper terrain raycast
        
        // Get screen dimensions
        f32 screenW = lastInput_.viewportMax.x - lastInput_.viewportMin.x;
        f32 screenH = lastInput_.viewportMax.y - lastInput_.viewportMin.y;
        if (screenW < 1 || screenH < 1) {
            screenW = 1920;
            screenH = 1080;
        }
        
        // Normalized device coordinates
        f32 ndcX = ((screenPos.x - lastInput_.viewportMin.x) / screenW) * 2.0f - 1.0f;
        f32 ndcY = 1.0f - ((screenPos.y - lastInput_.viewportMin.y) / screenH) * 2.0f;
        
        // Get inverse view-projection
        Mat4 viewProj = camera_.getViewProjLH_ZO(screenW / screenH);
        Mat4 invViewProj = glm::inverse(viewProj);
        
        // Near and far points in world space
        Vec4 nearPoint = invViewProj * Vec4(ndcX, ndcY, 0.0f, 1.0f);
        Vec4 farPoint = invViewProj * Vec4(ndcX, ndcY, 1.0f, 1.0f);
        
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;
        
        // Ray direction
        Vec3 rayOrigin = Vec3(nearPoint);
        Vec3 rayDir = glm::normalize(Vec3(farPoint) - rayOrigin);
        
        // Intersect with y=0 plane
        if (std::abs(rayDir.y) > 0.0001f) {
            f32 t = -rayOrigin.y / rayDir.y;
            if (t > 0.0f) {
                worldPos = rayOrigin + rayDir * t;
                return true;
            }
        }
        
        break;
    }
    
    return false;
}

Entity GameplayController::pickEntityAt(const Vec2& screenPos) const {
    if (!world_) return INVALID_ENTITY;
    
    Vec3 worldPos;
    if (!screenToWorld(screenPos, worldPos)) {
        return INVALID_ENTITY;
    }
    
    // Find nearest entity to click position
    const f32 pickRadius = 5.0f;
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = pickRadius;
    
    auto& reg = world_->getEntityManager().getRegistry();
    
    // Check heroes
    auto heroView = reg.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        const auto& transform = heroView.get<TransformComponent>(entity);
        Vec3 diff = transform.position - worldPos;
        diff.y = 0;  // Ignore height
        f32 dist = glm::length(diff);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    // Check creeps
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.state == CreepState::Dead) continue;
        
        const auto& transform = creepView.get<TransformComponent>(entity);
        Vec3 diff = transform.position - worldPos;
        diff.y = 0;
        f32 dist = glm::length(diff);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    // Check towers/buildings
    auto objView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : objView) {
        const auto& obj = objView.get<ObjectComponent>(entity);
        if (obj.type != ObjectType::Tower && obj.type != ObjectType::Building && obj.type != ObjectType::Base) {
            continue;
        }
        
        const auto& transform = objView.get<TransformComponent>(entity);
        Vec3 diff = transform.position - worldPos;
        diff.y = 0;
        f32 dist = glm::length(diff);
        
        // Buildings have larger pick radius
        f32 radius = (obj.type == ObjectType::Tower) ? 3.0f : 8.0f;
        if (dist < radius && dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}

Entity GameplayController::findNearestCreep(const Vec3& position, i32 teamId) const {
    if (!world_) return INVALID_ENTITY;
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = std::numeric_limits<f32>::max();
    
    auto& reg = world_->getEntityManager().getRegistry();
    auto view = reg.view<CreepComponent, TransformComponent>();
    
    for (auto entity : view) {
        const auto& creep = view.get<CreepComponent>(entity);
        if (creep.state == CreepState::Dead) continue;
        if (teamId >= 0 && creep.teamId != teamId) continue;
        
        const auto& transform = view.get<TransformComponent>(entity);
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}

Entity GameplayController::findNearestHero(const Vec3& position, i32 teamId) const {
    if (!world_) return INVALID_ENTITY;
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = std::numeric_limits<f32>::max();
    
    auto& reg = world_->getEntityManager().getRegistry();
    auto view = reg.view<HeroComponent, TransformComponent>();
    
    for (auto entity : view) {
        const auto& hero = view.get<HeroComponent>(entity);
        if (hero.state == HeroState::Dead) continue;
        if (teamId >= 0 && hero.teamId != teamId) continue;
        
        const auto& transform = view.get<TransformComponent>(entity);
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}

Entity GameplayController::findNearestEnemy(const Vec3& position, i32 myTeamId) const {
    if (!world_) return INVALID_ENTITY;
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = std::numeric_limits<f32>::max();
    
    auto& reg = world_->getEntityManager().getRegistry();
    
    // Check enemy heroes
    auto heroView = reg.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        const auto& hero = heroView.get<HeroComponent>(entity);
        if (hero.state == HeroState::Dead) continue;
        if (hero.teamId == myTeamId) continue;
        
        const auto& transform = heroView.get<TransformComponent>(entity);
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    // Check enemy creeps
    auto creepView = reg.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.state == CreepState::Dead) continue;
        if (creep.teamId == myTeamId) continue;
        
        const auto& transform = creepView.get<TransformComponent>(entity);
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = entity;
        }
    }
    
    return nearest;
}

// ==================== Statistics ====================

void GameplayController::updateStats() {
    if (!world_) return;
    
    auto& reg = world_->getEntityManager().getRegistry();
    
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
            if (creep.teamId == 1) stats_.radiantCreeps++;
            else if (creep.teamId == 2) stats_.direCreeps++;
        }
    }
    
    // Count towers and buildings
    auto objView = reg.view<ObjectComponent>();
    for (auto entity : objView) {
        const auto& obj = objView.get<ObjectComponent>(entity);
        
        if (obj.type == ObjectType::Tower) {
            if (obj.teamId == 1) stats_.radiantTowers++;
            else if (obj.teamId == 2) stats_.direTowers++;
        } else if (obj.type == ObjectType::Building || obj.type == ObjectType::Base) {
            if (obj.teamId == 1) stats_.radiantBuildings++;
            else if (obj.teamId == 2) stats_.direBuildings++;
        }
    }
}

} // namespace WorldEditor
