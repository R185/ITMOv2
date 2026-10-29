# Tasks: подтверждение перезапуска по клавише R

**Input**: `specs/001-restart-confirm/{spec-ui.md,spec-logic.md,contract.md,plan.md,data-model.md,contracts/restart-guard.md}`

**Tests**: включены — конституция проекта (принцип IV) требует тест на новое поведение.

**Organization**: по пользовательским историям спеки логики (P1–P5); задачи UI вынесены в свою
фазу, потому что зависят от готового модуля. Все задачи выполнены; у каждой указан критерий
проверки и фактический результат.

## Phase 1: Setup

- [x] T001 [P] Зафиксировать скоуп и проверить, что фичи ещё нет: `src/ui/sdl_app.cpp`,
      `case SDLK_r` — мгновенный сброс. **Проверка**: код прочитан, поведение воспроизведено
      чтением; вывод — нужна не «reset», а «подтверждение reset». **Результат**: скоуп изменён.
- [x] T002 [P] Согласовать общий контракт двух команд до кода: `specs/001-restart-confirm/contract.md`.
      **Проверка**: каждая из шести претензий ссылается на требование обеих спек и имеет решение.
      **Результат**: 6 решений, обе спеки обновлены (`*.before.md` сохранены).

## Phase 2: Foundational (блокирует все истории)

- [x] T003 Создать интерфейс модуля `include/tetris/core/restart_guard.hpp`: `FrameFacts`,
      `RestartState`, перечисления, `RestartGuard::step/state/active/clear`, константы
      `kWindowMs = 2000`, `kMinIntervalMs = 50`. **Проверка**: в файле нет включений SDL,
      `<chrono>`, обращений к времени и файлам. **Результат**: grep по `SDL|chrono|GetTicks|time(|rand(`
      — пусто.
- [x] T004 Реализовать политику `src/core/restart_guard.cpp`: порядок разбора кадра
      время → game over → игровая клавиша → фокус → R. **Проверка**: собирается в
      `tetris_tests` (цель без SDL). **Результат**: `make build` — 0 предупреждений.

**Checkpoint**: модуль компилируется независимо от UI, партия им не управляется.

## Phase 3: User Story 1 — двойное нажатие R сбрасывает партию (P1, MVP)

**Goal**: случайное R не стирает партию, осознанное второе — стирает, с тем же seed.

**Independent Test**: `./build/tetris_tests` — набор `restart.*` без дисплея.

- [x] T005 [P] [US1] Тесты открытия и подтверждения: `single_press_opens_window`,
      `second_press_confirms`, `confirmed_reported_once`, `remaining_and_progress_are_clamped`
      в `tests/test_restart_guard.cpp`. **Проверка**: тесты падают до реализации, зелёные после.
- [x] T006 [US1] Границы срока: `deadline_expires_window` (125×16 мс = 2000),
      `boundary_one_ms_before_deadline_confirms` (остаток 1 мс — ещё подтверждает),
      `press_after_expiry_opens_new_window`, `negative_and_huge_delta`.
      **Проверка**: `Expired` ровно на 2000 мс, отрицательная дельта не двигает окно назад.
- [x] T007 [US1] Антидребезг: `debounce_ignores_press_within_50ms` — второе R через 20 мс даёт
      `Ignored`, остаток окна не меняется. **Проверка**: тест зелёный, `remaining_ms == 1980`.
- [x] T008 [US1] Сверка сброса с ядром: `reset_with_same_seed_matches_fresh_game` —
      `Game::reset(seed)` даёт JSON, равный `Game(seed)`. **Проверка**: тест зелёный, счёт,
      линии, уровень и tick обнулены.

**Checkpoint**: политика P1 закрыта тестами без окна.

## Phase 4: User Story 2 — безопасная отмена (P2)

- [x] T009 [US2] Отмена игровой клавишей: `action_key_cancels_without_restart` (итог
      `Cancelled`, причина `PlayerAction`), клавиша не применяется — это обязанность UI из
      `contracts/restart-guard.md`, п. 4.
- [x] T010 [US2] Отмена потерей фокуса: `focus_lost_cancels` (причина `FocusLost`).
- [x] T011 [US2] Удержание R: `no_repeat_frames_do_not_confirm` — кадры без нажатия не
      подтверждают, окно только истекает. **Проверка**: 200 кадров, финал — `Timeout`.

