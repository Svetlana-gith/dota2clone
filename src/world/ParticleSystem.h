#pragma once

#include "System.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

// Particle effect types
enum class ParticleEffectType : u8 {
    None = 0,
    Fireball,           // Fire projectile trail
    Explosion,          // Area damage explosion
    Heal,               // Green healing particles
    Stun,               // Stars around head
    Buff,               // Golden sparkles
    Debuff,             // Purple/dark particles
    LevelUp,            // Golden burst
    Death,              // Fade out particles
    Attack,             // Slash/hit effect
    TowerShot,          // Tower projectile trail
    Gold,               // Gold coin pickup
    Experience,         // Blue exp orbs
    CastSpell,          // Magic cast effect
    Projectile,         // Flying projectile trail
    AoEIndicator,       // Ground circle for AoE
    Lightning,          // Electric/lightning effect
    Ice,                // Frost/ice effect
    Fire,               // Fire damage effect
    Poison,             // Poison/DoT effect
    Shield,             // Defensive shield effect
    Aura                // Passive aura glow
};

// Single particle
struct Particle {
    Vec3 position = Vec3(0);
    Vec3 velocity = Vec3(0);
    Vec3 acceleration = Vec3(0, -9.8f, 0);  // Gravity
    Vec4 color = Vec4(1, 1, 1, 1);
    Vec4 colorEnd = Vec4(1, 1, 1, 0);       // Fade to
    f32 size = 0.5f;
    f32 sizeEnd = 0.0f;
    f32 lifetime = 1.0f;
    f32 age = 0.0f;
    f32 rotation = 0.0f;
    f32 rotationSpeed = 0.0f;
    bool alive = true;
};

// Particle emitter component
struct ParticleEmitterComponent {
    ParticleEffectType effectType = ParticleEffectType::None;
    
    // Emission settings
    f32 emissionRate = 10.0f;       // Particles per second
    f32 emissionTimer = 0.0f;
    i32 maxParticles = 100;
    i32 burstCount = 0;             // Instant burst (0 = continuous)
    bool loop = true;
    bool active = true;
    
    // Particle properties
    f32 particleLifetime = 1.0f;
    f32 particleLifetimeVariance = 0.2f;
    Vec3 emitDirection = Vec3(0, 1, 0);
    f32 emitAngle = 30.0f;          // Cone angle in degrees
    f32 emitSpeed = 5.0f;
    f32 emitSpeedVariance = 1.0f;
    f32 particleSize = 0.3f;
    f32 particleSizeVariance = 0.1f;
    Vec4 startColor = Vec4(1, 1, 1, 1);
    Vec4 endColor = Vec4(1, 1, 1, 0);
    bool useGravity = true;
    f32 gravityScale = 1.0f;
    
    // Duration (for non-looping effects)
    f32 duration = 0.0f;
    f32 elapsed = 0.0f;
    
    // Particles storage
    Vector<Particle> particles;
    
    ParticleEmitterComponent() = default;
};

class ParticleSystem : public System {
public:
    ParticleSystem(EntityManager& entityManager);
    ~ParticleSystem() override = default;
    
    void update(f32 deltaTime) override;
    String getName() const override { return "ParticleSystem"; }
    
    // Create particle effects
    Entity createEffect(ParticleEffectType type, const Vec3& position, f32 duration = 0.0f);
    Entity createEffectAttached(ParticleEffectType type, Entity parent);
    
    // Preset effects
    void spawnFireball(const Vec3& position);
    void spawnExplosion(const Vec3& position, f32 radius);
    void spawnHealEffect(const Vec3& position);
    void spawnStunEffect(Entity target);
    void spawnLevelUpEffect(const Vec3& position);
    void spawnDeathEffect(const Vec3& position);
    void spawnAttackEffect(const Vec3& position, const Vec3& direction);
    void spawnGoldPickup(const Vec3& position, i32 amount);
    
    // Ability visual effects
    void spawnProjectileTrail(const Vec3& start, const Vec3& end, const Vec4& color);
    void spawnAoEIndicator(const Vec3& position, f32 radius, const Vec4& color);
    void spawnLightningEffect(const Vec3& start, const Vec3& end);
    void spawnIceEffect(const Vec3& position);
    void spawnFireEffect(const Vec3& position);
    void spawnPoisonEffect(const Vec3& position);
    void spawnShieldEffect(Entity target);
    void spawnAuraEffect(Entity target, const Vec4& color);
    void spawnCastEffect(const Vec3& position, const Vec4& color);
    
private:
    EntityManager& entityManager_;
    
    void updateEmitter(Entity entity, ParticleEmitterComponent& emitter, f32 deltaTime);
    void emitParticle(ParticleEmitterComponent& emitter, const Vec3& position);
    void updateParticle(Particle& p, f32 deltaTime);
    
    // Setup presets
    void setupFireballEmitter(ParticleEmitterComponent& emitter);
    void setupExplosionEmitter(ParticleEmitterComponent& emitter);
    void setupHealEmitter(ParticleEmitterComponent& emitter);
    void setupStunEmitter(ParticleEmitterComponent& emitter);
    void setupLevelUpEmitter(ParticleEmitterComponent& emitter);
    void setupAttackEmitter(ParticleEmitterComponent& emitter);
    void setupCastSpellEmitter(ParticleEmitterComponent& emitter);
    void setupProjectileEmitter(ParticleEmitterComponent& emitter);
    void setupAoEIndicatorEmitter(ParticleEmitterComponent& emitter);
    void setupLightningEmitter(ParticleEmitterComponent& emitter);
    void setupIceEmitter(ParticleEmitterComponent& emitter);
    void setupFireEmitter(ParticleEmitterComponent& emitter);
    void setupPoisonEmitter(ParticleEmitterComponent& emitter);
    void setupShieldEmitter(ParticleEmitterComponent& emitter);
    void setupAuraEmitter(ParticleEmitterComponent& emitter);
};

} // namespace WorldEditor
