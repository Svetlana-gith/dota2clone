# Быстрый старт - Аутентификация

## ✅ Всё готово для тестирования!

### Вариант 1: Гостевой режим (самый простой)
**Не требует auth сервера!**

1. Запустите игру
2. Нажмите **"Play as Guest"**
3. Готово!

---

### Вариант 2: С тестовым аккаунтом

#### Шаг 1: Запустите Auth Server
```bash
start_auth_server.bat
```

Вы увидите:
```
=== Authentication Server ===
Port: 27016
Database: auth.db

Auth server initialized successfully
Listening on port 27016
Press Ctrl+C to stop
```

#### Шаг 2: Войдите в игру
Запустите игру и используйте:
- **Username:** `testuser`
- **Password:** `password123`

**Тестовый пользователь уже создан и готов к использованию!**

---

### Вариант 3: Создать новый аккаунт

1. Запустите auth сервер (`start_auth_server.bat`)
2. Запустите игру
3. Нажмите **"Create Account"**
4. Введите свои данные:
   - Username: 3-20 символов (буквы, цифры, подчеркивание)
   - Password: минимум 8 символов

---

## Файлы

### Созданные файлы:
- `auth.db` - база данных с аккаунтами
- `auth.db-shm`, `auth.db-wal` - служебные файлы SQLite

### Утилиты:
- `start_auth_server.bat` - запуск auth сервера
- `scripts/add_test_user.bat` - добавить тестового пользователя
- `build/bin/Debug/AuthServer.exe` - auth сервер
- `build/bin/Debug/AddTestUser.exe` - утилита добавления тестового пользователя

---

## Troubleshooting

### "Cannot connect to auth server"
- Убедитесь, что auth сервер запущен
- Проверьте, что он слушает на порту 27016

### "Test user already exists"
Это нормально! Пользователь уже создан, просто войдите с учетными данными.

### Сбросить всё
```bash
# Остановите auth сервер (Ctrl+C)
# Удалите файлы базы данных
del auth.db auth.db-shm auth.db-wal
# Запустите auth сервер снова - создаст новую БД
start_auth_server.bat
# Добавьте тестового пользователя
scripts\add_test_user.bat
```

---

## Подробная документация

См. `docs/AuthServer_QuickStart.md` для полной информации.
