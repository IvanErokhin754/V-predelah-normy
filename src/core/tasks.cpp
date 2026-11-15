#include "tasks.h"

#include <cstdio>
#include <utility>

namespace core {

namespace {

const char* const kSectorLetters[] = {"А", "Б", "В", "Г", "Д", "Е", "К"};
const int kSectorLetterCount = 7;

std::string sectorName(Rng& rng) {
    return std::string(kSectorLetters[rng.range(0, kSectorLetterCount - 1)]) + "-" +
           std::to_string(rng.range(1, 9));
}

std::string twoDigits(int v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d", v);
    return buf;
}

std::string clockTime(Rng& rng) {
    return twoDigits(rng.range(0, 23)) + ":" + twoDigits(rng.range(0, 59));
}

std::string date2030(Rng& rng) {
    return twoDigits(rng.range(1, 28)) + "." + twoDigits(rng.range(1, 12)) + ".2030";
}

void setAnswers(Task& t, const char* const texts[3], const Effect effects[3], Rng& rng) {
    for (int i = 0; i < 3; ++i) t.answers[i] = Answer{i, texts[i], effects[i]};
    // Перемешиваем позиции. Эффект лежит внутри ответа и переезжает вместе с ним.
    for (int i = 2; i > 0; --i) std::swap(t.answers[i], t.answers[rng.range(0, i)]);
}

} // namespace

Task makeTask(const Balance& b, int variant, Rng& rng) {
    Task t;
    t.variant = variant;

    if (variant == kCardsCheckNumbers || variant == kCardsCheckHistory) {
        t.kind = TaskKind::Cards;
        t.order = (variant == kCardsCheckNumbers) ? "Сверьте номера" : "Проверьте историю перевода";
        t.cardNumber = std::to_string(rng.range(1000, 9999));
        t.cardSector[0] = sectorName(rng);
        do {
            t.cardSector[1] = sectorName(rng);
        } while (t.cardSector[1] == t.cardSector[0]);
        t.cardDate[0] = date2030(rng);
        t.cardDate[1] = date2030(rng);
        static const char* const texts[3] = {
            "Подтвердить обе карточки",
            "Отметить совпадение номера и закрыть запрос",
            "Запросить журнал переводов",
        };
        setAnswers(t, texts, b.cards[variant == kCardsCheckNumbers ? 0 : 1], rng);
    } else if (variant == kGauge) {
        t.kind = TaskKind::Gauge;
        t.order = "Сверьте показания и отметьте расхождение";
        struct Device {
            const char* name;
            const char* unit;
        };
        static const Device devices[] = {
            {"Манометр М-", "кПа"},
            {"Термометр Т-", "°C"},
            {"Вольтметр В-", "В"},
            {"Расходомер Р-", "л/мин"},
        };
        const Device& d = devices[rng.range(0, 3)];
        t.gaugeDevice = std::string(d.name) + std::to_string(rng.range(1, 9));
        t.gaugeUnit = d.unit;
        t.gaugeArea = "Участок " + std::to_string(rng.range(2, 9));
        // В журнале цифры переставлены: 62 на приборе, 26 в записи.
        const int tens = rng.range(2, 9);
        const int ones = rng.range(1, tens - 1);
        t.gaugeNeedle = tens * 10 + ones;
        t.gaugeJournal = ones * 10 + tens;
        if (rng.range(0, 1) == 1) std::swap(t.gaugeNeedle, t.gaugeJournal);
        static const char* const texts[3] = {
            "Принять значение из журнала",
            "Отметить расхождение и приложить показание прибора",
            "Запросить предыдущие отчёты по всем приборам участка",
        };
        setAnswers(t, texts, b.gauge, rng);
    } else {
        t.kind = TaskKind::Sector;
        t.order = "Отметьте наличие речи";
        t.sectorCode = sectorName(rng);
        t.sectorDoorTime = clockTime(rng);
        t.sectorRecTime = clockTime(rng);
        static const char* const texts[3] = {
            "Указать «Помехи»",
            "Указать «Есть речь»",
            "Указать «Сектор числится пустым ошибочно»",
        };
        setAnswers(t, texts, b.sector, rng);
    }
    return t;
}

ShiftKind shiftKindForDay(const Balance& b, int day) {
    if (day < 1 || day % b.shiftEvery != 0) return ShiftKind::None;
    const int index = day / b.shiftEvery; // 1, 2, 3, ...
    return (index % 2 == 1) ? ShiftKind::Seal : ShiftKind::Signal;
}

Shift makeShift(ShiftKind kind, Rng& rng) {
    Shift s;
    s.kind = kind;
    if (kind == ShiftKind::Seal) {
        // Три разные цифры; на ящике две последние переставлены (317 -> 371).
        const int a = rng.range(1, 9);
        int b2 = 0;
        int c = 0;
        do {
            b2 = rng.range(0, 9);
            c = rng.range(0, 9);
        } while (b2 == c);
        s.invoiceSeal = a * 100 + b2 * 10 + c;
        s.crateSeal = a * 100 + c * 10 + b2;
        s.crateNo = rng.range(10, 49);
        s.neighborNo = s.crateNo + 1;
        s.neighborSeal = rng.range(100, 999);
    }
    return s;
}

namespace {
// Все связки из двух заданий разных типов.
const int kPairs[][2] = {
    {kCardsCheckNumbers, kGauge},
    {kCardsCheckNumbers, kSector},
    {kCardsCheckHistory, kGauge},
    {kCardsCheckHistory, kSector},
    {kGauge, kSector},
};
} // namespace

int pairCount() { return static_cast<int>(sizeof(kPairs) / sizeof(kPairs[0])); }

void pairVariants(int pairIndex, int* first, int* second) {
    *first = kPairs[pairIndex][0];
    *second = kPairs[pairIndex][1];
}

} // namespace core
