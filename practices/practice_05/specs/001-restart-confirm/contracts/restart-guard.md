# Контракт модуля: `tetris::RestartGuard`

Заголовок: `include/tetris/core/restart_guard.hpp`. Реализация: `src/core/restart_guard.cpp`.
Заголовок — замороженный интерфейс (принцип III конституции): сигнатуры меняет архитектор.

## Публичный интерфейс

```cpp
namespace tetris {

enum class RestartOutcome { None, Opened, Confirmed, Cancelled, Expired, Ignored };
enum class RestartSource { None, Game, Pause };
enum class RestartCloseReason { None, Confirmed, PlayerAction, Timeout, GameOver, FocusLost };

struct FrameFacts {
    int delta_ms{0};
    bool r_pressed{false};
    bool action_key_pressed{false};
    bool pause{false};
    bool game_over{false};
    bool focus_lost{false};
};

struct RestartState {
    bool active{false};
    int remaining_ms{0};
    int window_ms{2000};
    double progress{0.0};
    RestartSource source{RestartSource::None};
    RestartCloseReason last_close_reason{RestartCloseReason::None};
};

class RestartGuard {
public:
    static constexpr int kWindowMs = 2000;
    static constexpr int kMinIntervalMs = 50;

    RestartOutcome step(const FrameFacts& facts);
    const RestartState& state() const;
    bool active() const;
    void clear();
};

}  // namespace tetris
```

## Обязанности вызывающей стороны (UI)

1. Вызывать `step` ровно один раз за отрисованный кадр — в игре, в паузе и на экране game over.
2. Передавать фактическое время кадра; собственного таймера окна не держать.
3. Передавать `r_pressed` только при `event.key.repeat == 0`; несколько нажатий в кадре
   сводить к одному.
4. Пока `active() == true`, не применять игровые действия к `Game` — передавать
   `action_key_pressed`, а клавишу проглатывать.
5. Вызывать `Game::reset(seed)` только по итогу `Confirmed`, с сидом, взятым до сброса.
6. Рисовать оверлей по `state()`: `active`, `remaining_ms`, `progress`, `window_ms`.
7. После `Confirmed` сбросить `best_recorded`, чтобы рекорд новой партии записался.
8. Не передавать состояние окна в `GameSnapshot` и в JSON headless-вывода.

## Гарантии модуля

1. Окно не открывается при `game_over == true`: нажатие R сразу даёт `Confirmed`.
2. Открытие окна не меняет партию и не вызывает `Game`.
3. `Confirmed` — единственный итог, требующий сброса; возвращается не более одного раза за окно.
4. Время накапливается монотонно, отрицательная дельта игнорируется, скачок ⩾2000 мс закрывает
   окно одним шагом.
5. Нажатие R внутри 50 мс после открытия — `Ignored`, окно не продлевается и не закрывается.
6. Никаких зависимостей от оконной библиотеки, системных часов и файловой системы.

## Совместимость

- Headless CLI и MCP: без изменений; токен `R` в сценариях по-прежнему означает «вправо».
- `GameSnapshot` и формат JSON: без изменений (регрессия проверяется побайтно).
- Публичные заголовки ядра, кроме нового файла: без изменений.
