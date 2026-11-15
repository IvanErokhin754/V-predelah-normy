// Состояние прохождения: номер дня, скрытый рейтинг, исход утра, задания дня.
// Модуль ничего не знает о графике и вводе — его можно тестировать отдельно.
#pragma once

#include <cstdint>
#include <string>

#include "balance.h"
#include "rng.h"
#include "tasks.h"

namespace core {

// Что происходит утром. Решается один раз на рассвете и хранится в сохранении.
enum class Morning {
    Normal,        // 35–65: привычный порядок вызова
    ConfirmNumber, // 21–34: конвоир просит подтвердить номер у двери
    Watch,         // 66–79: дополнительное наблюдение
    BadLow,        // 0–20: «Рабочее назначение отменено»
    BadHigh,       // 80–100: «Вас ожидает индивидуальное собеседование»
    Release        // сработали 2%: хорошая концовка
};

class GameState {
public:
    Balance balance;

    // Начинает игру с первого дня. Рассвет первого дня тоже проверяет освобождение.
    void newGame(uint64_t seed);

    int day() const { return day_; }
    Morning morning() const { return morning_; }
    int prisonerNumber() const { return prisoner_; }
    bool isEnding() const {
        return morning_ == Morning::BadLow || morning_ == Morning::BadHigh ||
               morning_ == Morning::Release;
    }

    static const int kTaskCount = 2;
    const Task& task(int index) const { return tasks_[index]; }
    const Shift& shift() const { return shift_; }

    // Индекс первого неподтверждённого задания теста или kTaskCount, если тест пройден.
    int currentTask() const;
    bool testDone() const { return currentTask() == kTaskCount; }
    bool shiftPending() const { return shift_.kind != ShiftKind::None && !shift_.done; }
    // Обязательные задания дня: тест и, если сегодня смена, действие смены.
    bool mandatoryDone() const { return testDone() && !shiftPending(); }

    // Подтверждает ответ в позиции slot (0–2, как на экране) для задания taskIndex.
    // Возвращает false и ничего не меняет, если задание уже подтверждено,
    // очередь до него не дошла или аргументы некорректны.
    bool confirmAnswer(int taskIndex, int slot);

    // Первое подтверждённое действие завершает смену. Смена доступна после теста.
    bool confirmShiftAction(int action);

    // Утреннее подтверждение номера у двери: рейтинг не меняется.
    bool numberConfirmed() const { return numberConfirmed_; }
    void confirmNumber() { numberConfirmed_ = true; }

    // Закрывает день: маршрут следующего утра определяется здесь, один раз,
    // по итоговому рейтингу. Затем наступает рассвет нового дня.
    // Возвращает false, если обязательные задания не выполнены или игра окончена.
    bool endDay();

    // Сохранение. Текст «ключ=значение»; pack/unpack делают его нечитаемым
    // в блокноте, чтобы рейтинг не подсматривали в файле.
    std::string serialize() const;
    bool deserialize(const std::string& text);
    static std::string pack(const std::string& plain);
    static bool unpack(const std::string& packed, std::string* plain);

    // --- для режима разработчика и тестов; в обычной игре нигде не показывается ---
    int rating() const { return rating_; }
    void debugSetRating(int value);
    void debugForceRelease() { forceRelease_ = true; }
    bool debugReleaseForced() const { return forceRelease_; }
    void debugCompleteTasks(); // закрывает задания дня без начисления очков
    void debugEndDay();        // завершает день в обход проверки заданий

private:
    void dawn(Band route);
    void buildDay();

    Rng rng_{1};
    int day_ = 0;
    int rating_ = 50;
    int prisoner_ = 0;
    Morning morning_ = Morning::Normal;
    bool numberConfirmed_ = false;
    bool forceRelease_ = false;

    int prevPair_ = -1;    // связка вчерашнего дня
    int todayPair_ = -1;   // связка сегодняшнего дня
    uint64_t daySeed_ = 0; // зерно, из которого собраны задания дня
    Task tasks_[kTaskCount];
    Shift shift_;
};

} // namespace core
