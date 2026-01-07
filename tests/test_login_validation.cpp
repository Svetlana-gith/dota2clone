/**
 * Login/Register Form Validation Tests
 * 
 * Tests for validation logic used in Login Redesign feature.
 * These tests verify the correctness properties defined in design.md:
 * - Property 1: Empty Input Validation
 * - Property 2: Password Mismatch Detection
 * - Property 3: Username Length Validation
 * 
 * Feature: login-redesign
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <cctype>
#include <random>

// Validation functions extracted from LoginForm/RegisterForm for testing
namespace LoginValidation {

/**
 * Check if string is empty or contains only whitespace
 */
bool IsEmptyOrWhitespace(const std::string& str) {
    if (str.empty()) return true;
    for (char c : str) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

/**
 * Validate username length (3-20 characters)
 */
bool IsValidUsernameLength(const std::string& username) {
    return username.length() >= 3 && username.length() <= 20;
}

/**
 * Validate password length (minimum 8 characters)
 */
bool IsValidPasswordLength(const std::string& password) {
    return password.length() >= 8;
}

/**
 * Check if passwords match
 */
bool PasswordsMatch(const std::string& password, const std::string& confirmPassword) {
    return password == confirmPassword;
}

/**
 * Full login form validation
 * Returns error message or empty string if valid
 */
std::string ValidateLoginForm(const std::string& username, const std::string& password) {
    if (IsEmptyOrWhitespace(username)) {
        return "Please enter username";
    }
    if (IsEmptyOrWhitespace(password)) {
        return "Please enter password";
    }
    return "";
}

/**
 * Full registration form validation
 * Returns error message or empty string if valid
 */
std::string ValidateRegisterForm(const std::string& username, 
                                  const std::string& password, 
                                  const std::string& confirmPassword) {
    if (IsEmptyOrWhitespace(username)) {
        return "Please enter username";
    }
    if (!IsValidUsernameLength(username)) {
        return "Username must be 3-20 characters";
    }
    if (IsEmptyOrWhitespace(password)) {
        return "Please enter password";
    }
    if (!IsValidPasswordLength(password)) {
        return "Password must be at least 8 characters";
    }
    if (!PasswordsMatch(password, confirmPassword)) {
        return "Passwords do not match";
    }
    return "";
}

} // namespace LoginValidation

// ============================================================================
// Unit Tests
// ============================================================================

TEST_CASE("LoginForm - Empty username validation", "[login][validation]") {
    using namespace LoginValidation;
    
    SECTION("Empty string shows error") {
        auto error = ValidateLoginForm("", "password123");
        REQUIRE(error == "Please enter username");
    }
    
    SECTION("Whitespace-only username shows error") {
        auto error = ValidateLoginForm("   ", "password123");
        REQUIRE(error == "Please enter username");
    }
    
    SECTION("Tab-only username shows error") {
        auto error = ValidateLoginForm("\t\t", "password123");
        REQUIRE(error == "Please enter username");
    }
    
    SECTION("Valid username passes") {
        auto error = ValidateLoginForm("validuser", "password123");
        REQUIRE(error.empty());
    }
}

TEST_CASE("LoginForm - Empty password validation", "[login][validation]") {
    using namespace LoginValidation;
    
    SECTION("Empty password shows error") {
        auto error = ValidateLoginForm("validuser", "");
        REQUIRE(error == "Please enter password");
    }
    
    SECTION("Whitespace-only password shows error") {
        auto error = ValidateLoginForm("validuser", "   ");
        REQUIRE(error == "Please enter password");
    }
}

TEST_CASE("RegisterForm - Username length validation", "[register][validation]") {
    using namespace LoginValidation;
    
    SECTION("Username too short (2 chars)") {
        auto error = ValidateRegisterForm("ab", "password123", "password123");
        REQUIRE(error == "Username must be 3-20 characters");
    }
    
    SECTION("Username too short (1 char)") {
        auto error = ValidateRegisterForm("a", "password123", "password123");
        REQUIRE(error == "Username must be 3-20 characters");
    }
    
    SECTION("Username too long (21 chars)") {
        auto error = ValidateRegisterForm("abcdefghijklmnopqrstu", "password123", "password123");
        REQUIRE(error == "Username must be 3-20 characters");
    }
    
    SECTION("Username minimum valid (3 chars)") {
        auto error = ValidateRegisterForm("abc", "password123", "password123");
        REQUIRE(error.empty());
    }
    
    SECTION("Username maximum valid (20 chars)") {
        auto error = ValidateRegisterForm("abcdefghijklmnopqrst", "password123", "password123");
        REQUIRE(error.empty());
    }
}

TEST_CASE("RegisterForm - Password length validation", "[register][validation]") {
    using namespace LoginValidation;
    
    SECTION("Password too short (7 chars)") {
        auto error = ValidateRegisterForm("validuser", "1234567", "1234567");
        REQUIRE(error == "Password must be at least 8 characters");
    }
    
    SECTION("Password minimum valid (8 chars)") {
        auto error = ValidateRegisterForm("validuser", "12345678", "12345678");
        REQUIRE(error.empty());
    }
}

TEST_CASE("RegisterForm - Password mismatch validation", "[register][validation]") {
    using namespace LoginValidation;
    
    SECTION("Different passwords show error") {
        auto error = ValidateRegisterForm("validuser", "password123", "password456");
        REQUIRE(error == "Passwords do not match");
    }
    
    SECTION("Matching passwords pass") {
        auto error = ValidateRegisterForm("validuser", "password123", "password123");
        REQUIRE(error.empty());
    }
    
    SECTION("Case-sensitive mismatch") {
        auto error = ValidateRegisterForm("validuser", "Password123", "password123");
        REQUIRE(error == "Passwords do not match");
    }
}

