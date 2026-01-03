-- Authentication System Database Initialization Script
-- This script creates all necessary tables and indexes for the auth system

-- Enable foreign keys
PRAGMA foreign_keys = ON;

-- Enable WAL mode for better concurrency
PRAGMA journal_mode = WAL;

-- Optimize performance
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = -64000;
PRAGMA temp_store = MEMORY;

-- Accounts table
CREATE TABLE IF NOT EXISTS accounts (
    account_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    email TEXT UNIQUE,
    created_at INTEGER NOT NULL,
    last_login INTEGER,
    is_banned INTEGER DEFAULT 0,
    ban_reason TEXT,
    ban_until INTEGER
);

CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts(username);
CREATE INDEX IF NOT EXISTS idx_accounts_email ON accounts(email);

-- Sessions table
CREATE TABLE IF NOT EXISTS sessions (
    session_token TEXT PRIMARY KEY,
    account_id INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL,
    ip_address TEXT,
    last_used INTEGER,
    FOREIGN KEY(account_id) REFERENCES accounts(account_id)
);

CREATE INDEX IF NOT EXISTS idx_sessions_account ON sessions(account_id);
CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);

-- Login history table
CREATE TABLE IF NOT EXISTS login_history (
    history_id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    ip_address TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    success INTEGER NOT NULL,
    failure_reason TEXT,
    FOREIGN KEY(account_id) REFERENCES accounts(account_id)
);

CREATE INDEX IF NOT EXISTS idx_login_history_account ON login_history(account_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_login_history_ip ON login_history(ip_address, timestamp DESC);

-- Rate limiting table
CREATE TABLE IF NOT EXISTS rate_limits (
    limit_key TEXT PRIMARY KEY,
    attempt_count INTEGER NOT NULL,
    window_start INTEGER NOT NULL,
    last_attempt INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_rate_limits_window ON rate_limits(window_start);

-- Two-factor authentication table (for future use)
CREATE TABLE IF NOT EXISTS two_factor_auth (
    account_id INTEGER PRIMARY KEY,
    totp_secret TEXT NOT NULL,
    enabled INTEGER DEFAULT 0,
    recovery_codes TEXT,
    FOREIGN KEY(account_id) REFERENCES accounts(account_id)
);

-- Player statistics (for matchmaking integration)
CREATE TABLE IF NOT EXISTS player_stats (
    account_id INTEGER PRIMARY KEY,
    rating INTEGER DEFAULT 1500,
    games_played INTEGER DEFAULT 0,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    FOREIGN KEY(account_id) REFERENCES accounts(account_id)
);

-- Verify schema
SELECT 'Database schema created successfully' AS status;
