// Задания ежедневного теста (А, Б, В) и рабочие смены.
// Здесь только данные и правила; как это рисуется — дело слоя game/.
#pragma once

#include <string>

#include "balance.h"
#include "rng.h"

namespace core {

enum class TaskKind { Cards, Gauge, Sector, Rations, Headcount, Vitals, Note };

// Варианты заданий. У заданий А и Д по два варианта приказа, и это разные
// варианты: один и тот же ответ в них оценивается по-разному.
enum TaskVariant {
    kCardsCheckNumbers = 0, // А. Две карточки, «Сверьте номера»
    kCardsCheckHistory = 1, // А. Две карточки, «Проверьте историю перевода»
    kGauge = 2,             // Б. Аналоговый прибор
    kSector = 3,            // В. Пустой сектор
    kRations = 4,           // Г. Нетронутые пайки
    kHeadcountList = 5,     // Д. Лишний человек на снимке, «Сверьте число со списком»
    kHeadcountMark = 6,     // Д. Лишний человек на снимке, «Отметьте всех, кто на снимке»
    kVitals = 7,            // Е. Датчик пульса
    kNote = 8,              // Ж. Записка из камеры
    kVariantCount = 9
};

TaskKind kindOfVariant(int variant);

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

    // Материалы задания Г: пайки выданы, но не тронуты.
    int rationCell = 0;
    int rationDays = 3;
    std::string rationLastDoor; // последнее открытие двери камеры

    // Материалы задания Д: снимок камеры наблюдения.
    std::string photoCamera;
    std::string photoTime;
    int photoListed = 0;  // сколько человек в списке
    int photoPeople = 0;  // сколько на снимке (на одного больше)
    int photoExtra = 0;   // который из них лишний: стоит лицом к стене

    // Материалы задания Е: датчик пульса пленного.
    int vitalsPrisoner = 0;
    std::string vitalsFlatTime;    // с этого момента сигнала нет
    std::string vitalsJournalTime; // когда в журнале записано «стабильно»
    int vitalsFlatAt = 0;          // доля ленты (из 100), где обрывается сигнал

    // Материалы задания Ж: записка, найденная в камере.
    int noteCell = 0;
    int noteDay = 0;
    std::string noteText;

    Answer answers[3]; // уже в перемешанном порядке показа
    bool confirmed = false;
    int chosenId = -1;
};

Task makeTask(const Balance& b, int variant, Rng& rng);

enum class ShiftKind { None, Seal, Signal, Power };

// Действия смены; номера совпадают с порядком в таблице ТЗ.
enum SealAction { kSealConfirm = 0, kSealMismatch = 1, kSealScanNeighbor = 2 };
enum SignalAction { kSignalNone = 0, kSignalReceived = 1, kSignalRespond = 2 };
// Смена «Рубильник»: опустить рубильник по приказу, отметить, что сектор занят,
// или самовольно перевести сектор на резервное питание.
enum PowerAction { kPowerCut = 0, kPowerMark = 1, kPowerBackup = 2 };

struct Shift {
    ShiftKind kind = ShiftKind::None;
    int crateNo = 0;      // номер ящика из поручения
    int invoiceSeal = 0;  // номер пломбы в накладной
    int crateSeal = 0;    // номер пломбы на самом ящике
    int neighborNo = 0;   // соседний ящик (вне поручения)
    int neighborSeal = 0;
    int powerSector = 0;   // номер сектора «Д-N», который велено обесточить
    int powerOccupied = 0; // сколько человек показывает табло сектора
    bool done = false;
    int action = -1;
};

// Какая смена выпадает на данный день: каждый N-й день; по кругу
// «пломба» → «сигнал» → «рубильник», начиная со сверки пломбы.
ShiftKind shiftKindForDay(const Balance& b, int day);
Shift makeShift(ShiftKind kind, Rng& rng);

// Число допустимых связок из двух заданий разных типов.
int pairCount();
// Варианты заданий в связке с данным индексом.
void pairVariants(int pairIndex, int* first, int* second);

} // namespace core
