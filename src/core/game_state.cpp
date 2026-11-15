#include "game_state.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>

namespace core {

namespace {

Morning morningFromBand(Band band) {
    switch (band) {
        case Band::BadLow: return Morning::BadLow;
        case Band::Confirm: return Morning::ConfirmNumber;
        case Band::Normal: return Morning::Normal;
        case Band::Watch: return Morning::Watch;
        case Band::BadHigh: return Morning::BadHigh;
    }
    return Morning::Normal;
}

} // namespace

void GameState::newGame(uint64_t seed) {
    rng_ = Rng(seed);
    day_ = 0;
    rating_ = balance.start;
    prisoner_ = rng_.range(1000, 9999);
    prevPair_ = -1;
    todayPair_ = -1;
    dawn(bandOf(balance, rating_));
}

void GameState::dawn(Band route) {
    ++day_;
    numberConfirmed_ = false;

    // Одна независимая проверка на рассвет. Освобождение важнее маршрута,
    // назначенного по вчерашней оценке.
    const bool rolled = rng_.unit() < balance.releaseChance;
    const bool release = rolled || forceRelease_;
    forceRelease_ = false;
    morning_ = release ? Morning::Release : morningFromBand(route);

    daySeed_ = rng_.next();
    buildDay();
}

void GameState::buildDay() {
    Rng r(daySeed_);

    // Связка не должна совпасть со вчерашней: выбираем из оставшихся.
    const int n = pairCount();
    int index = 0;
    if (prevPair_ < 0) {
        index = r.range(0, n - 1);
    } else {
        index = r.range(0, n - 2);
        if (index >= prevPair_) ++index;
    }
    todayPair_ = index;

    int first = 0;
    int second = 0;
    pairVariants(index, &first, &second);
    if (r.range(0, 1) == 1) std::swap(first, second);
    tasks_[0] = makeTask(balance, first, r);
    tasks_[1] = makeTask(balance, second, r);

    shift_ = makeShift(shiftKindForDay(balance, day_), r);
}

void GameState::debugSetTasks(int first, int second) {
    if (first < 0 || first >= kVariantCount || second < 0 || second >= kVariantCount) return;
    Rng r(daySeed_ ^ 0x5EEDull);
    tasks_[0] = makeTask(balance, first, r);
    tasks_[1] = makeTask(balance, second, r);
}

int GameState::currentTask() const {
    for (int i = 0; i < kTaskCount; ++i)
        if (!tasks_[i].confirmed) return i;
    return kTaskCount;
}

bool GameState::confirmAnswer(int taskIndex, int slot) {
    if (isEnding()) return false;
    if (slot < 0 || slot > 2) return false;
    // Подтверждать можно только текущее задание: уже принятые ответы
    // не редактируются, а до следующих очередь ещё не дошла.
    if (taskIndex != currentTask()) return false;

    Task& t = tasks_[taskIndex];
    const Answer& a = t.answers[slot];
    rating_ = applyEffect(balance, rating_, a.effect);
    t.confirmed = true;
    t.chosenId = a.id;
    return true;
}

bool GameState::confirmShiftAction(int action) {
    if (isEnding()) return false;
    if (action < 0 || action > 2) return false;
    if (!testDone() || !shiftPending()) return false;

    const Effect& e = shift_.kind == ShiftKind::Seal     ? balance.seal[action]
                      : shift_.kind == ShiftKind::Signal ? balance.signal[action]
                                                         : balance.power[action];
    rating_ = applyEffect(balance, rating_, e);
    shift_.done = true;
    shift_.action = action;
    return true;
}

bool GameState::endDay() {
    if (isEnding() || !mandatoryDone()) return false;
    const Band route = bandOf(balance, rating_); // решение — один раз, по итогу дня
    prevPair_ = todayPair_;
    dawn(route);
    return true;
}

void GameState::debugSetRating(int value) {
    rating_ = std::clamp(value, balance.minRating, balance.maxRating);
}

void GameState::debugCompleteTasks() {
    for (Task& t : tasks_) t.confirmed = true;
    if (shift_.kind != ShiftKind::None) shift_.done = true;
}

void GameState::debugEndDay() {
    if (isEnding()) return;
    debugCompleteTasks();
    endDay();
}

// ---------------------------------------------------------------- сохранение

std::string GameState::serialize() const {
    std::ostringstream out;
    out << "v=2\n";
    out << "rng=" << rng_.state << "\n";
    out << "day=" << day_ << "\n";
    out << "rating=" << rating_ << "\n";
    out << "prisoner=" << prisoner_ << "\n";
    out << "morning=" << static_cast<int>(morning_) << "\n";
    out << "number_confirmed=" << (numberConfirmed_ ? 1 : 0) << "\n";
    out << "force_release=" << (forceRelease_ ? 1 : 0) << "\n";
    out << "prev_pair=" << prevPair_ << "\n";
    out << "day_seed=" << daySeed_ << "\n";
    for (int i = 0; i < kTaskCount; ++i) {
        out << "task" << i << "_confirmed=" << (tasks_[i].confirmed ? 1 : 0) << "\n";
        out << "task" << i << "_chosen=" << tasks_[i].chosenId << "\n";
    }
    out << "shift_done=" << (shift_.done ? 1 : 0) << "\n";
    out << "shift_action=" << shift_.action << "\n";
    return out.str();
}

bool GameState::deserialize(const std::string& text) {
    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }

