#include "ParticleSystem.h"
#include <random>
#include <algorithm>

namespace WorldEditor {

static std::mt19937 g_rng(std::random_device{}());

static f32 randomFloat(f32 min, f32 max) {
    std::uniform_real_distribution<f32> dist(min, max);
    return dist(g_rng);
}

static Vec3 randomInCone(const Vec3& direction, f32 angleDegrees) {
    f32 angleRad = glm::radians(angleDegrees);
    f32 cosAngle = std::cos(angleRad);
    
    // Random point on unit sphere cap
    f32 z = randomFloat(cosAngle, 1.0f);
    f32 phi = randomFloat(0.0f, 2.0f * 3.14159f);
    f32 sinTheta = std::sqrt(1.0f - z * z);
    
    Vec3 localDir(sinTheta * std::cos(phi), sinTheta * std::sin(phi), z);
    
    // Rotate to align with direction
    Vec3 up = std::abs(direction.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 right = glm::normalize(glm::cross(up, direction));
    up = glm::cross(direction, right);
    
    return right * localDir.x + up * localDir.y + direction * localDir.z;
}

ParticleSystem::ParticleSystem(EntityManager& entityManager)
    : entityManager_(entityManager) {}

void ParticleSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    
    Vector<Entity> toRemove;
    
    auto view = registry.view<ParticleEmitterComponent, TransformComponent>();
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitterComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        updateEmitter(entity, emitter, deltaTime);
        
        // Remove finished non-looping effects
        if (!emitter.loop && emitter.elapsed >= emitter.duration && emitter.particles.empty()) {
            toRemove.push_back(entity);
        }
    }
    
    for (auto e : toRemove) {
        entityManager_.destroyEntity(e);
    }
}

void ParticleSystem::updateEmitter(Entity entity, ParticleEmitterComponent& emitter, f32 deltaTime) {
    if (!emitter.active) return;
    
    auto& transform = entityManager_.getComponent<TransformComponent>(entity);
    
    // Update elapsed time
    emitter.elapsed += deltaTime;
    
    // Emit new particles
    if (emitter.loop || emitter.elapsed < emitter.duration) {
        if (emitter.burstCount > 0 && emitter.emissionTimer == 0) {
            // Burst emission
            for (i32 i = 0; i < emitter.burstCount; i++) {
                if ((i32)emitter.particles.size() < emitter.maxParticles) {
                    emitParticle(emitter, transform.position);
                }
            }
            emitter.emissionTimer = 1.0f; // Prevent re-burst
        } else if (emitter.burstCount == 0) {
            // Continuous emission
            emitter.emissionTimer += deltaTime;
            f32 interval = 1.0f / emitter.emissionRate;
            
            while (emitter.emissionTimer >= interval) {
                emitter.emissionTimer -= interval;
                if ((i32)emitter.particles.size() < emitter.maxParticles) {
                    emitParticle(emitter, transform.position);
                }
            }
        }
    }
    
    // Update existing particles
    for (auto& p : emitter.particles) {
        if (p.alive) {
            updateParticle(p, deltaTime);
        }
    }
    
    // Remove dead particles
    emitter.particles.erase(
        std::remove_if(emitter.particles.begin(), emitter.particles.end(),
            [](const Particle& p) { return !p.alive; }),
        emitter.particles.end());
}

void ParticleSystem::emitParticle(ParticleEmitterComponent& emitter, const Vec3& position) {
    Particle p;
    p.position = position;
    
    // Random direction in cone
    Vec3 dir = randomInCone(emitter.emitDirection, emitter.emitAngle);
    f32 speed = emitter.emitSpeed + randomFloat(-emitter.emitSpeedVariance, emitter.emitSpeedVariance);
    p.velocity = dir * speed;
    
    // Gravity
    p.acceleration = emitter.useGravity ? Vec3(0, -9.8f * emitter.gravityScale, 0) : Vec3(0);
    
    // Lifetime
    p.lifetime = emitter.particleLifetime + randomFloat(-emitter.particleLifetimeVariance, emitter.particleLifetimeVariance);
    p.age = 0;
    
    // Size
    p.size = emitter.particleSize + randomFloat(-emitter.particleSizeVariance, emitter.particleSizeVariance);
    p.sizeEnd = p.size * 0.1f;
    
    // Color
    p.color = emitter.startColor;
    p.colorEnd = emitter.endColor;
    
    // Rotation
    p.rotation = randomFloat(0, 360);
    p.rotationSpeed = randomFloat(-180, 180);
    
    p.alive = true;
    
    emitter.particles.push_back(p);
}

