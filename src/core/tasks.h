// Задания ежедневного теста (А, Б, В) и рабочие смены.
// Здесь только данные и правила; как это рисуется — дело слоя game/.
#pragma once

#include <string>

#include "balance.h"
#include "rng.h"

namespace core {

enum class TaskKind { Cards, Gauge, Sector };

// Варианты заданий. У задания А два варианта приказа, и это разные варианты:
// один и тот же ответ в них оценивается по-разному.
enum TaskVariant {
    kCardsCheckNumbers = 0,
    kCardsCheckHistory = 1,
    kGauge = 2,
    kSector = 3,
    kVariantCount = 4
};

struct Answer {
    int id = 0;       // номер строки в таблице ТЗ; последствия привязаны к нему
    std::string text; // что видит игрок
    Effect effect;
};

struct Task {
    TaskKind kind = TaskKind::Cards;
    int variant = 0;
    std::string order; // текст приказа над материалами

    // Материалы задания А: две карточки с одним номером.
    std::string cardNumber;
    std::string cardSector[2];
    std::string cardDate[2];

    // Материалы задания Б: фото прибора и запись в журнале.
    std::string gaugeDevice;
    std::string gaugeArea;
    std::string gaugeUnit;
    int gaugeNeedle = 0;
    int gaugeJournal = 0;

    // Материалы задания В: «пустой» сектор.
    std::string sectorCode;
    std::string sectorDoorTime;
    std::string sectorRecTime;

    Answer answers[3]; // уже в перемешанном порядке показа
    bool confirmed = false;
    int chosenId = -1;
};

Task makeTask(const Balance& b, int variant, Rng& rng);

enum class ShiftKind { None, Seal, Signal };

// Действия смены; номера совпадают с порядком в таблице ТЗ.
enum SealAction { kSealConfirm = 0, kSealMismatch = 1, kSealScanNeighbor = 2 };
enum SignalAction { kSignalNone = 0, kSignalReceived = 1, kSignalRespond = 2 };

struct Shift {
    ShiftKind kind = ShiftKind::None;
    int crateNo = 0;      // номер ящика из поручения
    int invoiceSeal = 0;  // номер пломбы в накладной
    int crateSeal = 0;    // номер пломбы на самом ящике
    int neighborNo = 0;   // соседний ящик (вне поручения)
    int neighborSeal = 0;
    bool done = false;
    int action = -1;
};

// Какая смена выпадает на данный день: каждый N-й день, начиная со сверки пломбы.
ShiftKind shiftKindForDay(const Balance& b, int day);
Shift makeShift(ShiftKind kind, Rng& rng);

// Число допустимых связок из двух заданий разных типов.
int pairCount();
// Варианты заданий в связке с данным индексом.
void pairVariants(int pairIndex, int* first, int* second);

} // namespace core
