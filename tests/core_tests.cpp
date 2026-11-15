// Тесты ядра правил. Без фреймворка: макрос CHECK и счётчик ошибок.
// Запуск: ./norma_tests [путь к data/balance.ini]
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/core/balance.h"
#include "../src/core/game_state.h"
#include "../src/core/tasks.h"

using namespace core;

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("  ПРОВАЛ %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        const long long va = static_cast<long long>(a);                          \
        const long long vb = static_cast<long long>(b);                          \
        if (va != vb) {                                                          \
            ++g_failed;                                                          \
            std::printf("  ПРОВАЛ %s:%d: %s == %s (%lld против %lld)\n", __FILE__, \
                        __LINE__, #a, #b, va, vb);                               \
        }                                                                        \
    } while (0)

// Слот на экране, в котором лежит ответ с данным номером строки таблицы.
static int slotOf(const Task& t, int answerId) {
    for (int i = 0; i < 3; ++i)
        if (t.answers[i].id == answerId) return i;
    return -1;
}

// Доводит игру до дня без концовки; освобождение для чистоты опытов выключено.
static GameState freshGame(uint64_t seed) {
    GameState g;
    g.balance.releaseChance = 0.0;
    g.newGame(seed);
    return g;
}

static void testTowardMiddle() {
    std::printf("к середине\n");
    Balance b;
    const Effect mid = Effect::mid();
    CHECK_EQ(applyEffect(b, 74, mid), 72);
    CHECK_EQ(applyEffect(b, 30, mid), 32);
    CHECK_EQ(applyEffect(b, 52, mid), 50);
    CHECK_EQ(applyEffect(b, 51, mid), 50); // до середины 1 очко — сдвиг на 1
    CHECK_EQ(applyEffect(b, 49, mid), 50);
    CHECK_EQ(applyEffect(b, 50, mid), 50); // на середине остаётся на месте
    CHECK_EQ(applyEffect(b, 0, mid), 2);
    CHECK_EQ(applyEffect(b, 100, mid), 98);
}

static void testClamp() {
    std::printf("границы 0–100\n");
    Balance b;
    CHECK_EQ(applyEffect(b, 98, Effect::add(+7)), 100);
    CHECK_EQ(applyEffect(b, 3, Effect::add(-8)), 0);
    CHECK_EQ(applyEffect(b, 50, Effect::add(+6)), 56);
    CHECK_EQ(applyEffect(b, 50, Effect::add(-7)), 43);
}

static void testBands() {
    std::printf("пороги\n");
    Balance b;
    CHECK(bandOf(b, 0) == Band::BadLow);
    CHECK(bandOf(b, 20) == Band::BadLow);
    CHECK(bandOf(b, 21) == Band::Confirm);
    CHECK(bandOf(b, 34) == Band::Confirm);
    CHECK(bandOf(b, 35) == Band::Normal);
    CHECK(bandOf(b, 50) == Band::Normal);
    CHECK(bandOf(b, 65) == Band::Normal);
    CHECK(bandOf(b, 66) == Band::Watch);
    CHECK(bandOf(b, 79) == Band::Watch);
    CHECK(bandOf(b, 80) == Band::BadHigh);
    CHECK(bandOf(b, 100) == Band::BadHigh);
}

static void testTables() {
    std::printf("таблицы эффектов из ТЗ\n");
    Balance b;
    // Задание А: один и тот же ответ при разных приказах.
    CHECK(b.cards[0][0] == Effect::add(-7));
    CHECK(b.cards[0][1] == Effect::mid());
    CHECK(b.cards[0][2] == Effect::add(+7));
    CHECK(b.cards[1][0] == Effect::add(-7));
    CHECK(b.cards[1][1] == Effect::add(-5));
    CHECK(b.cards[1][2] == Effect::mid());
    // Б, В и смены.
    CHECK(b.gauge[0] == Effect::add(-8) && b.gauge[1] == Effect::mid() && b.gauge[2] == Effect::add(+6));
    CHECK(b.sector[0] == Effect::add(-6) && b.sector[1] == Effect::mid() && b.sector[2] == Effect::add(+8));
    CHECK(b.seal[0] == Effect::add(-8) && b.seal[1] == Effect::mid() && b.seal[2] == Effect::add(+6));
    CHECK(b.signal[0] == Effect::add(-6) && b.signal[1] == Effect::mid() && b.signal[2] == Effect::add(+8));

    // Эффект привязан к самому ответу, а не к позиции на экране.
    Rng rng(7);
    bool sawShuffle = false;
    for (int i = 0; i < 200; ++i) {
        for (int variant = 0; variant < kVariantCount; ++variant) {
            const Task t = makeTask(b, variant, rng);
            bool seen[3] = {false, false, false};
            for (int slot = 0; slot < 3; ++slot) {
                const Answer& a = t.answers[slot];
                CHECK(a.id >= 0 && a.id <= 2);
                seen[a.id] = true;
                if (a.id != slot) sawShuffle = true;
                const Effect* table = variant == kCardsCheckNumbers   ? b.cards[0]
                                      : variant == kCardsCheckHistory ? b.cards[1]
                                      : variant == kGauge             ? b.gauge
                                                                      : b.sector;
                CHECK(a.effect == table[a.id]);
            }
            CHECK(seen[0] && seen[1] && seen[2]);
            if (variant == kGauge) {
                CHECK(t.gaugeNeedle != t.gaugeJournal);
                CHECK(t.gaugeNeedle >= 10 && t.gaugeNeedle <= 99);
                CHECK_EQ(t.gaugeNeedle % 10 * 10 + t.gaugeNeedle / 10, t.gaugeJournal);
            }
            if (t.kind == TaskKind::Cards) CHECK(t.cardSector[0] != t.cardSector[1]);
        }
    }
    CHECK(sawShuffle);
}

static void testSameAnswerDifferentOrders() {
    std::printf("одно действие при двух приказах\n");
    Balance b;
    Rng rng(11);
    const Task numbers = makeTask(b, kCardsCheckNumbers, rng);
    const Task history = makeTask(b, kCardsCheckHistory, rng);
    // «Запросить журнал переводов»: +7 против «к середине».
    const int requestLog = 2;
    CHECK_EQ(applyEffect(b, 60, numbers.answers[slotOf(numbers, requestLog)].effect), 67);
    CHECK_EQ(applyEffect(b, 60, history.answers[slotOf(history, requestLog)].effect), 58);
    // «Отметить совпадение и закрыть запрос»: «к середине» против −5.
    const int closeRequest = 1;
    CHECK_EQ(applyEffect(b, 60, numbers.answers[slotOf(numbers, closeRequest)].effect), 58);
    CHECK_EQ(applyEffect(b, 60, history.answers[slotOf(history, closeRequest)].effect), 55);
    CHECK(numbers.order != history.order);
}

// Находит день, где есть задания нужных вариантов в нужном порядке.
static bool seekDay(GameState& g, int firstVariant, int secondVariant) {
    for (int guard = 0; guard < 500; ++guard) {
        if (g.task(0).variant == firstVariant && g.task(1).variant == secondVariant &&
            g.shift().kind == ShiftKind::None)
            return true;
        g.debugEndDay();
    }
    return false;
}

static void testSpecExample() {
    std::printf("пример из раздела 7 (74 -> 81 -> 79)\n");
    GameState g = freshGame(2030);
    CHECK(seekDay(g, kCardsCheckNumbers, kGauge));
    g.debugSetRating(74);

    // Запрос журнала вне поручения: +7.
    CHECK(g.confirmAnswer(0, slotOf(g.task(0), 2)));
    CHECK_EQ(g.rating(), 81); // промежуточный выход за порог
    // Правильная сверка прибора: к середине.
    CHECK(g.confirmAnswer(1, slotOf(g.task(1), 1)));
    CHECK_EQ(g.rating(), 79);

    CHECK(g.mandatoryDone());
    CHECK(g.endDay());
    CHECK(g.morning() == Morning::Watch); // обычный маршрут, не концовка
    CHECK(!g.isEnding());
}

static void testNoDoubleScoring() {
    std::printf("повторное подтверждение не начисляет очки\n");
    GameState g = freshGame(5);
    const int before = g.rating();
    const int slot = slotOf(g.task(0), 0);
    CHECK(g.confirmAnswer(0, slot));
    const int after = g.rating();
    CHECK(after != before);
    CHECK(!g.confirmAnswer(0, slot));               // двойное нажатие
    CHECK(!g.confirmAnswer(0, (slot + 1) % 3));     // попытка переиграть ответ
    CHECK_EQ(g.rating(), after);
    CHECK(!g.confirmAnswer(1, 5));                  // некорректный слот
    CHECK(!g.confirmAnswer(2, 0));                  // нет такого задания
    CHECK_EQ(g.rating(), after);
    CHECK_EQ(g.currentTask(), 1);
    CHECK(!g.endDay()); // тест не закончен — день закрыть нельзя
    CHECK(g.confirmAnswer(1, 0));
    CHECK(g.testDone());
}

static void testShifts() {
    std::printf("рабочие смены\n");
    Balance b;
    CHECK(shiftKindForDay(b, 1) == ShiftKind::None);
    CHECK(shiftKindForDay(b, 2) == ShiftKind::None);
    CHECK(shiftKindForDay(b, 3) == ShiftKind::Seal); // первой идёт сверка пломбы
    CHECK(shiftKindForDay(b, 4) == ShiftKind::None);
    CHECK(shiftKindForDay(b, 6) == ShiftKind::Signal);
    CHECK(shiftKindForDay(b, 9) == ShiftKind::Seal);
    CHECK(shiftKindForDay(b, 12) == ShiftKind::Signal);

    GameState g = freshGame(77);
    g.debugEndDay();
    g.debugEndDay();
    CHECK_EQ(g.day(), 3);
    CHECK(g.shift().kind == ShiftKind::Seal);
    CHECK(g.shift().invoiceSeal != g.shift().crateSeal);
    CHECK_EQ(g.shift().invoiceSeal / 100, g.shift().crateSeal / 100);

    // До конца теста смена недоступна.
    CHECK(!g.confirmShiftAction(kSealConfirm));
    CHECK(g.confirmAnswer(0, slotOf(g.task(0), 1)));
    CHECK(g.confirmAnswer(1, slotOf(g.task(1), 1)));
    CHECK(g.testDone());
    CHECK(!g.mandatoryDone()); // шлюз ещё закрыт: смена обязательна
    CHECK(!g.endDay());

    g.debugSetRating(50);
    CHECK(g.confirmShiftAction(kSealScanNeighbor)); // считыватель на соседнем ящике: +6
    CHECK_EQ(g.rating(), 56);
    CHECK(!g.confirmShiftAction(kSealConfirm)); // первое действие завершило смену
    CHECK_EQ(g.rating(), 56);
    CHECK(g.mandatoryDone());

    // Смена «сигнал» на шестой день.
    g.debugEndDay();
    g.debugEndDay();
    g.debugEndDay();
    CHECK_EQ(g.day(), 6);
    CHECK(g.shift().kind == ShiftKind::Signal);
    CHECK(g.confirmAnswer(0, slotOf(g.task(0), 1)));
    CHECK(g.confirmAnswer(1, slotOf(g.task(1), 1)));
    g.debugSetRating(50);
    CHECK(g.confirmShiftAction(kSignalNone)); // «Сигнала нет»: −6
    CHECK_EQ(g.rating(), 44);
}

static void testRouteDecidedOncePerDay() {
    std::printf("маршрут решается один раз, по итогу дня\n");
    // Промежуточный выход за верхний порог обратим в течение дня.
    {
        GameState g = freshGame(31);
        CHECK(seekDay(g, kCardsCheckNumbers, kSector));
        g.debugSetRating(77);
        CHECK(g.confirmAnswer(0, slotOf(g.task(0), 2))); // +7 -> 84
        CHECK_EQ(g.rating(), 84);
        CHECK(g.confirmAnswer(1, slotOf(g.task(1), 0))); // «Помехи» −6 -> 78
        CHECK_EQ(g.rating(), 78);
        CHECK(g.endDay());
        CHECK(g.morning() == Morning::Watch);
    }
    // Итог на пороге: 80 — уже «угроза», 79 — ещё нет; 20 — «непригоден», 21 — ещё нет.
    struct Case {
        int rating;
        Morning expect;
    };
    const Case cases[] = {
        {0, Morning::BadLow},   {20, Morning::BadLow},        {21, Morning::ConfirmNumber},
        {34, Morning::ConfirmNumber}, {35, Morning::Normal},  {65, Morning::Normal},
        {66, Morning::Watch},   {79, Morning::Watch},         {80, Morning::BadHigh},
        {100, Morning::BadHigh},
    };
    for (const Case& c : cases) {
        GameState g = freshGame(900 + c.rating);
        g.debugCompleteTasks();
        g.debugSetRating(c.rating);
        CHECK(g.endDay());
        CHECK(g.morning() == c.expect);
        CHECK_EQ(g.day(), 2);
        const bool bad = c.expect == Morning::BadLow || c.expect == Morning::BadHigh;
        CHECK(g.isEnding() == bad);
        if (bad) {
            // После концовки игра не продолжается и очки не начисляются.
            const int r = g.rating();
            CHECK(!g.confirmAnswer(0, 0));
            CHECK(!g.endDay());
            CHECK_EQ(g.rating(), r);
            CHECK_EQ(g.day(), 2);
        }
    }
    // Подтверждение номера у двери рейтинг не меняет.
    {
        GameState g = freshGame(4);
        g.debugCompleteTasks();
        g.debugSetRating(30);
        CHECK(g.endDay());
        CHECK(g.morning() == Morning::ConfirmNumber);
        CHECK(!g.numberConfirmed());
        g.confirmNumber();
        CHECK(g.numberConfirmed());
        CHECK_EQ(g.rating(), 30);
    }
}

static void testPairsDoNotRepeat() {
    std::printf("связка заданий не повторяется два дня подряд\n");
    GameState g = freshGame(123);
    int prevA = -1;
    int prevB = -1;
    int variantSeen[kVariantCount] = {0, 0, 0, 0};
    for (int i = 0; i < 3000; ++i) {
        const Task& t0 = g.task(0);
        const Task& t1 = g.task(1);
        CHECK(t0.kind != t1.kind); // два задания разных типов
        int a = t0.variant;
        int b = t1.variant;
        if (a > b) std::swap(a, b); // связка — неупорядоченная пара вариантов
        CHECK(!(a == prevA && b == prevB));
        prevA = a;
        prevB = b;
        ++variantSeen[t0.variant];
        ++variantSeen[t1.variant];
        g.debugEndDay();
    }
    for (int v = 0; v < kVariantCount; ++v) CHECK(variantSeen[v] > 300);
}

static void testRelease() {
    std::printf("освобождение: 2%% на рассвете, приоритет над маршрутом\n");
    // Частота: доля игр, где освобождение выпало именно на данном рассвете.
    {
        int firstDay = 0;
        int secondDay = 0;
        const int runs = 200000;
        for (int i = 0; i < runs; ++i) {
            GameState g;
            g.newGame(1000003ull * static_cast<uint64_t>(i) + 17);
            if (g.morning() == Morning::Release) {
                ++firstDay; // проверка идёт с первого же дня
                continue;
            }
            g.debugEndDay();
            if (g.morning() == Morning::Release) ++secondDay;
        }
        const double p1 = static_cast<double>(firstDay) / runs;
        const double p2 = static_cast<double>(secondDay) / (runs - firstDay);
        std::printf("  день 1: %.4f, день 2: %.4f (ожидается 0.0200)\n", p1, p2);
        CHECK(p1 > 0.018 && p1 < 0.022);
        CHECK(p2 > 0.018 && p2 < 0.022);
    }
    // Приоритет: рейтинг ушёл в плохой диапазон, но освобождение сработало.
    {
        GameState g = freshGame(8);
        g.debugCompleteTasks();
        g.debugSetRating(5);
        g.debugForceRelease();
        CHECK(g.endDay());
        CHECK(g.morning() == Morning::Release);
        CHECK(g.isEnding());
    }
    {
        GameState g = freshGame(8);
        g.debugCompleteTasks();
        g.debugSetRating(95);
        g.debugForceRelease();
        CHECK(g.endDay());
        CHECK(g.morning() == Morning::Release);
    }
    // При шансе 0 освобождения нет никогда, при 100 — сразу.
    {
        GameState g;
        g.balance.releaseChance = 1.0;
        g.newGame(1);
        CHECK(g.morning() == Morning::Release);
    }
}

static void testSaveLoad() {
    std::printf("сохранение: исход утра и прогресс дня\n");
    GameState g = freshGame(4242);
    g.debugEndDay();
    g.debugEndDay(); // день 3, смена
    CHECK(g.confirmAnswer(0, slotOf(g.task(0), 2)));
    const std::string packed = GameState::pack(g.serialize());
    CHECK(packed.find("rating") == std::string::npos); // в файле рейтинг не читается

    std::string plain;
    CHECK(GameState::unpack(packed, &plain));
    CHECK(plain == g.serialize());
    CHECK(GameState::unpack(packed + "\n", &plain));

    GameState h;
    h.balance.releaseChance = 0.0;
    CHECK(h.deserialize(plain));
    CHECK_EQ(h.day(), g.day());
    CHECK_EQ(h.rating(), g.rating());
    CHECK(h.morning() == g.morning());
    CHECK_EQ(h.prisonerNumber(), g.prisonerNumber());
    CHECK_EQ(h.currentTask(), 1);
    CHECK(h.shift().kind == g.shift().kind);
    CHECK_EQ(h.shift().invoiceSeal, g.shift().invoiceSeal);
    for (int i = 0; i < GameState::kTaskCount; ++i) {
        CHECK_EQ(h.task(i).variant, g.task(i).variant);
        CHECK(h.task(i).order == g.task(i).order);
        for (int s = 0; s < 3; ++s) CHECK_EQ(h.task(i).answers[s].id, g.task(i).answers[s].id);
    }
    // Загрузка не даёт переиграть подтверждённый ответ.
    const int r = h.rating();
    CHECK(!h.confirmAnswer(0, 0));
    CHECK_EQ(h.rating(), r);

    // Дальше обе копии живут одинаково: загрузка не запускает новую проверку 2%.
    GameState g2 = g;
    g2.balance.releaseChance = 0.5;
    GameState h2;
    h2.balance.releaseChance = 0.5;
    CHECK(h2.deserialize(g2.serialize()));
    for (int i = 0; i < 40 && !g2.isEnding(); ++i) {
        g2.debugEndDay();
        h2.debugEndDay();
        CHECK(g2.morning() == h2.morning());
        CHECK_EQ(g2.day(), h2.day());
    }
    // Исход утра хранится в файле: повторная загрузка его не меняет.
    for (int i = 0; i < 20; ++i) {
        GameState again;
        again.balance.releaseChance = 0.5;
        CHECK(again.deserialize(g2.serialize()));
        CHECK(again.morning() == g2.morning());
        CHECK_EQ(again.day(), g2.day());
    }

    // Битые данные отклоняются.
    GameState bad;
    CHECK(!bad.deserialize("v=1\nday=abc\n"));
    CHECK(!bad.deserialize(""));
    std::string tampered = packed;
    tampered[tampered.size() - 3] = (tampered[tampered.size() - 3] == '0') ? '1' : '0';
    CHECK(!GameState::unpack(tampered, &plain));
    CHECK(!GameState::unpack("мусор", &plain));
}

static void testBalanceFile(const char* path) {
    std::printf("balance.ini\n");
    Balance b;
    std::string err;
    CHECK(b.loadText("rating.start = 40\nthreshold.watch_max = 75 # комментарий\n"
                     "gauge.accept_journal = -9\nsector.speech = mid\nrelease.chance_percent = 5\n",
                     &err));
    CHECK_EQ(b.start, 40);
    CHECK_EQ(b.watchMax, 75);
    CHECK(b.gauge[0] == Effect::add(-9));
    CHECK(b.sector[1] == Effect::mid());
    CHECK(b.releaseChance > 0.049 && b.releaseChance < 0.051);
    CHECK(bandOf(b, 76) == Band::BadHigh);

    // Ошибки: баланс остаётся прежним, сообщение называет строку.
    Balance c;
    CHECK(!c.loadText("rating.start = 40\nнеизвестно = 1\n", &err));
    CHECK(err.find("строка 2") != std::string::npos);
    CHECK_EQ(c.start, 50);
    CHECK(!c.loadText("gauge.accept_journal = много\n", &err));
    CHECK(!c.loadText("threshold.bad_low_max = 90\n", &err)); // пороги не по возрастанию
    CHECK_EQ(c.badLowMax, 20);

    if (path) {
        // Файл из поставки обязан совпадать со значениями из ТЗ.
        Balance fromFile;
        fromFile.start = -1; // убедимся, что значение действительно прочитано
        err.clear();
        CHECK(fromFile.loadFile(path, &err));
        if (!err.empty()) std::printf("  %s\n", err.c_str());
        const Balance def;
        CHECK_EQ(fromFile.start, def.start);
        CHECK_EQ(fromFile.badLowMax, def.badLowMax);
        CHECK_EQ(fromFile.confirmMax, def.confirmMax);
        CHECK_EQ(fromFile.normalMax, def.normalMax);
        CHECK_EQ(fromFile.watchMax, def.watchMax);
        CHECK_EQ(fromFile.shiftEvery, def.shiftEvery);
        CHECK(fromFile.releaseChance > 0.0199 && fromFile.releaseChance < 0.0201);
        for (int o = 0; o < 2; ++o)
            for (int a = 0; a < 3; ++a) CHECK(fromFile.cards[o][a] == def.cards[o][a]);
        for (int a = 0; a < 3; ++a) {
            CHECK(fromFile.gauge[a] == def.gauge[a]);
            CHECK(fromFile.sector[a] == def.sector[a]);
            CHECK(fromFile.seal[a] == def.seal[a]);
            CHECK(fromFile.signal[a] == def.signal[a]);
        }
    }
}

int main(int argc, char** argv) {
    testTowardMiddle();
    testClamp();
    testBands();
    testTables();
    testSameAnswerDifferentOrders();
    testSpecExample();
    testNoDoubleScoring();
    testShifts();
    testRouteDecidedOncePerDay();
    testPairsDoNotRepeat();
    testRelease();
    testSaveLoad();
    testBalanceFile(argc > 1 ? argv[1] : nullptr);

    std::printf("\nпроверок: %d, провалов: %d\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
