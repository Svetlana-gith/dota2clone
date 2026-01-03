# Dedicated Server - Quick Start

## ✅ Статус: РАБОТАЕТ!

Dedicated Server успешно собран и запущен.

## 🚀 Запуск сервера

### Компиляция
```bash
cmake --build build --config Release --target DedicatedServer
```

### Запуск (порт по умолчанию 27015)
```bash
.\build\bin\Release\DedicatedServer.exe
```

### Запуск на кастомном порту
```bash
.\build\bin\Release\DedicatedServer.exe 7777
```

## 📊 Вывод сервера

```
[18:42:35.260] [info] === Dedicated Server Initializing ===
[18:42:35.262] [info] Network system initialized
[18:42:35.262] [info] EntityManager initialized
[18:42:35.263] [info] Server world created
[18:42:35.264] [info] Socket bound to port 27015
[18:42:35.264] [info] Network server started on port 27015
[18:42:35.264] [info] === Dedicated Server Ready ===
[18:42:35.265] [info] Listening on port 27015
[18:42:35.265] [info] Tick rate: 30 Hz
[18:42:35.265] [info] Press Ctrl+C to stop
```

## 🎮 Что работает

- ✅ UDP сервер на порту 27015
- ✅ ServerWorld (игровая симуляция)
- ✅ Fixed timestep (30 Hz)
- ✅ Client connection handling
- ✅ Packet receiving/sending
- ✅ Game state snapshots
- ✅ Stats logging (каждые 10 секунд)

## 📝 Следующие шаги

### Phase 1: Client Integration (NEXT)
1. **Интегрировать NetworkClient в InGameState**
   - Добавить NetworkClient в InGameState
   - Подключаться к серверу при входе в игру
   - Отключаться при выходе

2. **Отправка Input с клиента**
   - Собирать PlayerInput из мыши/клавиатуры
   - Отправлять на сервер каждый фрейм
   - Буферизировать для prediction

3. **Получение Snapshots**
   - Принимать WorldSnapshot от сервера
   - Обновлять ClientWorld
   - Интерполировать позиции

### Phase 2: Client-Side Prediction
1. **Prediction**
   - Локально симулировать свои действия
   - Не ждать ответа сервера

2. **Reconciliation**
   - Сравнивать локальное состояние с сервером
   - Пересимулировать при расхождении

3. **Interpolation**
   - Плавно интерполировать других игроков
   - Использовать SnapshotBuffer

### Phase 3: UI Integration
1. **Connection UI**
   - Экран подключения
   - Индикатор пинга
   - Индикатор потери пакетов

2. **Lobby System**
   - Список серверов
   - Создание/присоединение к игре

### Phase 4: Testing
1. **Local Multiplayer Test**
   - Запустить DedicatedServer.exe
   - Запустить 2+ клиента Game.exe
   - Проверить синхронизацию

2. **Network Conditions**
   - Тест с искусственной задержкой
   - Тест с потерей пакетов
   - Тест с jitter

## 🔧 Конфигурация

### Сетевые параметры
Файл: `src/common/NetworkTypes.h`
```cpp
constexpr u32 SERVER_TICK_RATE = 30;        // Hz
constexpr f32 INTERPOLATION_DELAY = 0.1f;   // 100ms
constexpr u32 MAX_CLIENTS = 10;
```

### Порт сервера
Файл: `src/network/NetworkCommon.h`
```cpp
constexpr u16 DEFAULT_SERVER_PORT = 27015;
constexpr u32 MAX_PACKET_SIZE = 1400;
constexpr f32 CLIENT_TIMEOUT = 10.0f;
```

## 🐛 Отладка

### Логирование
Сервер использует spdlog с цветным выводом:
- **[info]** - обычные события
- **[warn]** - предупреждения
- **[error]** - ошибки

### Wireshark
Для анализа сетевых пакетов:
```
Filter: udp.port == 27015
```

### Статистика сервера
Каждые 10 секунд выводится:
- Tick Rate (фактический vs целевой)
- Количество подключенных клиентов
- Количество сущностей в мире
- Игровое время

## 📦 Архитектура

```
DedicatedServer.exe
    ├─ NetworkServer (UDP socket, client management)
    ├─ ServerWorld (authoritative simulation)
    │   ├─ EntityManager (ECS)
    │   ├─ HeroSystem
    │   ├─ CreepSystem
    │   ├─ TowerSystem
    │   └─ CombatSystem
    └─ Fixed timestep loop (30 Hz)
```

## 🎯 Цель

Создать полноценный multiplayer для MOBA:
- 10 игроков (5v5)
- Authoritative server
- Client-side prediction
- Entity interpolation
- Низкая латентность (<100ms)

---

**Статус:** ✅ Сервер работает, готов к интеграции с клиентом!
