#include "AnimationSystem.h"
#include <cmath>

namespace WorldEditor {

AnimationSystem::AnimationSystem(EntityManager& entityManager)
    : entityManager_(entityManager) {}

void AnimationSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    
    auto view = registry.view<AnimationComponent, TransformComponent>();
    for (auto entity : view) {
        auto& anim = view.get<AnimationComponent>(entity);
        updateAnimation(entity, anim, deltaTime);
    }
}

void AnimationSystem::updateAnimation(Entity entity, AnimationComponent& anim, f32 deltaTime) {
    if (!anim.playing) return;
    
    anim.animationTime += deltaTime * anim.animationSpeed;
    
    // Check if animation finished
    if (anim.animationTime >= anim.animationDuration) {
        if (anim.looping) {
            anim.animationTime = std::fmod(anim.animationTime, anim.animationDuration);
        } else {
            anim.animationTime = anim.animationDuration;
            anim.playing = false;
            
            // Play queued animation
            if (anim.queuedAnimation != AnimationType::None) {
                playAnimation(entity, anim.queuedAnimation, true);
                anim.queuedAnimation = AnimationType::None;
            } else {
                // Return to idle
                playAnimation(entity, AnimationType::Idle, true);
            }
        }
    }
    
    // Apply visual effects based on animation
    applyAnimationEffects(entity, anim);
}

void AnimationSystem::applyAnimationEffects(Entity entity, AnimationComponent& anim) {
    f32 t = anim.animationTime / anim.animationDuration;
    
    switch (anim.currentAnimation) {
        case AnimationType::Idle:
            // Subtle breathing effect
            anim.scaleMultiplier = 1.0f + 0.02f * std::sin(anim.animationTime * 2.0f);
            anim.positionOffset = Vec3(0, 0.05f * std::sin(anim.animationTime * 2.0f), 0);
            break;
            
        case AnimationType::Walk:
        case AnimationType::Run:
            // Bobbing motion
            {
                f32 speed = (anim.currentAnimation == AnimationType::Run) ? 8.0f : 4.0f;
                f32 amplitude = (anim.currentAnimation == AnimationType::Run) ? 0.15f : 0.08f;
                anim.positionOffset = Vec3(0, amplitude * std::abs(std::sin(anim.animationTime * speed)), 0);
                anim.scaleMultiplier = 1.0f;
            }
            break;
            
        case AnimationType::Attack:
            // Windup -> Strike -> Recovery
            {
                f32 windupEnd = anim.attackWindup / anim.animationDuration;
                f32 strikeEnd = (anim.attackWindup + 0.1f) / anim.animationDuration;
                
                if (t < windupEnd) {
                    // Windup - pull back
                    f32 windupT = t / windupEnd;
                    anim.scaleMultiplier = 1.0f - 0.1f * windupT;
                    anim.positionOffset = Vec3(0, 0.2f * windupT, -0.3f * windupT);
                } else if (t < strikeEnd) {
                    // Strike - lunge forward
                    f32 strikeT = (t - windupEnd) / (strikeEnd - windupEnd);
                    anim.scaleMultiplier = 1.0f + 0.15f * (1.0f - strikeT);
                    anim.positionOffset = Vec3(0, -0.1f, 0.5f * (1.0f - strikeT));
                    
                    // Mark damage dealt at peak of strike
                    if (!anim.attackDamageDealt && strikeT > 0.5f) {
                        anim.attackDamageDealt = true;
                    }
                } else {
                    // Recovery
                    f32 recoveryT = (t - strikeEnd) / (1.0f - strikeEnd);
                    anim.scaleMultiplier = 1.0f;
                    anim.positionOffset = Vec3(0) * (1.0f - recoveryT);
                }
            }
            break;
            
        case AnimationType::CastSpell:
            // Raise arms, glow effect
            {
                f32 castT = std::sin(t * 3.14159f);
                anim.scaleMultiplier = 1.0f + 0.1f * castT;
                anim.positionOffset = Vec3(0, 0.3f * castT, 0);
            }
            break;
            
        case AnimationType::TakeDamage:
            // Flinch back
            {
                f32 flinchT = 1.0f - t;
                anim.positionOffset = Vec3(0, 0, -0.2f * flinchT);
                anim.scaleMultiplier = 1.0f - 0.1f * flinchT;
            }
            break;
            
        case AnimationType::Death:
            // Fall down
            {
                anim.scaleMultiplier = 1.0f - 0.3f * t;
                anim.positionOffset = Vec3(0, -1.0f * t * t, 0);
                anim.rotationOffset = 90.0f * t;  // Fall over
            }
            break;
            
        case AnimationType::Victory:
            // Jump and celebrate
            {
                f32 jumpT = std::sin(t * 3.14159f * 2.0f);
                anim.positionOffset = Vec3(0, 0.5f * std::max(0.0f, jumpT), 0);
                anim.scaleMultiplier = 1.0f + 0.1f * std::abs(jumpT);
            }
            break;
            
        default:
            anim.scaleMultiplier = 1.0f;
            anim.positionOffset = Vec3(0);
            anim.rotationOffset = 0.0f;
            break;
    }
}

