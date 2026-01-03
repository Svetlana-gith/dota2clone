#include "auth/SecurityManager.h"
#include "bcrypt_hash.h"
#include <spdlog/spdlog.h>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>

// Windows Crypto API
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#include <openssl/rand.h>
#endif

namespace auth {

std::string SecurityManager::HashPassword(const std::string& password, int cost) {
    if (password.empty()) {
        spdlog::error("Cannot hash empty password");
        return "";
    }

    // Clamp cost between 4 and 31
    cost = std::max(4, std::min(31, cost));

    // Hash password
    char hash[61];
    if (bcrypt_hashpw(password.c_str(), cost, hash) != 0) {
        spdlog::error("Failed to hash password with bcrypt");
        return "";
    }

    return std::string(hash);
}

bool SecurityManager::VerifyPassword(const std::string& password, const std::string& hash) {
    if (password.empty() || hash.empty()) {
        return false;
    }

    int result = bcrypt_checkpw(password.c_str(), hash.c_str());
    return result == 0;
}

std::string SecurityManager::SHA256Hash(const std::string& data) {
    if (data.empty()) {
        return "";
    }

#ifdef _WIN32
    // Use Windows BCrypt API for SHA256
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status;
    DWORD hashLength = 0;
    DWORD resultLength = 0;

    // Open algorithm provider
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        spdlog::error("Failed to open SHA256 algorithm provider");
        return "";
    }

    // Get hash length
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLength, sizeof(DWORD), &resultLength, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    // Create hash
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    // Hash data
    status = BCryptHashData(hHash, (PBYTE)data.data(), (ULONG)data.size(), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    // Get hash value
    std::vector<uint8_t> hashValue(hashLength);
    status = BCryptFinishHash(hHash, hashValue.data(), hashLength, 0);
    
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        return "";
    }

    // Convert to hex string
    std::stringstream ss;
    for (uint8_t byte : hashValue) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }

    return ss.str();
#else
    // Use OpenSSL for other platforms
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
#endif
}

