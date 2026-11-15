#include "balance.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace core {

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool parseInt(const std::string& s, int* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    *out = static_cast<int>(v);
    return true;
}

bool parseDouble(const std::string& s, double* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (*end != '\0') return false;
    *out = v;
    return true;
}

bool parseEffect(const std::string& s, Effect* out) {
    if (s == "mid" || s == "middle") {
        *out = Effect::mid();
        return true;
    }
    int v = 0;
    if (!parseInt(s, &v)) return false;
    *out = Effect::add(v);
    return true;
}

// Привязка имени ключа к полю баланса.
struct IntKey {
    const char* name;
    int Balance::* field;
};
const IntKey kIntKeys[] = {
    {"rating.start", &Balance::start},
    {"rating.min", &Balance::minRating},
    {"rating.max", &Balance::maxRating},
    {"rating.middle", &Balance::middle},
    {"rating.middle_step", &Balance::middleStep},
    {"threshold.bad_low_max", &Balance::badLowMax},
    {"threshold.confirm_max", &Balance::confirmMax},
    {"threshold.normal_max", &Balance::normalMax},
    {"threshold.watch_max", &Balance::watchMax},
    {"shift.every_days", &Balance::shiftEvery},
    {"convoy.strikes_to_kill", &Balance::convoyStrikesToKill},
    {"convoy.reminder_seconds", &Balance::convoyReminder},
    {"convoy.strike_every_seconds", &Balance::convoyStrikeEvery},
    {"convoy.shift_reminder_seconds", &Balance::convoyShiftReminder},
    {"convoy.evening_reminder_seconds", &Balance::convoyEveningReminder},
    {"convoy.lag_seconds", &Balance::convoyLagSeconds},
};

Effect* findEffect(Balance& b, const std::string& key) {
    struct EffectKey {
        const char* name;
        Effect* field;
    };
    const EffectKey keys[] = {
        {"cards.check_numbers.confirm_both", &b.cards[0][0]},
        {"cards.check_numbers.close_request", &b.cards[0][1]},
        {"cards.check_numbers.request_log", &b.cards[0][2]},
        {"cards.check_history.confirm_both", &b.cards[1][0]},
        {"cards.check_history.close_request", &b.cards[1][1]},
        {"cards.check_history.request_log", &b.cards[1][2]},
        {"gauge.accept_journal", &b.gauge[0]},
        {"gauge.mark_mismatch", &b.gauge[1]},
        {"gauge.request_all_reports", &b.gauge[2]},
        {"sector.noise", &b.sector[0]},
        {"sector.speech", &b.sector[1]},
        {"sector.marked_empty_by_mistake", &b.sector[2]},
        {"rations.confirm_issue", &b.rations[0]},
        {"rations.mark_untouched", &b.rations[1]},
        {"rations.request_opening", &b.rations[2]},
        {"headcount.check_list.confirm_list", &b.headcount[0][0]},
        {"headcount.check_list.mark_extra", &b.headcount[0][1]},
        {"headcount.check_list.request_identity", &b.headcount[0][2]},
        {"headcount.mark_all.confirm_list", &b.headcount[1][0]},
        {"headcount.mark_all.mark_extra", &b.headcount[1][1]},
        {"headcount.mark_all.request_identity", &b.headcount[1][2]},
        {"vitals.copy_journal", &b.vitals[0]},
        {"vitals.mark_no_signal", &b.vitals[1]},
        {"vitals.request_inspection", &b.vitals[2]},
        {"note.discard", &b.note[0]},
        {"note.copy_verbatim", &b.note[1]},
        {"note.request_cellmates", &b.note[2]},
        {"power.cut", &b.power[0]},
        {"power.mark_occupied", &b.power[1]},
        {"power.backup", &b.power[2]},
        {"seal.confirm", &b.seal[0]},
        {"seal.mismatch", &b.seal[1]},
        {"seal.scan_neighbor", &b.seal[2]},
        {"signal.none", &b.signal[0]},
        {"signal.received", &b.signal[1]},
        {"signal.respond", &b.signal[2]},
    };
    for (const EffectKey& k : keys)
        if (key == k.name) return k.field;
    return nullptr;
}

bool fail(std::string* error, int line, const std::string& what) {
    if (error) *error = "строка " + std::to_string(line) + ": " + what;
    return false;
}

} // namespace

bool Balance::loadText(const std::string& text, std::string* error) {
    Balance next = *this; // меняем копию, чтобы при ошибке ничего не испортить
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        const size_t hash = raw.find('#');
        if (hash != std::string::npos) raw.erase(hash);
        const std::string line = trim(raw);
        if (line.empty()) continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos) return fail(error, lineNo, "нет знака '='");
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        bool known = false;
        for (const IntKey& k : kIntKeys) {
            if (key != k.name) continue;
            if (!parseInt(value, &(next.*(k.field))))
                return fail(error, lineNo, "ожидалось целое число: " + value);
            known = true;
            break;
        }
        if (known) continue;

        if (key == "release.chance_percent") {
            double percent = 0;
            if (!parseDouble(value, &percent) || percent < 0 || percent > 100)
                return fail(error, lineNo, "ожидался процент от 0 до 100: " + value);
            next.releaseChance = percent / 100.0;
            continue;
        }

        if (Effect* e = findEffect(next, key)) {
            if (!parseEffect(value, e))
                return fail(error, lineNo, "ожидалось число или mid: " + value);
            continue;
        }
        return fail(error, lineNo, "неизвестный ключ: " + key);
    }

    // Проверка согласованности: пороги должны идти по возрастанию.
    const bool ordered = next.minRating <= next.badLowMax && next.badLowMax < next.confirmMax &&
                         next.confirmMax < next.normalMax && next.normalMax < next.watchMax &&
                         next.watchMax < next.maxRating;
    if (!ordered) return fail(error, lineNo, "пороги должны идти по возрастанию");
    if (next.start < next.minRating || next.start > next.maxRating)
        return fail(error, lineNo, "стартовый рейтинг вне диапазона");
    if (next.convoyStrikesToKill < 1 || next.convoyReminder < 1 || next.convoyStrikeEvery < 1 ||
        next.convoyShiftReminder < 1 || next.convoyEveningReminder < 1 || next.convoyLagSeconds < 1)
        return fail(error, lineNo, "параметры конвоя должны быть не меньше 1");
    if (next.middleStep < 1 || next.shiftEvery < 1)
        return fail(error, lineNo, "шаг к середине и период смен должны быть не меньше 1");

    *this = next;
    return true;
}

bool Balance::loadFile(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "не удалось открыть файл " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return loadText(ss.str(), error);
}

int applyEffect(const Balance& b, int rating, const Effect& e) {
    if (e.towardMiddle) {
        // Сдвиг в сторону середины, но не дальше неё: 51 -> 50, а не 49.
        const int distance = std::abs(rating - b.middle);
        const int step = std::min(b.middleStep, distance);
        rating += (rating > b.middle) ? -step : step;
    } else {
        rating += e.delta;
    }
    return std::clamp(rating, b.minRating, b.maxRating);
}

Band bandOf(const Balance& b, int rating) {
    if (rating <= b.badLowMax) return Band::BadLow;
    if (rating <= b.confirmMax) return Band::Confirm;
    if (rating <= b.normalMax) return Band::Normal;
    if (rating <= b.watchMax) return Band::Watch;
    return Band::BadHigh;
}

} // namespace core