void ParticleSystem::updateParticle(Particle& p, f32 deltaTime) {
    p.age += deltaTime;
    
    if (p.age >= p.lifetime) {
        p.alive = false;
        return;
    }
    
    // Physics
    p.velocity += p.acceleration * deltaTime;
    p.position += p.velocity * deltaTime;
    p.rotation += p.rotationSpeed * deltaTime;
    
    // Interpolate properties
    f32 t = p.age / p.lifetime;
    p.color = glm::mix(p.color, p.colorEnd, t);
    p.size = glm::mix(p.size, p.sizeEnd, t);
}

Entity ParticleSystem::createEffect(ParticleEffectType type, const Vec3& position, f32 duration) {
    Entity entity = entityManager_.createEntity("ParticleEffect");
    auto& transform = entityManager_.addComponent<TransformComponent>(entity);
    transform.position = position;
    
    auto& emitter = entityManager_.addComponent<ParticleEmitterComponent>(entity);
    emitter.effectType = type;
    emitter.duration = duration > 0 ? duration : 2.0f;
    emitter.loop = (duration <= 0);
    
    switch (type) {
        case ParticleEffectType::Fireball: setupFireballEmitter(emitter); break;
        case ParticleEffectType::Explosion: setupExplosionEmitter(emitter); break;
        case ParticleEffectType::Heal: setupHealEmitter(emitter); break;
        case ParticleEffectType::Stun: setupStunEmitter(emitter); break;
        case ParticleEffectType::LevelUp: setupLevelUpEmitter(emitter); break;
        case ParticleEffectType::Attack: setupAttackEmitter(emitter); break;
        case ParticleEffectType::CastSpell: setupCastSpellEmitter(emitter); break;
        case ParticleEffectType::Projectile: setupProjectileEmitter(emitter); break;
        case ParticleEffectType::AoEIndicator: setupAoEIndicatorEmitter(emitter); break;
        case ParticleEffectType::Lightning: setupLightningEmitter(emitter); break;
        case ParticleEffectType::Ice: setupIceEmitter(emitter); break;
        case ParticleEffectType::Fire: setupFireEmitter(emitter); break;
        case ParticleEffectType::Poison: setupPoisonEmitter(emitter); break;
        case ParticleEffectType::Shield: setupShieldEmitter(emitter); break;
        case ParticleEffectType::Aura: setupAuraEmitter(emitter); break;
        default: break;
    }
    
    return entity;
}

Entity ParticleSystem::createEffectAttached(ParticleEffectType type, Entity parent) {
    if (!entityManager_.hasComponent<TransformComponent>(parent)) {
        return INVALID_ENTITY;
    }
    
    auto& parentTransform = entityManager_.getComponent<TransformComponent>(parent);
    return createEffect(type, parentTransform.position, 0);
}

void ParticleSystem::spawnFireball(const Vec3& position) {
    createEffect(ParticleEffectType::Fireball, position, 0.5f);
}

void ParticleSystem::spawnExplosion(const Vec3& position, f32 radius) {
    auto entity = createEffect(ParticleEffectType::Explosion, position, 1.0f);
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.emitSpeed = radius * 2.0f;
        emitter.burstCount = (i32)(radius * 20);
    }
}

void ParticleSystem::spawnHealEffect(const Vec3& position) {
    createEffect(ParticleEffectType::Heal, position, 1.5f);
}

void ParticleSystem::spawnStunEffect(Entity target) {
    createEffectAttached(ParticleEffectType::Stun, target);
}

void ParticleSystem::spawnLevelUpEffect(const Vec3& position) {
    createEffect(ParticleEffectType::LevelUp, position, 2.0f);
}

