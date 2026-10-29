// Подтверждение перезапуска по клавише R (спека: specs/001-restart-confirm/).
//
// Модуль ядра: политика «когда перезапуск требует второго нажатия» и ничего
// больше. Здесь нет оконной библиотеки, нет системных часов и нет файлов —
// время приходит снаружи как прошедшие миллисекунды кадра, поэтому поведение
// детерминировано и проверяется юнит-тестами без окна.
#pragma once

namespace tetris {

// Что произошло в этом кадре (одно значение на шаг).
enum class RestartOutcome {
    None,       // ничего не случилось
    Opened,     // окно подтверждения открыто
    Confirmed,  // перезапуск подтверждён — единственный сигнал к Game::reset
    Cancelled,  // окно закрыто без перезапуска
    Expired,    // окно закрыто по истечении 2000 мс
    Ignored,    // нажатие R пришло слишком быстро после открытия
};

// Откуда открыто окно: из идущей партии или из паузы.
enum class RestartSource { None, Game, Pause };

// Почему окно закрылось (для UI и для тестов).
enum class RestartCloseReason {
    None,          // ещё ни разу не закрывалось
    Confirmed,     // закрыто подтверждением
    PlayerAction,  // игровая клавиша проглочена, окно снято
    Timeout,       // истекли 2000 мс
    GameOver,      // партия завершилась при открытом окне
    FocusLost,     // окно потеряло фокус
};

// Факты одного кадра. UI заполняет их и вызывает step() ровно один раз за кадр.
struct FrameFacts {
    int delta_ms{0};                  // прошедшие мс кадра, отрицательные трактуются как 0
    bool r_pressed{false};            // R нажато без автоповтора; несколько нажатий сводятся к одному
    bool action_key_pressed{false};   // игровая клавиша нажата и проглочена (не применялась)
    bool pause{false};                // партия сейчас на паузе
    bool game_over{false};            // партия завершена
    bool focus_lost{false};           // окно потеряло фокус в этом кадре
};

// Состояние для отрисовки оверлея и для решений UI.
struct RestartState {
    bool active{false};
    int remaining_ms{0};     // 0..kWindowMs
    int window_ms{2000};
    double progress{0.0};    // remaining_ms / window_ms, 0.0..1.0
    RestartSource source{RestartSource::None};
    RestartCloseReason last_close_reason{RestartCloseReason::None};
};

class RestartGuard {
public:
    static constexpr int kWindowMs = 2000;      // срок подтверждения
    static constexpr int kMinIntervalMs = 50;   // антидребезг между нажатиями R

    // Шаг кадра: время → game over → игровая клавиша → потеря фокуса → R.
    RestartOutcome step(const FrameFacts& facts);

    const RestartState& state() const { return state_; }
    bool active() const { return state_.active; }

    // Полный сброс (например, при смене партии извне модуля).
    void clear();

private:
    void open(RestartSource source);
    void close(RestartCloseReason reason);

    RestartState state_{};
    int elapsed_ms_{0};  // накопленное время открытого окна
};

}  // namespace tetris
