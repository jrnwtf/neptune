#pragma once

// Textmode-specific configuration for crash prevention
namespace TextmodeConfig
{
    // Memory limits to prevent excessive allocations
    constexpr size_t MAX_TARGETS = 32;
    constexpr size_t MAX_HITBOXES = 16;
    constexpr size_t MAX_BACKTRACK_RECORDS = 8;
    constexpr size_t MAX_BONE_CALCULATIONS = 64;
    
    // Processing limits for stability
    constexpr int MAX_TRACES_PER_FRAME = 16;
    constexpr int MAX_ENTITY_CHECKS_PER_FRAME = 32;
    constexpr float MIN_FRAME_TIME = 0.016f; // ~60 FPS minimum
    
    // Safety thresholds
    constexpr float MAX_VALID_DISTANCE = 16384.0f;
    constexpr float MAX_VALID_ANGLE = 360.0f;
    constexpr int MAX_BONE_INDEX = 128;
    
    // Textmode-specific optimizations
    constexpr bool ENABLE_EXCEPTION_HANDLING = true;
    constexpr bool ENABLE_MEMORY_LIMITS = true;
    constexpr bool ENABLE_PROCESSING_LIMITS = true;
    constexpr bool ENABLE_ENTITY_VALIDATION = true;
    
    // Feature disabling for stability
    constexpr bool DISABLE_PROJECTILE_AIMBOT = true; // AimbotProjectile completely disabled in textmode
    
    // Performance monitoring
    constexpr float CRASH_PREVENTION_TIMEOUT = 5.0f; // Skip processing if taking too long
    constexpr int MAX_CONSECUTIVE_FAILURES = 3; // Max failures before disabling feature temporarily
}

#ifdef TEXTMODE
    // Force enable all safety features in textmode
    #define AIMBOT_SAFETY_ENABLED 1
    #define AIMBOT_MEMORY_LIMITS_ENABLED 1
    #define AIMBOT_EXCEPTION_HANDLING_ENABLED 1
#else
    // Optional in regular builds
    #define AIMBOT_SAFETY_ENABLED 0
    #define AIMBOT_MEMORY_LIMITS_ENABLED 0
    #define AIMBOT_EXCEPTION_HANDLING_ENABLED 0
#endif 