/**
 * Add MM Test Users Tool
 * Creates test1/test1 and test2/test2 accounts for matchmaking testing
 */

#include "auth/DatabaseManager.h"
#include "auth/SecurityManager.h"
#include "core/Types.h"
#include <iostream>

void CreateUser(auth::DatabaseManager& db, auth::SecurityManager& security, 
                const std::string& username, const std::string& password) {
    std::cout << "Creating user: " << username << std::endl;
    
    // Hash password - client sends SHA256(password), server stores bcrypt(SHA256(password))
    std::string passwordSHA256 = security.SHA256Hash(password);
    std::string passwordHash = security.HashPassword(passwordSHA256, 10);
    
    if (passwordHash.empty()) {
        std::cerr << "ERROR: Failed to hash password for " << username << std::endl;
        return;
    }
    
    // Check if user already exists
    auth::Account existingAccount;
    if (db.GetAccountByUsername(username, existingAccount)) {
        std::cout << "  User exists - updating password..." << std::endl;
        if (db.UpdatePassword(existingAccount.accountId, passwordHash)) {
            std::cout << "  Password updated!" << std::endl;
        }
        return;
    }
    
    // Create account
    u64 accountId = 0;
    if (db.CreateAccount(username, passwordHash, accountId)) {
        std::cout << "  Created! Account ID: " << accountId << std::endl;
    } else {
        std::cerr << "  ERROR: Failed to create account" << std::endl;
    }
}

int main() {
    std::cout << "=== Add MM Test Users ===" << std::endl;
    
    auth::DatabaseManager db;
    if (!db.Initialize("auth.db")) {
        std::cerr << "ERROR: Failed to open auth.db" << std::endl;
        return 1;
    }
    
    auth::SecurityManager security;
    
    // Create test1/test1
    CreateUser(db, security, "test1", "test1");
    
    // Create test2/test2
    CreateUser(db, security, "test2", "test2");
    
    std::cout << std::endl;
    std::cout << "Done! Test accounts:" << std::endl;
    std::cout << "  test1 / test1" << std::endl;
    std::cout << "  test2 / test2" << std::endl;
    
    return 0;
}
