# Authentication Server - Quick Start

## Запуск Auth Сервера

### Простой способ
Запустите батник:
```bash
start_auth_server.bat
```

### Вручную
```bash
build\bin\Debug\AuthServer.exe
```

Auth сервер:
- Слушает на порту **27016**
- Автоматически создает файл **auth.db** при первом запуске
- Логирует все операции в консоль

## Тестовые учетные данные

### Вариант 1: Гостевой режим (рекомендуется)
Самый простой способ для тестирования:
1. Запустите игру
2. На экране логина нажмите **"Play as Guest"**
3. Готово! Не требует auth сервера

### Вариант 2: Создать новый аккаунт
1. Запустите auth сервер (`start_auth_server.bat`)
2. Запустите игру
3. Нажмите **"Create Account"**
4. Введите:
   - Username: `testuser` (3-20 символов)
   - Password: `password123` (минимум 8 символов)
   - Confirm Password: `password123`
5. Нажмите **"CREATE ACCOUNT"**

### Вариант 3: Добавить тестового пользователя в БД
Если нужен готовый тестовый аккаунт:

1. Запустите auth сервер один раз (создаст auth.db), затем остановите его
2. Запустите:
   ```bash
   scripts\add_test_user.bat
   ```
   ИЛИ напрямую:
   ```bash
   build\bin\Debug\AddTestUser.exe
   ```
3. Запустите auth сервер снова
4. Войдите с учетными данными:
   - **Username:** `testuser`
   - **Password:** `password123`

## Проверка работы

### Auth сервер запущен правильно, если видите:
```
[15:38:52.123] [info] === Authentication Server ===
[15:38:52.124] [info] Port: 27016
[15:38:52.125] [info] Database: auth.db
[15:38:52.126] [info] Database opened: auth.db
[15:38:52.127] [info] Auth server initialized successfully
[15:38:52.128] [info] Listening on port 27016
[15:38:52.129] [info] Press Ctrl+C to stop
```

### Файлы, которые создаются:
- `auth.db` - база данных SQLite с аккаунтами
- `auth.db-shm` - shared memory (WAL mode)
- `auth.db-wal` - write-ahead log (WAL mode)

## Остановка сервера

Нажмите **Ctrl+C** в окне auth сервера

## Troubleshooting

### Ошибка: "Failed to bind socket to port 27016"
- Порт уже занят другим процессом
- Проверьте, не запущен ли уже auth сервер
- Или измените порт: `AuthServer.exe 27017`

### Ошибка: "Cannot connect to auth server" в игре
- Убедитесь, что auth сервер запущен
- Проверьте, что он слушает на порту 27016
- Проверьте firewall (может блокировать локальные подключения)

### База данных повреждена
1. Остановите auth сервер
2. Удалите файлы: `auth.db`, `auth.db-shm`, `auth.db-wal`
3. Запустите auth сервер снова (создаст новую БД)

## Архитектура

```
AuthServer.exe
    ├─ UDP Socket (port 27016)
    ├─ DatabaseManager (SQLite)
    │   ├─ accounts table
    │   ├─ sessions table
    │   └─ login_history table
    └─ SecurityManager (bcrypt)
```

## Следующие шаги

- Для production используйте Release сборку
- Настройте автоматический запуск как Windows Service
- Настройте регулярные бэкапы auth.db
- См. `docs/AuthDatabase_Setup.md` для деталей

## Полезные команды

### Просмотр аккаунтов в БД
```bash
sqlite3 auth.db "SELECT account_id, username, created_at FROM accounts;"
```

### Сброс пароля
```bash
sqlite3 auth.db "UPDATE accounts SET password_hash='$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy' WHERE username='testuser';"
```
(Устанавливает пароль `password123`)

### Удаление аккаунта
```bash
sqlite3 auth.db "DELETE FROM accounts WHERE username='testuser';"
```