**Checkpoint**: случайные действия не теряют партию.

## Phase 5: User Story 3 — game over (P3)

- [x] T012 [US3] `game_over_press_confirms_immediately`: на экране проигрыша окно не открывается,
      R сразу даёт `Confirmed`.
- [x] T013 [US3] `game_over_while_window_open_cancels`: переход в game over при открытом окне
      закрывает его с причиной `GameOver` и не сбрасывает партию.

## Phase 6: User Story 4 — пауза (P4)

- [x] T014 [US4] `pause_opens_window_with_pause_source`: R при паузе открывает окно с
      `source = Pause` (решение №1 контракта — пауза не блокирует R).
- [x] T015 [US4] Зафиксировать в коде и контракте: пауза не может включиться при открытом окне —
      клавиша паузы приходит как игровая и проглатывается UI (решение №3).

## Phase 7: User Story 5 — контракт для UI (P3)

- [x] T016 [US5] `step()` возвращает ровно один итог на кадр, приоритеты зафиксированы в
      `data-model.md` и реализованы порядком проверок. **Проверка**: тесты на комбинации
      «game over + R» и «истечение + R» дают `Confirmed` и `Opened` соответственно.
- [x] T017 [US5] `RestartState` отдаёт `active/remaining_ms/window_ms/progress/source/
      last_close_reason`; инварианты 1–2 из `data-model.md` проверены тестами.

## Phase 8: UI-слой (после модуля)

- [x] T018 [P] `src/ui/sdl_app.cpp`: `handle_event` превращён в сборщик фактов кадра
      (`FrameEvents`: quit, focus_lost, r_pressed, action_key_pressed, toggle_pause, actions);
      автоповтор отбрасывается (`event.key.repeat != 0`).
- [x] T019 Игровой цикл: `RestartGuard` как член `run_sdl`, один `step()` на кадр, `Game::reset`
      только по `Confirmed` с сидом до сброса, сброс `best_recorded` и аккумулятора тиков.
- [x] T020 Пока окно открыто — игровые действия не применяются, гравитация не идёт
      (`!restart_guard.active()` в условии тиков). **Проверка**: сборка SDL-цели + запуск.
- [x] T021 Оверлей: приоритет `RESTART/PRESS R AGAIN` (свой цвет) выше `GAME OVER` и `PAUSED`;
      подсказка `R RESTART` в панели. Тексты только из набора 3x5 (`A–Z`, `0–9`, пробел).

## Phase 9: Polish

- [x] T022 Проверить совместимость: headless JSON и ASCII `--seed 42 --script "D3 HOLD HD"`
      побайтно равны эталону, снятому до правок. **Результат**: `cmp` пуст.
- [x] T023 Проверить MCP: `mcp_smoke.py` — 6 инструментов, `sim(seed=42, script=HD)` отвечает.
- [x] T024 Прогнать `make test`: **116/116 тестов, 1127 проверок, 0 падений** (было 100 тестов).
- [x] T025 Запустить SDL-цикл без дисплея: `SDL_VIDEODRIVER=dummy timeout 3 ./build/tetris`
      → код 124 (работал до таймаута), ошибок в выводе нет.
- [ ] T026 Ручная проверка в живом окне: R → окно, второе R → новая партия, истечение 2 с →
      отмена. Требует дисплея, остаётся за исполнителем.
- [x] T027 Отчёт: `REPORT.md` — оба трека, пайплайн Spec Kit, таблица проверок, рефлексия и
      сравнение фреймворков.

## Dependencies & Execution Order

- Phase 1 → Phase 2 (контракт и интерфейс) → Phase 3 (P1) → Phase 4–7 (независимы между собой,
  зависят только от модуля) → Phase 8 (UI, требует готового модуля) → Phase 9.
- Внутри истории: тест → реализация → прогон. Тесты одного файла выполняются последовательно.

## Notes

- Все тесты живут в `tests/test_restart_guard.cpp` и не требуют SDL — это и было целью
  размещения модуля в ядре.
- Задача T026 не выполнена автоматически: нет дисплея в неинтерактивной сессии.
