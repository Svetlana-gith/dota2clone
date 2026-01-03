/**
 * Authentication Server - Standalone executable
 * 
 * Runs the authentication server on port 27016 (default).
 * Creates auth.db database automatically on first run.
 */

#include "auth/AuthServer.h"
#include "core/Types.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Shutdown signal received");
        g_running.store(false);
    }
}

int main(int argc, char* argv[]) {
    // Setup logging
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);
    
    // Parse command line arguments
    u16 port = 27016;  // Default auth server port
    std::string dbPath = "auth.db";
    
    if (argc >= 2) {
        try {
            port = static_cast<u16>(std::stoi(argv[1]));
        } catch (...) {
            spdlog::error("Invalid port number: {}", argv[1]);
            return 1;
        }
    }
    
    if (argc >= 3) {
        dbPath = argv[2];
    }
    
    spdlog::info("=== Authentication Server ===");
    spdlog::info("Port: {}", port);
    spdlog::info("Database: {}", dbPath);
    spdlog::info("");
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create and initialize auth server
    auth::AuthServer server;
    
    if (!server.Initialize(port, dbPath)) {
        spdlog::error("Failed to initialize auth server");
        return 1;
    }
    
    spdlog::info("Auth server initialized successfully");
    spdlog::info("Listening on port {}", port);
    spdlog::info("Press Ctrl+C to stop");
    spdlog::info("");
    
    // Run server (non-blocking mode with manual update loop)
    server.Run(false);
    
    // Main loop
    while (g_running.load()) {
        server.Update(100);  // 100ms timeout for packet processing
        
        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    spdlog::info("Shutting down auth server...");
    server.Shutdown();
    spdlog::info("Auth server stopped");
    
    return 0;
}
