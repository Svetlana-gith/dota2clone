# HeroPick Crash Investigation

## Статус: В процессе

## Проблема
Краш при любом клике мышью в HeroPickState. Игра падает внутри `CPanel2D::OnMouseDown`.

## Что сделано

### Исправления
1. **Дублирование MatchReady** - добавлен флаг `m_matchReadyHandled` в MainMenuState
2. **Разделение логов** - каждый клиент пишет в `logs/game_<PID>.log`
3. **Защита от рекурсии** - добавлен depth tracking в `CPanel2D::OnMouseDown`

### Отладка
- Добавлено логирование в HeroPickState, CUIEngine, CPanel2D
- Временно отключена обработка мыши в HeroPickState (Login/MainMenu работают)
- Отключен callback `OnHeroPick` для изоляции проблемы

## Следующие шаги
1. Включить мышь обратно и добавить логи с ID панелей
2. Проверить корректность UI дерева в HeroPickState::CreateUI
3. Проверить очистку UI при переходе между состояниями
4. Искать dangling pointers или проблемы с shared_ptr

## Изменённые файлы
- `src/game/GameState.h`
- `src/game/states/MainMenuState.cpp`
- `src/game/GameMain.cpp`
- `src/game/states/HeroPickState.cpp`
- `src/game/ui/panorama/CUIEngine.cpp`
- `src/game/ui/panorama/CPanel2D.cpp`
