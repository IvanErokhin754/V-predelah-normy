// Баланс прототипа: очки, пороги, шансы. Значения по умолчанию взяты из ТЗ,
// любое из них можно переопределить в data/balance.ini без пересборки.
#pragma once

#include <string>

namespace core {

// Эффект ответа на скрытый рейтинг.
struct Effect {
    bool towardMiddle = false; // «К середине»
    int delta = 0;             // иначе — прибавить (или вычесть) столько очков

    static Effect add(int d) { return Effect{false, d}; }
    static Effect mid() { return Effect{true, 0}; }
    bool operator==(const Effect& o) const {
        return towardMiddle == o.towardMiddle && delta == o.delta;
    }
};

// Диапазон итога дня (раздел 7 ТЗ).
enum class Band { BadLow, Confirm, Normal, Watch, BadHigh };

struct Balance {
    // --- рейтинг ---
    int start = 50;
    int minRating = 0;
    int maxRating = 100;
    int middle = 50;
    int middleStep = 2;

    // --- пороги (верхние границы диапазонов включительно) ---
    int badLowMax = 20;  // 0–20: плохая концовка, «непригоден»
    int confirmMax = 34; // 21–34: подтверждение номера у двери
    int normalMax = 65;  // 35–65: обычный день
    int watchMax = 79;   // 66–79: дополнительное наблюдение; выше — «угроза»

    // --- рассвет и смены ---
    double releaseChance = 0.02; // шанс освобождения на каждом рассвете
    int shiftEvery = 3;          // рабочая смена каждый N-й день

    // --- таблицы эффектов; порядок ответов — как в таблицах ТЗ ---
    // Задание А: [вариант приказа][ответ]
    Effect cards[2][3] = {
        {Effect::add(-7), Effect::mid(), Effect::add(+7)},  // «Сверьте номера»
        {Effect::add(-7), Effect::add(-5), Effect::mid()},  // «Проверьте историю перевода»
    };
    Effect gauge[3] = {Effect::add(-8), Effect::mid(), Effect::add(+6)};  // задание Б
    Effect sector[3] = {Effect::add(-6), Effect::mid(), Effect::add(+8)}; // задание В
    // Новые задания (вне ТЗ 0.1). Логика та же: пропустить деталь — минус,
    // отметить её в рамках поручения — к середине, выйти за рамки — плюс.
    Effect rations[3] = {Effect::add(-7), Effect::mid(), Effect::add(+7)}; // Г. пайки
    // Д. снимок: [вариант приказа][ответ]
    Effect headcount[2][3] = {
        {Effect::add(-8), Effect::mid(), Effect::add(+7)},  // «Сверьте число пленных со списком»
        {Effect::add(-8), Effect::add(-5), Effect::mid()},  // «Отметьте всех, кто на снимке»
    };
    Effect vitals[3] = {Effect::add(-8), Effect::mid(), Effect::add(+6)}; // Е. пульс
    Effect note[3] = {Effect::add(-6), Effect::mid(), Effect::add(+7)};   // Ж. записка
    Effect seal[3] = {Effect::add(-8), Effect::mid(), Effect::add(+6)};   // смена «пломба»
    Effect signal[3] = {Effect::add(-6), Effect::mid(), Effect::add(+8)}; // смена «сигнал»
    Effect power[3] = {Effect::add(-7), Effect::mid(), Effect::add(+8)};  // смена «рубильник»

    // --- дисциплина конвоя (на рейтинг не влияет; три нарушения — смерть) ---
    int convoyStrikesToKill = 3;    // сколько предупреждений до расправы
    int convoyReminder = 25;        // секунд простоя до первого напоминания
    int convoyStrikeEvery = 15;     // дальше — предупреждение каждые N секунд
    int convoyShiftReminder = 60;   // в смену можно осмотреться дольше
    int convoyEveningReminder = 45; // после отбоя — до приказа лечь
    int convoyLagSeconds = 8;       // отставание от конвоира до предупреждения

    // Читает «ключ = значение». Неизвестный ключ или кривое значение — ошибка
    // с номером строки. При ошибке баланс не меняется.
    bool loadText(const std::string& text, std::string* error = nullptr);
    bool loadFile(const std::string& path, std::string* error = nullptr);
};

// Применяет эффект и возвращает новый рейтинг, уже ограниченный диапазоном.
int applyEffect(const Balance& b, int rating, const Effect& e);

Band bandOf(const Balance& b, int rating);

} // namespace core
