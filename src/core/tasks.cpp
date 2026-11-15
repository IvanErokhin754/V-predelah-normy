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

void setAnswers(Task& t, const std::string texts[3], const Effect effects[3], Rng& rng) {
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
        static const std::string texts[3] = {
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
        static const std::string texts[3] = {
            "Принять значение из журнала",
            "Отметить расхождение и приложить показание прибора",
            "Запросить предыдущие отчёты по всем приборам участка",
        };
        setAnswers(t, texts, b.gauge, rng);
    } else if (variant == kSector) {
        t.kind = TaskKind::Sector;
        t.order = "Отметьте наличие речи";
        t.sectorCode = sectorName(rng);
        t.sectorDoorTime = clockTime(rng);
        t.sectorRecTime = clockTime(rng);
        static const std::string texts[3] = {
            "Указать «Помехи»",
            "Указать «Есть речь»",
            "Указать «Сектор числится пустым ошибочно»",
        };
        setAnswers(t, texts, b.sector, rng);
    } else if (variant == kRations) {
        // Журнал говорит «выдано», лоток говорит, что никто не ел.
        t.kind = TaskKind::Rations;
        t.order = "Подтвердите выдачу пайков";
        t.rationCell = rng.range(2, 29);
        t.rationDays = rng.range(3, 5);
        t.rationLastDoor = date2030(rng) + " " + clockTime(rng);
        const std::string texts[3] = {
            "Подтвердить выдачу за " + std::to_string(t.rationDays) + " дн.",
            "Отметить: пайки не тронуты",
            "Запросить вскрытие камеры " + std::to_string(t.rationCell),
        };
        setAnswers(t, texts, b.rations, rng);
    } else if (variant == kHeadcountList || variant == kHeadcountMark) {
        // На снимке на одного человека больше, чем в списке. Лишний стоит
        // лицом к стене.
        t.kind = TaskKind::Headcount;
        t.order = variant == kHeadcountList ? "Сверьте число пленных со списком" : "Отметьте всех, кто на снимке";
        t.photoCamera = "НАБЛ-" + std::to_string(rng.range(10, 48));
        t.photoTime = clockTime(rng);
        t.photoListed = rng.range(3, 5);
        t.photoPeople = t.photoListed + 1;
        t.photoExtra = rng.range(0, t.photoPeople - 1);
        const std::string texts[3] = {
            "Подтвердить список: " + std::to_string(t.photoListed) + " чел.",
            "Отметить: на снимке " + std::to_string(t.photoPeople) + " чел.",
            "Запросить личность человека у стены",
        };
        setAnswers(t, texts, b.headcount[variant == kHeadcountList ? 0 : 1], rng);
    } else if (variant == kVitals) {
        // Лента пульса обрывается в прямую линию; в журнале — «стабильно».
        t.kind = TaskKind::Vitals;
        t.order = "Перенесите состояние пленного в отчёт";
        t.vitalsPrisoner = rng.range(1000, 9999);
        const int hour = rng.range(1, 5);
        t.vitalsFlatTime = twoDigits(hour) + ":" + twoDigits(rng.range(0, 59));
        t.vitalsJournalTime = twoDigits(hour + rng.range(1, 3)) + ":00";
        t.vitalsFlatAt = rng.range(45, 70);
        const std::string texts[3] = {
            "Перенести «Стабильно» из журнала",
            "Отметить: сигнала нет с " + t.vitalsFlatTime,
            "Запросить осмотр камеры пленного",
        };
        setAnswers(t, texts, b.vitals, rng);
    } else {
        // Записка, найденная в камере. Текст — дословно, без правок.
        t.kind = TaskKind::Note;
        t.order = "Перенесите текст записки в отчёт";
        static const char* const notes[] = {
            "Они не считают тех, кто молчит. Молчи.",
            "Если из крыла Д стучат — не отвечай. Я ответил.",
            "Его номер теперь на моей двери. Значит, он был здесь до меня.",
            "Норма — это не середина. Это там, где тебя не видно.",
            "Конвоир стоял у двери всю ночь. Он ждал, что я встану.",
            "Мне перестали давать пайки. Значит, меня уже списали.",
            "Не смотри ему в глаз, когда он считает.",
            "Сегодня в коридоре опять открыли дверь, которой нет на плане.",
        };
        const int count = static_cast<int>(sizeof(notes) / sizeof(notes[0]));
        t.noteText = notes[rng.range(0, count - 1)];
        t.noteCell = rng.range(2, 29);
        t.noteDay = rng.range(12, 140);
        static const std::string texts[3] = {
            "Списать записку как мусор",
            "Перенести текст дословно",
            "Запросить, кто ещё содержался в камере",
        };
        setAnswers(t, texts, b.note, rng);
    }
    return t;
}

TaskKind kindOfVariant(int variant) {
    switch (variant) {
        case kCardsCheckNumbers:
        case kCardsCheckHistory: return TaskKind::Cards;
        case kGauge: return TaskKind::Gauge;
        case kSector: return TaskKind::Sector;
        case kRations: return TaskKind::Rations;
        case kHeadcountList:
        case kHeadcountMark: return TaskKind::Headcount;
        case kVitals: return TaskKind::Vitals;
        default: return TaskKind::Note;
    }
}

ShiftKind shiftKindForDay(const Balance& b, int day) {
    if (day < 1 || day % b.shiftEvery != 0) return ShiftKind::None;
    const int index = day / b.shiftEvery; // 1, 2, 3, ...
    switch (index % 3) {
        case 1: return ShiftKind::Seal;
        case 2: return ShiftKind::Signal;
        default: return ShiftKind::Power;
    }
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
    } else if (kind == ShiftKind::Power) {
        s.powerSector = rng.range(1, 9);
        s.powerOccupied = rng.range(1, 3);
    }
    return s;
}

namespace {
// Все связки из двух заданий разных типов, в порядке номеров вариантов.
struct PairTable {
    int pairs[kVariantCount * kVariantCount][2];
    int count = 0;
    PairTable() {
        for (int a = 0; a < kVariantCount; ++a)
            for (int b = a + 1; b < kVariantCount; ++b)
                if (kindOfVariant(a) != kindOfVariant(b)) {
                    pairs[count][0] = a;
                    pairs[count][1] = b;
                    ++count;
                }
    }
};
const PairTable& pairTable() {
    static const PairTable table;
    return table;
}
} // namespace

int pairCount() { return pairTable().count; }

void pairVariants(int pairIndex, int* first, int* second) {
    *first = pairTable().pairs[pairIndex][0];
    *second = pairTable().pairs[pairIndex][1];
}

} // namespace core