// ============================================================================
// Property-Based Tests
// ============================================================================

// Helper to generate random whitespace strings
std::string GenerateWhitespaceString(size_t length, std::mt19937& rng) {
    const char whitespace[] = " \t\n\r";
    std::uniform_int_distribution<size_t> dist(0, sizeof(whitespace) - 2);
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += whitespace[dist(rng)];
    }
    return result;
}

// Helper to generate random alphanumeric strings
std::string GenerateAlphanumericString(size_t length, std::mt19937& rng) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<size_t> dist(0, sizeof(chars) - 2);
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dist(rng)];
    }
    return result;
}

TEST_CASE("Property 1: Empty Input Validation", "[login][property]") {
    /**
     * Feature: login-redesign, Property 1: Empty Input Validation
     * 
     * For any input string that is empty or contains only whitespace characters,
     * submitting the login form SHALL display an appropriate error message
     * and NOT attempt authentication.
     * 
     * Validates: Requirements 3.4, 3.5
     */
    using namespace LoginValidation;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> lengthDist(0, 20);
    
    // Run 100 iterations as specified in design
    for (int i = 0; i < 100; ++i) {
        size_t length = lengthDist(rng);
        std::string whitespaceUsername = GenerateWhitespaceString(length, rng);
        std::string whitespacePassword = GenerateWhitespaceString(length, rng);
        std::string validPassword = "validpassword123";
        std::string validUsername = "validuser";
        
        // Whitespace username should fail
        auto error1 = ValidateLoginForm(whitespaceUsername, validPassword);
        REQUIRE_FALSE(error1.empty());
        REQUIRE(error1 == "Please enter username");
        
        // Whitespace password should fail
        auto error2 = ValidateLoginForm(validUsername, whitespacePassword);
        REQUIRE_FALSE(error2.empty());
        REQUIRE(error2 == "Please enter password");
    }
}

TEST_CASE("Property 2: Password Mismatch Detection", "[register][property]") {
    /**
     * Feature: login-redesign, Property 2: Password Mismatch Detection
     * 
     * For any two password strings where password != confirmPassword,
     * submitting the registration form SHALL display "Passwords do not match"
     * error and NOT attempt registration.
     * 
     * Validates: Requirements 4.4
     */
    using namespace LoginValidation;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> lengthDist(8, 30);
    
    // Run 100 iterations as specified in design
    for (int i = 0; i < 100; ++i) {
        size_t len1 = lengthDist(rng);
        size_t len2 = lengthDist(rng);
        
        std::string password1 = GenerateAlphanumericString(len1, rng);
        std::string password2 = GenerateAlphanumericString(len2, rng);
        
        // Ensure passwords are different
        if (password1 == password2) {
            password2 += "X";
        }
        
        std::string validUsername = "validuser";
        
        auto error = ValidateRegisterForm(validUsername, password1, password2);
        REQUIRE(error == "Passwords do not match");
    }
}

TEST_CASE("Property 3: Username Length Validation", "[register][property]") {
    /**
     * Feature: login-redesign, Property 3: Username Length Validation
     * 
     * For any username string where length < 3 OR length > 20,
     * submitting the form SHALL display "Username must be 3-20 characters"
     * error and NOT attempt authentication/registration.
     * 
     * Validates: Requirements 4.5
     */
    using namespace LoginValidation;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // Test usernames that are too short (0-2 chars)
    std::uniform_int_distribution<size_t> shortDist(0, 2);
    for (int i = 0; i < 50; ++i) {
        size_t length = shortDist(rng);
        std::string shortUsername = GenerateAlphanumericString(length, rng);
        std::string validPassword = "password123";
        
        auto error = ValidateRegisterForm(shortUsername, validPassword, validPassword);
        
        // Empty username has different error message
        if (shortUsername.empty()) {
            REQUIRE(error == "Please enter username");
        } else {
            REQUIRE(error == "Username must be 3-20 characters");
        }
    }
    
    // Test usernames that are too long (21+ chars)
    std::uniform_int_distribution<size_t> longDist(21, 50);
    for (int i = 0; i < 50; ++i) {
        size_t length = longDist(rng);
        std::string longUsername = GenerateAlphanumericString(length, rng);
        std::string validPassword = "password123";
        
        auto error = ValidateRegisterForm(longUsername, validPassword, validPassword);
        REQUIRE(error == "Username must be 3-20 characters");
    }
}

TEST_CASE("Property: Valid inputs pass validation", "[login][register][property]") {
    /**
     * Complementary property: valid inputs should pass validation
     */
    using namespace LoginValidation;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> usernameLengthDist(3, 20);
    std::uniform_int_distribution<size_t> passwordLengthDist(8, 30);
    
    // Run 100 iterations
    for (int i = 0; i < 100; ++i) {
        size_t usernameLen = usernameLengthDist(rng);
        size_t passwordLen = passwordLengthDist(rng);
        
        std::string username = GenerateAlphanumericString(usernameLen, rng);
        std::string password = GenerateAlphanumericString(passwordLen, rng);
        
        // Login form should pass
        auto loginError = ValidateLoginForm(username, password);
        REQUIRE(loginError.empty());
        
        // Register form should pass with matching passwords
        auto registerError = ValidateRegisterForm(username, password, password);
        REQUIRE(registerError.empty());
    }
}