void AnimationSystem::playAnimation(Entity entity, AnimationType type, bool loop) {
    if (!entityManager_.hasComponent<AnimationComponent>(entity)) {
        entityManager_.addComponent<AnimationComponent>(entity);
    }
    
    auto& anim = entityManager_.getComponent<AnimationComponent>(entity);
    anim.currentAnimation = type;
    anim.animationTime = 0.0f;
    anim.animationDuration = getAnimationDuration(type);
    anim.looping = loop;
    anim.playing = true;
    anim.attackDamageDealt = false;
}

void AnimationSystem::playAttackAnimation(Entity entity, f32 windupTime, f32 recoveryTime) {
    if (!entityManager_.hasComponent<AnimationComponent>(entity)) {
        entityManager_.addComponent<AnimationComponent>(entity);
    }
    
    auto& anim = entityManager_.getComponent<AnimationComponent>(entity);
    anim.currentAnimation = AnimationType::Attack;
    anim.animationTime = 0.0f;
    anim.attackWindup = windupTime;
    anim.attackRecovery = recoveryTime;
    anim.animationDuration = windupTime + 0.1f + recoveryTime;
    anim.looping = false;
    anim.playing = true;
    anim.attackDamageDealt = false;
}

void AnimationSystem::stopAnimation(Entity entity) {
    if (!entityManager_.hasComponent<AnimationComponent>(entity)) return;
    
    auto& anim = entityManager_.getComponent<AnimationComponent>(entity);
    anim.playing = false;
}

bool AnimationSystem::isAnimationFinished(Entity entity) const {
    if (!entityManager_.hasComponent<AnimationComponent>(entity)) return true;
    
    const auto& anim = entityManager_.getComponent<AnimationComponent>(entity);
    return !anim.playing || (anim.animationTime >= anim.animationDuration && !anim.looping);
}

AnimationType AnimationSystem::getCurrentAnimation(Entity entity) const {
    if (!entityManager_.hasComponent<AnimationComponent>(entity)) return AnimationType::None;
    
    return entityManager_.getComponent<AnimationComponent>(entity).currentAnimation;
}

f32 AnimationSystem::getAnimationDuration(AnimationType type) const {
    switch (type) {
        case AnimationType::Idle: return 2.0f;
        case AnimationType::Walk: return 0.8f;
        case AnimationType::Run: return 0.5f;
        case AnimationType::Attack: return 0.6f;
        case AnimationType::CastSpell: return 1.0f;
        case AnimationType::TakeDamage: return 0.3f;
        case AnimationType::Death: return 1.5f;
        case AnimationType::Victory: return 2.0f;
        default: return 1.0f;
    }
}

} // namespace WorldEditor
