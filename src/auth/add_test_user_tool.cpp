/**
 * Add Test User Tool
 * 
 * Simple utility to add a test user to auth.db
 * Username: testuser
 * Password: password123
 */

#include "auth/DatabaseManager.h"
#include "auth/SecurityManager.h"
#include "core/Types.h"
#include <iostream>

int main() {
    std::cout << "=== Add Test User Tool ===" << std::endl;
    std::cout << std::endl;
    
    // Initialize database
    auth::DatabaseManager db;
    if (!db.Initialize("auth.db")) {
        std::cerr << "ERROR: Failed to open auth.db" << std::endl;
        std::cerr << "Please run AuthServer.exe first to create the database." << std::endl;
        return 1;
    }
    
    std::cout << "Database opened successfully" << std::endl;
    
    auth::SecurityManager security;
    
    // Hash password - client sends SHA256(password), server stores bcrypt(SHA256(password))
    std::string passwordSHA256 = security.SHA256Hash("password123");
    std::string passwordHash = security.HashPassword(passwordSHA256, 10);
    
    if (passwordHash.empty()) {
        std::cerr << "ERROR: Failed to hash password" << std::endl;
        return 1;
    }
    
    std::cout << "Password SHA256: " << passwordSHA256.substr(0, 16) << "..." << std::endl;
    
    // Check if user already exists
    auth::Account existingAccount;
    if (db.GetAccountByUsername("testuser", existingAccount)) {
        std::cout << std::endl;
        std::cout << "Test user already exists - updating password hash..." << std::endl;
        
        // Update password with correct hash
        if (db.UpdatePassword(existingAccount.accountId, passwordHash)) {
            std::cout << "Password updated successfully!" << std::endl;
        } else {
            std::cerr << "ERROR: Failed to update password" << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "Login credentials:" << std::endl;
        std::cout << "  Username: testuser" << std::endl;
        std::cout << "  Password: password123" << std::endl;
        return 0;
    }
    
    // Create account
    u64 accountId = 0;
    if (!db.CreateAccount("testuser", passwordHash, accountId)) {
        std::cerr << "ERROR: Failed to create account" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Test user created successfully!" << std::endl;
    std::cout << "  Account ID: " << accountId << std::endl;
    std::cout << "  Username: testuser" << std::endl;
    std::cout << std::endl;
    std::cout << "Login credentials:" << std::endl;
    std::cout << "  Username: testuser" << std::endl;
    std::cout << "  Password: password123" << std::endl;
    std::cout << std::endl;
    
    return 0;
}
