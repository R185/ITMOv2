// Реализация словаря событий и фильтра (include/tetris/core/events.hpp).
//
// Список имён обязан совпадать с тем, что реально кладут в events_ ядро
// (src/core/game.cpp: left, right, rotate_*, soft_drop, hard_drop, hold,
// lock_out, game_over) и подсчёт очков (src/core/scoring.cpp: события линий
// вида "clear:<метка>" из clear_label, плюс combo, b2b, perfect_clear).
// Расхождение ловит tests/test_events.cpp: он сверяет словарь с закреплённым
// эталоном и проверяет, что в прогонах не появляется имён вне словаря.
#include "tetris/core/events.hpp"

#include <algorithm>

namespace tetris {
namespace {

// Эталонный словарь. Порядок — логический (сначала управление, затем фиксация),
// он же используется при печати перечня допустимых имён.
const std::vector<std::string>& vocabulary() {
    static const std::vector<std::string> names = {
        // Управление и состояние партии (src/core/game.cpp).
        "left",
        "right",
        "rotate_cw",
        "rotate_ccw",
        "rotate_180",
        "soft_drop",
        "hard_drop",
        "hold",
        "lock_out",
        "game_over",
        // Фиксация: события линий приходят с префиксом "clear:" и меткой из
        // src/core/scoring.cpp (clear_label); отдельно идут мета-события
        // комбо/бэк-ту-бэк и очистка поля.
        "clear:single",
        "clear:double",
        "clear:triple",
        "clear:tetris",
        "clear:tspin",
        "clear:tspin_mini",
        "clear:tspin_single",
        "clear:tspin_double",
        "clear:tspin_triple",
        "combo",
        "b2b",
        "perfect_clear",
    };
    return names;
}

std::string_view trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' ||
                           text[begin] == '\r')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' ||
                           text[end - 1] == '\r')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool is_known(std::string_view name) {
    const auto& names = vocabulary();
    return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

const std::vector<std::string>& known_events() { return vocabulary(); }

std::string known_events_list() {
    std::string out;
    for (const std::string& name : vocabulary()) {
        if (!out.empty()) {
            out += ", ";
        }
        out += name;
    }
    return out;
}

std::optional<EventFilter> parse_event_filter(std::string_view spec, std::string& error) {
    error.clear();
    const std::string_view trimmed = trim(spec);
    if (trimmed.empty()) {
        error = "пустой список событий";
        return std::nullopt;
    }

    EventFilter filter;
    std::size_t start = 0;
    while (start <= trimmed.size()) {
        const std::size_t comma = trimmed.find(',', start);
        const std::string_view raw =
            comma == std::string_view::npos ? trimmed.substr(start) : trimmed.substr(start, comma - start);
        const std::string_view name = trim(raw);
        if (name.empty()) {
            error = "пустое имя события в списке";
            return std::nullopt;
        }
        if (!is_known(name)) {
            error = "неизвестное событие \"" + std::string(name) + "\" (допустимые: " +
                    known_events_list() + ")";
            return std::nullopt;
        }
        filter.emplace(name);
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return filter;
}

std::vector<std::string> filter_events(const std::vector<std::string>& events,
                                       const EventFilter& filter) {
    std::vector<std::string> out;
    out.reserve(events.size());
    for (const std::string& event : events) {
        if (filter.find(event) != filter.end()) {
            out.push_back(event);
        }
    }
    return out;
}

}  // namespace tetris