void ParticleSystem::spawnDeathEffect(const Vec3& position) {
    createEffect(ParticleEffectType::Death, position, 1.0f);
}

void ParticleSystem::spawnAttackEffect(const Vec3& position, const Vec3& direction) {
    auto entity = createEffect(ParticleEffectType::Attack, position, 0.3f);
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.emitDirection = glm::normalize(direction);
    }
}

void ParticleSystem::spawnGoldPickup(const Vec3& position, i32 amount) {
    auto entity = createEffect(ParticleEffectType::Gold, position, 1.0f);
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.burstCount = std::min(amount / 10 + 1, 20);
    }
}

// Preset configurations
void ParticleSystem::setupFireballEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 50.0f;
    emitter.particleLifetime = 0.5f;
    emitter.emitDirection = Vec3(0, 0, -1);
    emitter.emitAngle = 20.0f;
    emitter.emitSpeed = 2.0f;
    emitter.particleSize = 0.3f;
    emitter.startColor = Vec4(1.0f, 0.6f, 0.1f, 1.0f);  // Orange
    emitter.endColor = Vec4(1.0f, 0.2f, 0.0f, 0.0f);    // Red fade
    emitter.useGravity = false;
}

void ParticleSystem::setupExplosionEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 50;
    emitter.loop = false;
    emitter.particleLifetime = 0.8f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 180.0f;  // Sphere
    emitter.emitSpeed = 8.0f;
    emitter.particleSize = 0.4f;
    emitter.startColor = Vec4(1.0f, 0.8f, 0.2f, 1.0f);  // Yellow
    emitter.endColor = Vec4(0.5f, 0.1f, 0.0f, 0.0f);    // Dark red fade
    emitter.useGravity = true;
    emitter.gravityScale = 0.5f;
}

void ParticleSystem::setupHealEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 20.0f;
    emitter.particleLifetime = 1.0f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 30.0f;
    emitter.emitSpeed = 3.0f;
    emitter.particleSize = 0.2f;
    emitter.startColor = Vec4(0.2f, 1.0f, 0.3f, 1.0f);  // Green
    emitter.endColor = Vec4(0.5f, 1.0f, 0.5f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupStunEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 5.0f;
    emitter.particleLifetime = 0.5f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 360.0f;
    emitter.emitSpeed = 1.0f;
    emitter.particleSize = 0.3f;
    emitter.startColor = Vec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow stars
    emitter.endColor = Vec4(1.0f, 1.0f, 0.5f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupLevelUpEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 30;
    emitter.loop = false;
    emitter.particleLifetime = 1.5f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 45.0f;
    emitter.emitSpeed = 6.0f;
    emitter.particleSize = 0.25f;
    emitter.startColor = Vec4(1.0f, 0.85f, 0.0f, 1.0f);  // Gold
    emitter.endColor = Vec4(1.0f, 1.0f, 0.5f, 0.0f);
    emitter.useGravity = true;
    emitter.gravityScale = 0.3f;
}

void ParticleSystem::setupAttackEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 10;
    emitter.loop = false;
    emitter.particleLifetime = 0.2f;
    emitter.emitAngle = 30.0f;
    emitter.emitSpeed = 5.0f;
    emitter.particleSize = 0.15f;
    emitter.startColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
    emitter.endColor = Vec4(0.8f, 0.8f, 0.8f, 0.0f);
    emitter.useGravity = false;
}

// ============ New Ability Effect Emitters ============

