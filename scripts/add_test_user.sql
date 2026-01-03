-- Add test user to authentication database
-- Username: testuser
-- Password: password123
-- Password hash generated with bcrypt (cost=10)

-- Note: This is a bcrypt hash of "password123"
-- Hash: $2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy

INSERT INTO accounts (username, password_hash, created_at, last_login)
VALUES (
    'testuser',
    '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy',
    strftime('%s', 'now'),
    NULL
);

-- Verify insertion
SELECT account_id, username, created_at FROM accounts WHERE username = 'testuser';