std::string SecurityManager::GenerateSecureToken(size_t length) {
#ifdef _WIN32
    // Use Windows BCrypt for cryptographically secure random
    std::vector<uint8_t> buffer(length);
    
    NTSTATUS status = BCryptGenRandom(nullptr, buffer.data(), (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        spdlog::error("Failed to generate secure random token");
        return "";
    }

    // Convert to hex string
    std::stringstream ss;
    for (uint8_t byte : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }

    return ss.str();
#else
    // Use OpenSSL for other platforms
    std::vector<unsigned char> buffer(length);
    if (RAND_bytes(buffer.data(), length) != 1) {
        spdlog::error("Failed to generate secure random token");
        return "";
    }

    std::stringstream ss;
    for (unsigned char byte : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
#endif
}

u64 SecurityManager::GenerateSecureRandom() {
#ifdef _WIN32
    u64 value = 0;
    NTSTATUS status = BCryptGenRandom(nullptr, (PBYTE)&value, sizeof(u64), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        spdlog::error("Failed to generate secure random number");
        return 0;
    }
    return value;
#else
    u64 value = 0;
    if (RAND_bytes((unsigned char*)&value, sizeof(u64)) != 1) {
        spdlog::error("Failed to generate secure random number");
        return 0;
    }
    return value;
#endif
}

std::string SecurityManager::MakeRateLimitKey(const std::string& ipAddress, RateLimitType type) {
    return ipAddress + ":" + std::to_string(static_cast<int>(type));
}

bool SecurityManager::CheckRateLimit(const std::string& ipAddress, RateLimitType type) {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    std::string key = MakeRateLimitKey(ipAddress, type);
    auto it = rateLimits_.find(key);
    
    if (it == rateLimits_.end()) {
        return false; // No rate limit state, not limited
    }

    u64 now = static_cast<u64>(std::time(nullptr));
    const auto& config = rateLimitConfigs_[type];
    
    // Check if window has expired
    if (now - it->second.windowStart > config.windowSeconds) {
        // Window expired, reset
        rateLimits_.erase(it);
        return false;
    }

    // Check if exceeded limit
    return it->second.attemptCount >= config.maxAttempts;
}

void SecurityManager::RecordAttempt(const std::string& ipAddress, RateLimitType type) {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    std::string key = MakeRateLimitKey(ipAddress, type);
    u64 now = static_cast<u64>(std::time(nullptr));
    
    auto it = rateLimits_.find(key);
    if (it == rateLimits_.end()) {
        // Create new rate limit state
        RateLimitState state;
        state.attemptCount = 1;
        state.windowStart = now;
        state.lastAttempt = now;
        rateLimits_[key] = state;
    } else {
        const auto& config = rateLimitConfigs_[type];
        
        // Check if window has expired
        if (now - it->second.windowStart > config.windowSeconds) {
            // Reset window
            it->second.attemptCount = 1;
            it->second.windowStart = now;
        } else {
            // Increment count
            it->second.attemptCount++;
        }
        it->second.lastAttempt = now;
    }
}

void SecurityManager::ResetRateLimit(const std::string& ipAddress, RateLimitType type) {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    std::string key = MakeRateLimitKey(ipAddress, type);
    rateLimits_.erase(key);
}

bool SecurityManager::IsBlacklisted(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(blacklistMutex_);
    
    auto it = ipBlacklist_.find(ipAddress);
    if (it == ipBlacklist_.end()) {
        return false;
    }

    // Check if temporary blacklist has expired
    if (it->second.expiresAt > 0) {
        u64 now = static_cast<u64>(std::time(nullptr));
        if (now >= it->second.expiresAt) {
            // Expired, remove from blacklist
            ipBlacklist_.erase(it);
            return false;
        }
    }

    return true;
}

void SecurityManager::AddToBlacklist(const std::string& ipAddress, u64 durationSeconds) {
    std::lock_guard<std::mutex> lock(blacklistMutex_);
    
    BlacklistEntry entry;
    if (durationSeconds > 0) {
        u64 now = static_cast<u64>(std::time(nullptr));
        entry.expiresAt = now + durationSeconds;
    } else {
        entry.expiresAt = 0; // Permanent
    }

    ipBlacklist_[ipAddress] = entry;
    spdlog::info("IP {} added to blacklist (duration: {}s)", ipAddress, durationSeconds);
}

void SecurityManager::RemoveFromBlacklist(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(blacklistMutex_);
    
    ipBlacklist_.erase(ipAddress);
    spdlog::info("IP {} removed from blacklist", ipAddress);
}

int SecurityManager::CleanupExpiredBlacklist() {
    std::lock_guard<std::mutex> lock(blacklistMutex_);
    
    u64 now = static_cast<u64>(std::time(nullptr));
    int count = 0;

    for (auto it = ipBlacklist_.begin(); it != ipBlacklist_.end();) {
        if (it->second.expiresAt > 0 && now >= it->second.expiresAt) {
            it = ipBlacklist_.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    if (count > 0) {
        spdlog::info("Cleaned up {} expired blacklist entries", count);
    }

    return count;
}

bool SecurityManager::IsSuspiciousActivity(u64 accountId, const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(activityMutex_);
    
    auto it = accountIpHistory_.find(accountId);
    if (it == accountIpHistory_.end()) {
        return false; // No history, not suspicious
    }

    const auto& ipHistory = it->second;
    
    // Check if this is a new IP address
    bool isNewIp = std::find(ipHistory.begin(), ipHistory.end(), ipAddress) == ipHistory.end();
    
    // If account has been accessed from many different IPs recently, it's suspicious
    if (isNewIp && ipHistory.size() >= 5) {
        spdlog::warn("Suspicious activity detected for account {}: too many different IPs", accountId);
        return true;
    }

    return false;
}

void SecurityManager::RecordLogin(u64 accountId, const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(activityMutex_);
    
    auto& ipHistory = accountIpHistory_[accountId];
    
    // Check if IP already exists
    auto it = std::find(ipHistory.begin(), ipHistory.end(), ipAddress);
    if (it == ipHistory.end()) {
        // Add new IP
        ipHistory.push_back(ipAddress);
        
        // Keep only last 10 IPs
        if (ipHistory.size() > 10) {
            ipHistory.erase(ipHistory.begin());
        }
    }
}

void SecurityManager::GetRateLimitConfig(RateLimitType type, int& outMaxAttempts, u64& outWindowSeconds) {
    const auto& config = rateLimitConfigs_[type];
    outMaxAttempts = config.maxAttempts;
    outWindowSeconds = config.windowSeconds;
}

} // namespace auth
