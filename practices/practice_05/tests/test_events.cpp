// Тесты словаря событий и фильтра (include/tetris/core/events.hpp).
//
// Покрывают требования спеки event-reporting: запрос подмножества, порядок и
// дубликаты, ошибки списка, пустой результат.
#include "tetris/core/events.hpp"

#include "test_framework.hpp"

#include "tetris/core/game.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace tetris;

namespace {

bool contains(const std::vector<std::string>& items, const std::string& value) {
    return std::find(items.begin(), items.end(), value) != items.end();
}

// Сравнение списков: CHECK_EQ не умеет печатать vector<string>, поэтому
// сравниваем булевым результатом с явным сообщением.
bool same(const std::vector<std::string>& got, const std::vector<std::string>& want) {
    if (got == want) {
        return true;
    }
    std::cerr << "    got:";
    for (const std::string& item : got) {
        std::cerr << ' ' << item;
    }
    std::cerr << " | want:";
    for (const std::string& item : want) {
        std::cerr << ' ' << item;
    }
    std::cerr << '\n';
    return false;
}

}  // namespace

TEST(events, keeps_only_requested) {
    std::string error;
    const auto filter = parse_event_filter("clear:tetris", error);
    REQUIRE(filter.has_value());
    CHECK(error.empty());
    CHECK(same(filter_events({"clear:tetris", "combo"}, *filter), {"clear:tetris"}));
}

TEST(events, allows_spaces_around_commas) {
    std::string error;
    const auto filter = parse_event_filter("  clear:single ,   clear:tetris  ", error);
    REQUIRE(filter.has_value());
    CHECK(same(filter_events({"clear:single", "clear:tspin_single"}, *filter), {"clear:single"}));
}

TEST(events, duplicate_names_do_not_duplicate_output) {
    std::string error;
    const auto filter = parse_event_filter("combo,clear:tetris,combo", error);
    REQUIRE(filter.has_value());
    CHECK(same(filter_events({"clear:tetris", "combo"}, *filter), {"clear:tetris", "combo"}));
}

TEST(events, order_follows_snapshot_not_request) {
    std::string error;
    const auto filter = parse_event_filter("combo,clear:tetris", error);
    REQUIRE(filter.has_value());
    // В запросе combo первым, в снапшоте clear:tetris первым — приоритет у снапшота.
    CHECK(same(filter_events({"clear:tetris", "combo"}, *filter), {"clear:tetris", "combo"}));
}

TEST(events, empty_spec_is_error) {
    std::string error;
    CHECK_FALSE(parse_event_filter("", error).has_value());
    CHECK(error.find("пустой") != std::string::npos);
    CHECK_FALSE(parse_event_filter("   ", error).has_value());
}

TEST(events, empty_name_in_list_is_error) {
    std::string error;
    CHECK_FALSE(parse_event_filter("clear:single,", error).has_value());
    CHECK(error.find("пустое") != std::string::npos);
    CHECK_FALSE(parse_event_filter("clear:single,,combo", error).has_value());
    CHECK(error.find("пустое") != std::string::npos);
}

TEST(events, unknown_name_is_error_with_vocabulary) {
    std::string error;
    CHECK_FALSE(parse_event_filter("foo", error).has_value());
    CHECK(error.find("foo") != std::string::npos);
    CHECK(error.find("clear:tetris") != std::string::npos);
}

TEST(events, wrong_case_is_error) {
    std::string error;
    CHECK_FALSE(parse_event_filter("Clear:tetris", error).has_value());
}

TEST(events, bare_clear_label_is_error) {
    // В снапшоте метка линии идёт с префиксом "clear:" — без него имя неизвестно.
    std::string error;
    CHECK_FALSE(parse_event_filter("tetris", error).has_value());
    CHECK(error.find("clear:tetris") != std::string::npos);
}

TEST(events, no_match_gives_empty_result) {
    std::string error;
    const auto filter = parse_event_filter("perfect_clear", error);
    REQUIRE(filter.has_value());
    CHECK(filter_events({"hard_drop"}, *filter).empty());
    CHECK(filter_events({}, *filter).empty());
}

TEST(events, vocabulary_is_pinned) {
    // Эталон: правка списка событий в ядре/подсчёте очков обязана обновить и
    // словарь, иначе фильтр начнёт отвергать реальные имена.
    const std::vector<std::string> expected = {
        "left",         "right",          "rotate_cw",     "rotate_ccw",    "rotate_180",
        "soft_drop",    "hard_drop",      "hold",          "lock_out",      "game_over",
        "clear:single", "clear:double",   "clear:triple",  "clear:tetris",  "clear:tspin",
        "clear:tspin_mini", "clear:tspin_single", "clear:tspin_double", "clear:tspin_triple",
        "combo",        "b2b",            "perfect_clear",
    };
    CHECK(same(known_events(), expected));
}

TEST(events, every_produced_event_is_in_vocabulary) {
    // Разные сценарии дают разные наборы событий; ни одно имя не должно
    // оказаться вне словаря, иначе его нельзя запросить через --events.
    const std::vector<std::string> scripts = {
        "L", "R", "CW", "CCW", "180", "D", "HD", "HOLD", "T5",
        "L4 CW HD", "R3 CCW HD", "180 HOLD HD", "D5 HD", "HD HOLD HD",
        "L2 CW2 D3 HOLD HD", "T120", "HD3 HOLD HD",
    };
    std::size_t produced = 0;
    for (const std::string& script : scripts) {
        for (std::uint64_t seed = 0; seed < 4; ++seed) {
            const GameSnapshot snapshot = run_script(seed, script, 500);
            for (const std::string& event : snapshot.events) {
                ++produced;
                CHECK(contains(known_events(), event));
            }
        }
    }
    CHECK(produced > 0);
}

TEST(events, line_clear_events_carry_clear_prefix) {
    // Найденный прогоном сценарий: фиксация даёт "clear:tspin_mini" и b2b.
    // Проверяем, что события линий приходят именно с префиксом "clear:" и
    // попадают в словарь, то есть их можно запросить через --events.
    const std::string script =
        "L HD CCW D3 HD CW D3 HD 180 HD CCW HD 180 D3 HD CW HD 180 HD R HD 180 D3 HD "
        "180 HD 180 HD R HD CW HD CW HD L D3 HD L HD 180 HD L2 HD L2 HD L2 HD L HD "
        "L D3 HD R HD R2 HD 180 D3 HD R2 HD L2 HD CCW HD R2 D3 HD CW HD CCW HD R2 HD "
        "180 HD 180 D3 HD 180 HD R2 HD CW HD CCW HD 180 HD R2 D3 HD";
    const GameSnapshot snapshot = run_script(53, script, 20000);
    bool saw_clear = false;
    for (const std::string& event : snapshot.events) {
        CHECK(contains(known_events(), event));
        if (event.rfind("clear:", 0) == 0) {
            saw_clear = true;
        }
    }
    CHECK(saw_clear);
}