void ParticleSystem::setupCastSpellEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 25;
    emitter.loop = false;
    emitter.particleLifetime = 0.6f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 60.0f;
    emitter.emitSpeed = 4.0f;
    emitter.particleSize = 0.2f;
    emitter.startColor = Vec4(0.4f, 0.6f, 1.0f, 1.0f);  // Blue magic
    emitter.endColor = Vec4(0.8f, 0.9f, 1.0f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupProjectileEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 40.0f;
    emitter.particleLifetime = 0.3f;
    emitter.emitDirection = Vec3(0, 0, -1);
    emitter.emitAngle = 15.0f;
    emitter.emitSpeed = 1.0f;
    emitter.particleSize = 0.15f;
    emitter.startColor = Vec4(1.0f, 0.8f, 0.3f, 1.0f);  // Golden trail
    emitter.endColor = Vec4(1.0f, 0.5f, 0.1f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupAoEIndicatorEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 30.0f;
    emitter.particleLifetime = 0.5f;
    emitter.emitDirection = Vec3(0, 0.2f, 0);
    emitter.emitAngle = 180.0f;  // Circle on ground
    emitter.emitSpeed = 0.5f;
    emitter.particleSize = 0.1f;
    emitter.startColor = Vec4(1.0f, 0.3f, 0.3f, 0.8f);  // Red warning
    emitter.endColor = Vec4(1.0f, 0.1f, 0.1f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupLightningEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 15;
    emitter.loop = false;
    emitter.particleLifetime = 0.15f;
    emitter.emitDirection = Vec3(0, -1, 0);
    emitter.emitAngle = 10.0f;
    emitter.emitSpeed = 20.0f;
    emitter.particleSize = 0.1f;
    emitter.startColor = Vec4(0.7f, 0.8f, 1.0f, 1.0f);  // Electric blue
    emitter.endColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupIceEmitter(ParticleEmitterComponent& emitter) {
    emitter.burstCount = 20;
    emitter.loop = false;
    emitter.particleLifetime = 1.0f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 45.0f;
    emitter.emitSpeed = 3.0f;
    emitter.particleSize = 0.2f;
    emitter.startColor = Vec4(0.6f, 0.9f, 1.0f, 1.0f);  // Ice blue
    emitter.endColor = Vec4(0.9f, 0.95f, 1.0f, 0.0f);
    emitter.useGravity = true;
    emitter.gravityScale = 0.3f;
}

void ParticleSystem::setupFireEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 35.0f;
    emitter.particleLifetime = 0.6f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 25.0f;
    emitter.emitSpeed = 4.0f;
    emitter.particleSize = 0.25f;
    emitter.startColor = Vec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange fire
    emitter.endColor = Vec4(1.0f, 0.1f, 0.0f, 0.0f);    // Red fade
    emitter.useGravity = false;
}

void ParticleSystem::setupPoisonEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 15.0f;
    emitter.particleLifetime = 1.2f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 40.0f;
    emitter.emitSpeed = 1.5f;
    emitter.particleSize = 0.18f;
    emitter.startColor = Vec4(0.3f, 0.8f, 0.2f, 0.9f);  // Toxic green
    emitter.endColor = Vec4(0.1f, 0.4f, 0.1f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupShieldEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 20.0f;
    emitter.particleLifetime = 0.8f;
    emitter.emitDirection = Vec3(0, 0, 1);
    emitter.emitAngle = 180.0f;  // Sphere around target
    emitter.emitSpeed = 0.3f;
    emitter.particleSize = 0.12f;
    emitter.startColor = Vec4(0.3f, 0.5f, 1.0f, 0.7f);  // Blue shield
    emitter.endColor = Vec4(0.6f, 0.8f, 1.0f, 0.0f);
    emitter.useGravity = false;
}

void ParticleSystem::setupAuraEmitter(ParticleEmitterComponent& emitter) {
    emitter.emissionRate = 10.0f;
    emitter.particleLifetime = 1.5f;
    emitter.emitDirection = Vec3(0, 1, 0);
    emitter.emitAngle = 360.0f;  // Ring around feet
    emitter.emitSpeed = 0.5f;
    emitter.particleSize = 0.1f;
    emitter.startColor = Vec4(1.0f, 0.9f, 0.5f, 0.6f);  // Golden glow
    emitter.endColor = Vec4(1.0f, 0.8f, 0.3f, 0.0f);
    emitter.useGravity = false;
}

// ============ New Ability Effect Spawners ============

void ParticleSystem::spawnProjectileTrail(const Vec3& start, const Vec3& end, const Vec4& color) {
    Vec3 direction = glm::normalize(end - start);
    f32 distance = glm::length(end - start);
    
    // Create trail particles along the path
    i32 numParticles = static_cast<i32>(distance * 3);
    for (i32 i = 0; i < numParticles; i++) {
        f32 t = static_cast<f32>(i) / numParticles;
        Vec3 pos = glm::mix(start, end, t);
        
        auto entity = createEffect(ParticleEffectType::Projectile, pos, 0.3f);
        if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
            auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
            emitter.emitDirection = direction;
            emitter.startColor = color;
            emitter.endColor = Vec4(color.r, color.g, color.b, 0.0f);
            emitter.burstCount = 3;
            emitter.emissionRate = 0;
        }
    }
}

void ParticleSystem::spawnAoEIndicator(const Vec3& position, f32 radius, const Vec4& color) {
    // Create ring of particles on ground
    i32 numPoints = static_cast<i32>(radius * 8);
    for (i32 i = 0; i < numPoints; i++) {
        f32 angle = (2.0f * 3.14159f * i) / numPoints;
        Vec3 offset(std::cos(angle) * radius, 0.1f, std::sin(angle) * radius);
        
        auto entity = createEffect(ParticleEffectType::AoEIndicator, position + offset, 0.5f);
        if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
            auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
            emitter.startColor = color;
            emitter.endColor = Vec4(color.r, color.g, color.b, 0.0f);
            emitter.burstCount = 5;
        }
    }
}

