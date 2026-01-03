#pragma once

#include "System.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

// Animation types
enum class AnimationType : u8 {
    None = 0,
    Idle,
    Walk,
    Run,
    Attack,
    CastSpell,
    TakeDamage,
    Death,
    Victory
};

// Animation component
struct AnimationComponent {
    AnimationType currentAnimation = AnimationType::Idle;
    AnimationType queuedAnimation = AnimationType::None;
    
    f32 animationTime = 0.0f;
    f32 animationSpeed = 1.0f;
    f32 animationDuration = 1.0f;
    bool looping = true;
    bool playing = true;
    
    // Attack animation specifics
    f32 attackWindup = 0.3f;     // Time before damage is dealt
    f32 attackRecovery = 0.2f;   // Time after damage
    bool attackDamageDealt = false;
    
    // Visual feedback
    f32 scaleMultiplier = 1.0f;  // For squash/stretch
    Vec3 positionOffset = Vec3(0);
    f32 rotationOffset = 0.0f;
    
    AnimationComponent() = default;
};

class AnimationSystem : public System {
public:
    AnimationSystem(EntityManager& entityManager);
    ~AnimationSystem() override = default;
    
    void update(f32 deltaTime) override;
    String getName() const override { return "AnimationSystem"; }
    
    // Play animation
    void playAnimation(Entity entity, AnimationType type, bool loop = true);
    void playAttackAnimation(Entity entity, f32 windupTime, f32 recoveryTime);
    void stopAnimation(Entity entity);
    
    // Query
    bool isAnimationFinished(Entity entity) const;
    AnimationType getCurrentAnimation(Entity entity) const;
    
private:
    EntityManager& entityManager_;
    
    void updateAnimation(Entity entity, AnimationComponent& anim, f32 deltaTime);
    void applyAnimationEffects(Entity entity, AnimationComponent& anim);
    
    f32 getAnimationDuration(AnimationType type) const;
};

} // namespace WorldEditor
