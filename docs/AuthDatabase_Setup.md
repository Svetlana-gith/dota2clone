# Authentication Database Setup Guide

## Overview

The authentication system uses SQLite as its database backend. This guide covers database initialization, configuration, and maintenance.

## Database Schema

The authentication database consists of the following tables:

### Core Tables

1. **accounts** - User account information
   - `account_id` (PRIMARY KEY)
   - `username` (UNIQUE)
   - `password_hash` (bcrypt)
   - `email` (UNIQUE, optional)
   - `created_at`, `last_login`
   - `is_banned`, `ban_reason`, `ban_until`

2. **sessions** - Active session tokens
   - `session_token` (PRIMARY KEY)
   - `account_id` (FOREIGN KEY)
   - `created_at`, `expires_at`, `last_used`
   - `ip_address`

3. **login_history** - Login attempt tracking
   - `history_id` (PRIMARY KEY)
   - `account_id` (FOREIGN KEY)
   - `ip_address`, `timestamp`
   - `success`, `failure_reason`

4. **rate_limits** - Rate limiting state
   - `limit_key` (PRIMARY KEY)
   - `attempt_count`, `window_start`, `last_attempt`

5. **two_factor_auth** - 2FA configuration (future)
   - `account_id` (PRIMARY KEY)
   - `totp_secret`, `enabled`, `recovery_codes`

6. **player_stats** - Matchmaking statistics
   - `account_id` (PRIMARY KEY)
   - `rating`, `games_played`, `wins`, `losses`

## Initialization

### Method 1: Using DatabaseManager (Recommended)

The DatabaseManager automatically creates the schema on first initialization:

```cpp
#include "auth/DatabaseManager.h"

auth::DatabaseManager db;
if (!db.Initialize("auth.db")) {
    // Handle error
}
```

### Method 2: Using SQL Script

Run the initialization script:

```bash
cd scripts
sqlite3 ../auth.db < init_auth_db.sql
```

Or on Windows:
```cmd
cd scripts
init_auth_db.bat
```

## Performance Optimizations

The database is configured with the following optimizations:

### WAL Mode (Write-Ahead Logging)
- Enables concurrent readers and writers
- Better performance for multi-threaded access
- Automatically enabled by DatabaseManager

### Indexes
All critical columns are indexed:
- `accounts.username` - Fast username lookups
- `accounts.email` - Fast email lookups
- `sessions.account_id` - Fast session queries by account
- `sessions.expires_at` - Fast expiration cleanup
- `login_history(account_id, timestamp)` - Fast history queries
- `login_history(ip_address, timestamp)` - Fast IP-based queries
- `rate_limits.window_start` - Fast rate limit cleanup

### Cache Settings
- 64MB cache size for frequently accessed data
- Memory-based temporary storage
- 256MB memory-mapped I/O

## Maintenance

### Cleanup Operations

The DatabaseManager provides automatic cleanup methods:

```cpp
// Clean up expired sessions (should run hourly)
int cleaned = db.CleanupExpiredSessions();

// Clean up old login history (should run daily)
int cleaned = db.CleanupOldLoginHistory(90); // Keep 90 days

// Clean up expired rate limits (should run hourly)
int cleaned = db.CleanupExpiredRateLimits();
```

### Backup Strategy

**Recommended backup schedule:**

1. **Daily full backup:**
   ```bash
   sqlite3 auth.db ".backup auth_backup_$(date +%Y%m%d).db"
   ```

2. **Hourly WAL backup:**
   ```bash
   cp auth.db-wal auth.db-wal.backup
   ```

3. **Retention:** Keep backups for 30 days

### Database Integrity Check

Periodically verify database integrity:

```bash
sqlite3 auth.db "PRAGMA integrity_check;"
```

### Vacuum (Defragmentation)

Reclaim unused space (run during low-traffic periods):

```bash
sqlite3 auth.db "VACUUM;"
```

## Security Considerations

### File Permissions

Set appropriate file permissions on the database:

**Linux/Unix:**
```bash
chmod 600 auth.db
chown authserver:authserver auth.db
```

**Windows:**
- Restrict access to SYSTEM and auth server service account only

### Encryption at Rest

For production deployments, consider:
1. Full disk encryption (BitLocker, LUKS)
2. SQLite encryption extensions (SQLCipher)
3. Encrypted filesystem

### SQL Injection Prevention

The DatabaseManager uses parameterized queries exclusively:
- All user input is bound as parameters
- No string concatenation in SQL
- Tested against common injection patterns

## Monitoring

### Key Metrics to Track

1. **Database size:** Monitor growth rate
2. **Query performance:** Log slow queries (>100ms)
3. **Connection errors:** Track failed connections
4. **Lock contention:** Monitor WAL checkpoint delays

### Logging

The DatabaseManager logs:
- All database operations (INFO level)
- Errors and warnings (ERROR/WARN level)
- Performance issues (WARN level)

## Troubleshooting

### Database Locked Errors

If you encounter "database is locked" errors:

1. Verify WAL mode is enabled:
   ```sql
   PRAGMA journal_mode;
   ```

2. Check for long-running transactions
3. Increase busy timeout:
   ```cpp
   sqlite3_busy_timeout(db, 5000); // 5 seconds
   ```

### Corruption Recovery

If database corruption is detected:

1. Stop the auth server
2. Restore from latest backup
3. Run integrity check
4. If backup is also corrupt, use `.recover` command:
   ```bash
   sqlite3 auth.db ".recover" | sqlite3 auth_recovered.db
   ```

### Performance Issues

If queries are slow:

1. Run `ANALYZE` to update statistics:
   ```sql
   ANALYZE;
   ```

2. Check index usage:
   ```sql
   EXPLAIN QUERY PLAN SELECT ...;
   ```

3. Consider adding missing indexes

## Configuration

### DatabaseManager Settings

The DatabaseManager can be configured via code:

```cpp
// Connection timeout
sqlite3_busy_timeout(db, 5000);

// Cache size (in KB, negative = KB)
PRAGMA cache_size = -64000;

// Memory-mapped I/O size
PRAGMA mmap_size = 268435456;
```

### Production Recommendations

For production deployments:

1. **Database location:** Use fast SSD storage
2. **Backup location:** Separate physical drive
3. **File system:** ext4 (Linux) or NTFS (Windows)
4. **Monitoring:** Set up alerts for errors and performance
5. **Maintenance window:** Schedule weekly vacuum during low traffic

## Migration

When updating the schema:

1. Create migration script
2. Test on backup copy
3. Apply during maintenance window
4. Verify with integrity check

Example migration:
```sql
BEGIN TRANSACTION;

-- Add new column
ALTER TABLE accounts ADD COLUMN phone_number TEXT;

-- Create index
CREATE INDEX idx_accounts_phone ON accounts(phone_number);

COMMIT;
```

## References

- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [SQLite WAL Mode](https://www.sqlite.org/wal.html)
- [SQLite Performance Tuning](https://www.sqlite.org/speed.html)