void ParticleSystem::spawnLightningEffect(const Vec3& start, const Vec3& end) {
    Vec3 direction = glm::normalize(end - start);
    f32 distance = glm::length(end - start);
    
    // Create lightning bolt segments
    Vec3 current = start;
    i32 segments = static_cast<i32>(distance * 2) + 1;
    
    for (i32 i = 0; i < segments; i++) {
        f32 t = static_cast<f32>(i + 1) / segments;
        Vec3 target = glm::mix(start, end, t);
        
        // Add random offset for jagged look
        if (i < segments - 1) {
            target += Vec3(randomFloat(-0.3f, 0.3f), randomFloat(-0.3f, 0.3f), randomFloat(-0.3f, 0.3f));
        }
        
        auto entity = createEffect(ParticleEffectType::Lightning, current, 0.2f);
        if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
            auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
            emitter.emitDirection = glm::normalize(target - current);
            emitter.burstCount = 8;
        }
        
        current = target;
    }
}

void ParticleSystem::spawnIceEffect(const Vec3& position) {
    auto entity = createEffect(ParticleEffectType::Ice, position, 1.0f);
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.burstCount = 30;
    }
}

void ParticleSystem::spawnFireEffect(const Vec3& position) {
    createEffect(ParticleEffectType::Fire, position, 1.5f);
}

void ParticleSystem::spawnPoisonEffect(const Vec3& position) {
    createEffect(ParticleEffectType::Poison, position, 2.0f);
}

void ParticleSystem::spawnShieldEffect(Entity target) {
    if (!entityManager_.hasComponent<TransformComponent>(target)) return;
    
    auto& transform = entityManager_.getComponent<TransformComponent>(target);
    createEffect(ParticleEffectType::Shield, transform.position + Vec3(0, 1, 0), 0);  // Looping
}

void ParticleSystem::spawnAuraEffect(Entity target, const Vec4& color) {
    if (!entityManager_.hasComponent<TransformComponent>(target)) return;
    
    auto& transform = entityManager_.getComponent<TransformComponent>(target);
    auto entity = createEffect(ParticleEffectType::Aura, transform.position, 0);  // Looping
    
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.startColor = color;
        emitter.endColor = Vec4(color.r, color.g, color.b, 0.0f);
    }
}

void ParticleSystem::spawnCastEffect(const Vec3& position, const Vec4& color) {
    auto entity = createEffect(ParticleEffectType::CastSpell, position, 0.6f);
    
    if (entityManager_.hasComponent<ParticleEmitterComponent>(entity)) {
        auto& emitter = entityManager_.getComponent<ParticleEmitterComponent>(entity);
        emitter.startColor = color;
        emitter.endColor = Vec4(color.r * 1.2f, color.g * 1.2f, color.b * 1.2f, 0.0f);
    }
}


} // namespace WorldEditor