    bool ok = true;
    auto getU64 = [&](const char* key) -> uint64_t {
        auto it = kv.find(key);
        if (it == kv.end() || it->second.empty()) {
            ok = false;
            return 0;
        }
        char* end = nullptr;
        const uint64_t v = std::strtoull(it->second.c_str(), &end, 10);
        if (*end != '\0') ok = false;
        return v;
    };
    auto getInt = [&](const char* key) -> int {
        auto it = kv.find(key);
        if (it == kv.end() || it->second.empty()) {
            ok = false;
            return 0;
        }
        char* end = nullptr;
        const long v = std::strtol(it->second.c_str(), &end, 10);
        if (*end != '\0') ok = false;
        return static_cast<int>(v);
    };

    if (getInt("v") != 2) return false; // v1 — сохранения версии 0.1 с другим набором заданий
    const uint64_t rngState = getU64("rng");
    const int day = getInt("day");
    const int rating = getInt("rating");
    const int prisoner = getInt("prisoner");
    const int morning = getInt("morning");
    const int numberConfirmed = getInt("number_confirmed");
    const int forceRelease = getInt("force_release");
    const int prevPair = getInt("prev_pair");
    const uint64_t daySeed = getU64("day_seed");
    bool taskConfirmed[kTaskCount];
    int taskChosen[kTaskCount];
    for (int i = 0; i < kTaskCount; ++i) {
        const std::string prefix = "task" + std::to_string(i);
        taskConfirmed[i] = getInt((prefix + "_confirmed").c_str()) != 0;
        taskChosen[i] = getInt((prefix + "_chosen").c_str());
    }
    const bool shiftDone = getInt("shift_done") != 0;
    const int shiftAction = getInt("shift_action");

    if (!ok) return false;
    if (day < 1 || morning < 0 || morning > static_cast<int>(Morning::Release)) return false;
    if (prevPair < -1 || prevPair >= pairCount()) return false;
    if (rating < balance.minRating || rating > balance.maxRating) return false;

    rng_.state = rngState;
    day_ = day;
    rating_ = rating;
    prisoner_ = prisoner;
    morning_ = static_cast<Morning>(morning); // исход утра берём из файла, без новой проверки
    numberConfirmed_ = numberConfirmed != 0;
    forceRelease_ = forceRelease != 0;
    prevPair_ = prevPair;
    daySeed_ = daySeed;

    buildDay(); // те же задания, что были до выхода: зерно дня сохранено
    for (int i = 0; i < kTaskCount; ++i) {
        tasks_[i].confirmed = taskConfirmed[i];
        tasks_[i].chosenId = taskChosen[i];
    }
    if (shift_.kind != ShiftKind::None) {
        shift_.done = shiftDone;
        shift_.action = shiftAction;
    }
    return true;
}

namespace {

uint8_t keyByte(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<uint8_t>(s >> 24);
}

unsigned checksum(const std::string& s) {
    unsigned sum = 0;
    for (unsigned char c : s) sum = (sum * 31u + c) & 0xFFFFu;
    return sum;
}

const char kMagic[] = "NORMA1:";

} // namespace

std::string GameState::pack(const std::string& plain) {
    static const char* const hex = "0123456789abcdef";
    char head[8];
    std::snprintf(head, sizeof(head), "%04x", checksum(plain));
    std::string out = std::string(kMagic) + head + ":";
    uint32_t key = 0x5EEDu;
    for (unsigned char c : plain) {
        const uint8_t x = static_cast<uint8_t>(c ^ keyByte(key));
        out.push_back(hex[x >> 4]);
        out.push_back(hex[x & 15]);
    }
    return out;
}

bool GameState::unpack(const std::string& packed, std::string* plain) {
    const std::string magic = kMagic;
    if (packed.compare(0, magic.size(), magic) != 0) return false;
    if (packed.size() < magic.size() + 5) return false;
    const std::string sumText = packed.substr(magic.size(), 4);
    size_t pos = magic.size() + 5;

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };

    std::string out;
    uint32_t key = 0x5EEDu;
    while (pos + 1 < packed.size()) {
        const int hi = nibble(packed[pos]);
        const int lo = nibble(packed[pos + 1]);
        if (hi < 0 || lo < 0) break; // хвостовые переводы строк и пробелы
        out.push_back(static_cast<char>(static_cast<uint8_t>(hi * 16 + lo) ^ keyByte(key)));
        pos += 2;
    }
    char expect[8];
    std::snprintf(expect, sizeof(expect), "%04x", checksum(out));
    if (sumText != expect) return false;
    *plain = out;
    return true;
}

} // namespace core
