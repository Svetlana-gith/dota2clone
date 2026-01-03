#pragma once

#include "core/Types.h"
#include "NetworkTypes.h"

namespace WorldEditor {

// Input command types
enum class InputCommandType : u8 {
    None = 0,
    Move,           // Move to position
    AttackMove,     // Attack-move to position
    AttackTarget,   // Attack specific target
    CastAbility,    // Cast ability
    UseItem,        // Use item
    Stop,           // Stop current action
    Hold            // Hold position
};

// Ability/Item targeting
enum class TargetType : u8 {
    None = 0,
    Position,       // Ground target
    Unit,           // Unit target
    Direction       // Vector target
};

// Player input command (sent from client to server)
struct PlayerInput {
    SequenceNumber sequenceNumber = 0;
    TickNumber clientTick = 0;
    
    InputCommandType commandType = InputCommandType::None;
    
    // Movement
    Vec3 targetPosition = Vec3(0.0f);
    Vec3 moveDirection = Vec3(0.0f);
    
    // Combat
    NetworkId targetEntityId = INVALID_NETWORK_ID;
    
    // Abilities
    i32 abilityIndex = -1;
    TargetType abilityTargetType = TargetType::None;
    Vec3 abilityTargetPosition = Vec3(0.0f);
    NetworkId abilityTargetEntityId = INVALID_NETWORK_ID;
    
    // Items
    i32 itemSlot = -1;
    
    // Modifiers
    bool isShiftQueued = false;     // Queue command
    bool isAttackMove = false;      // Attack-move modifier
    
    // Timestamp for lag compensation
    f32 timestamp = 0.0f;
    
    PlayerInput() = default;
    
    // Helper constructors
    static PlayerInput createMoveCommand(SequenceNumber seq, const Vec3& pos) {
        PlayerInput input;
        input.sequenceNumber = seq;
        input.commandType = InputCommandType::Move;
        input.targetPosition = pos;
        return input;
    }
    
    static PlayerInput createAttackCommand(SequenceNumber seq, NetworkId targetId) {
        PlayerInput input;
        input.sequenceNumber = seq;
        input.commandType = InputCommandType::AttackTarget;
        input.targetEntityId = targetId;
        return input;
    }
    
    static PlayerInput createAbilityCommand(SequenceNumber seq, i32 abilityIdx, const Vec3& pos) {
        PlayerInput input;
        input.sequenceNumber = seq;
        input.commandType = InputCommandType::CastAbility;
        input.abilityIndex = abilityIdx;
        input.abilityTargetType = TargetType::Position;
        input.abilityTargetPosition = pos;
        return input;
    }
};

// Input buffer for client-side prediction
class InputBuffer {
public:
    void addInput(const PlayerInput& input) {
        inputs_.push_back(input);
        if (inputs_.size() > NetworkConfig::INPUT_BUFFER_SIZE) {
            inputs_.erase(inputs_.begin());
        }
    }
    
    void removeInputsUpTo(SequenceNumber seq) {
        inputs_.erase(
            std::remove_if(inputs_.begin(), inputs_.end(),
                [seq](const PlayerInput& input) {
                    return input.sequenceNumber <= seq;
                }),
            inputs_.end()
        );
    }
    
    const Vector<PlayerInput>& getInputs() const { return inputs_; }
    
    void clear() { inputs_.clear(); }
    
private:
    Vector<PlayerInput> inputs_;
};

} // namespace WorldEditor
