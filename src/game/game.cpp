#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

#include "../engine/sdl_min.h"

namespace game {

namespace {

const float kEyeHeight = 1.65f;
const float kPlayerRadius = 0.28f;
const float kWalkSpeed = 2.3f;
const float kReach = 2.0f;
const float kFov = 70.0f * kPi / 180.0f;

const Vec3 kWingDoorPos{7.1f, 1.2f, 3.0f};
const Vec3 kMonitorPos{12.5f, 0.95f, 1.5f};
const Vec3 kCellLampPos{6.5f, 2.9f, 12.8f};

const char* const kPa = "ГРОМКАЯ СВЯЗЬ";
const char* const kRobot = "КОНВОИР";
const char* const kSpeaker = "ДИНАМИК";
const char* const kHuman = "ЧЕЛОВЕК";

bool readFile(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << data;
    return static_cast<bool>(f);
}

std::string onOff(bool v) { return v ? "вкл" : "выкл"; }

const char* morningName(core::Morning m) {
    switch (m) {
        case core::Morning::Normal: return "Normal";
        case core::Morning::ConfirmNumber: return "ConfirmNumber";
        case core::Morning::Watch: return "Watch";
        case core::Morning::BadLow: return "BadLow";
        case core::Morning::BadHigh: return "BadHigh";
        case core::Morning::Release: return "Release";
    }
    return "?";
}

} // namespace

// ============================================================ запуск и файлы

bool Game::init(const Options& options, std::string* error) {
    options_ = options;

    // Баланс: файл не обязателен, но если он есть и с ошибкой — сообщаем.
    const std::string balancePath = options_.dataDir + "balance.ini";
    std::string text;
    if (readFile(balancePath, &text)) {
        std::string err;
        if (!state_.balance.loadText(text, &err)) {
            notice_ = "balance.ini: " + err + " — взяты значения по умолчанию";
            std::fprintf(stderr, "%s\n", notice_.c_str());
        }
    }

    if (!renderer_.init(error)) return false;
    screenTarget_ = renderer_.makeTarget(Terminal::kWidth, Terminal::kHeight);
    audio_.init(); // без звукового устройства игра просто молчит
    loadSettings();
    audio_.setMasterVolume(settings_.volume);

    // Эмбиент: необязательный файл data/audio/ambient.wav.
    {
        const std::string musicPath = options_.dataDir + "audio/ambient.wav";
        std::ifstream probe(musicPath, std::ios::binary);
        if (probe) {
            std::string err;
            if (!audio_.loadMusic(musicPath, &err)) {
                notice_ = "Эмбиент: " + err;
                std::fprintf(stderr, "%s\n", notice_.c_str());
            }
        }
    }

    level_.build(0);
    level_.setScreenTexture(screenTarget_.color);
    screen_ = Screen::MainMenu;
    menuIndex_ = 0;
    return true;
}

void Game::shutdown() { audio_.shutdown(); }

bool Game::saveExists() const {
    if (options_.noSave) return false;
    std::ifstream f(options_.saveDir + "save.dat", std::ios::binary);
    return static_cast<bool>(f);
}

void Game::saveGame() {
    if (options_.noSave) return;
    writeFile(options_.saveDir + "save.dat", core::GameState::pack(state_.serialize()));
}

void Game::eraseSave() {
    if (options_.noSave) return;
    std::remove((options_.saveDir + "save.dat").c_str());
}

void Game::loadSettings() {
    if (options_.noSave) return;
    std::string text;
    if (!readFile(options_.saveDir + "settings.ini", &text)) return;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const float value = static_cast<float>(std::atof(line.c_str() + eq + 1));
        if (key == "ripple") settings_.ripple = value != 0;
        if (key == "typewriter") settings_.typewriter = value != 0;
        if (key == "mouse") settings_.mouse = std::clamp(value, 0.2f, 3.0f);
        if (key == "volume") settings_.volume = std::clamp(value, 0.0f, 1.0f);
        if (key == "music") settings_.music = std::clamp(value, 0.0f, 1.0f);
        if (key == "fullscreen") settings_.fullscreen = value != 0;
    }
}

void Game::storeSettings() {
    if (options_.noSave) return;
    std::ostringstream out;
    out << "ripple=" << (settings_.ripple ? 1 : 0) << "\n";
    out << "typewriter=" << (settings_.typewriter ? 1 : 0) << "\n";
    out << "mouse=" << settings_.mouse << "\n";
    out << "volume=" << settings_.volume << "\n";
    out << "music=" << settings_.music << "\n";
    out << "fullscreen=" << (settings_.fullscreen ? 1 : 0) << "\n";
    writeFile(options_.saveDir + "settings.ini", out.str());
}

void Game::startNewGame() {
    uint64_t seed = options_.seed;
    if (seed == 0)
        seed = static_cast<uint64_t>(std::time(nullptr)) * 1000003ull ^ SDL_GetPerformanceCounter();
    state_.newGame(seed);
    screen_ = Screen::Playing;
    startDay();
}

bool Game::continueGame() {
    std::string packed, plain;
    if (!readFile(options_.saveDir + "save.dat", &packed)) return false;
    if (!core::GameState::unpack(packed, &plain) || !state_.deserialize(plain)) {
        notice_ = "Сохранение повреждено или осталось от прошлой версии";
        return false;
    }
    screen_ = Screen::Playing;
    startDay();
    return true;
}

// ================================================================ цикл дня

void Game::startDay() {
    level_.setPlate(state_.prisonerNumber());
    level_.setShiftTextures(state_.shift());
    terminal_.reset();

    playerPos_ = spot::kBunkWake;
    yaw_ = 0; // лицом к двери (на север)
    pitch_ = 0;
    monitorMode_ = false;
    monitorBlend_ = 0;
    aimed_ = UseNone;

    for (Door& d : level_.doors) d.open = d.target = 0;
    level_.objects[ObjDaylight].visible = false;
    level_.objects[ObjOuterDoor].visible = true;
    level_.objects[ObjInvoice].visible = state_.shift().kind == core::ShiftKind::Seal;

    escort_ = Escort{};
    switch (state_.morning()) {
        case core::Morning::Watch:
            // Конвоир стоит прямо в дверях и смотрит в камеру.
            escortMode_ = EscortMode::Watch;
            escort_.pos = {6.5f, 0, 9.2f};
            escort_.yaw = kPi;
            break;
        case core::Morning::BadLow:
        case core::Morning::BadHigh:
            escortMode_ = EscortMode::Bad;
            escort_.pos = spot::kEscortAtCell;
            escort_.yaw = -kPi * 0.5f;
            break;
        case core::Morning::Release:
            // Комплекс обесточен: конвоир замер посреди коридора.
            escortMode_ = EscortMode::Off;
            escort_.pos = {9.6f, 0, 8.6f};
            escort_.yaw = 1.9f;
            break;
        default:
            escortMode_ = EscortMode::Normal;
            escort_.pos = spot::kEscortAtCell;
            escort_.yaw = -kPi * 0.5f;
            break;
    }

    lines_.clear();
    lineTime_ = 0;
    lineVoiceDelay_ = -1;
    shiftAnnounced_ = false;
    tasksDoneTimer_ = 0;
    knockTimer_ = 0;
    extraKnock_ = -1;
    replyKnock_ = -1;
    humanVisible_ = false;
    silent_ = false;
    white_ = 0;

    strikes_ = 0;
    idle_ = 0;
    idleStage_ = 0;
    idleRuleId_ = -1;
    lagTime_ = 0;
    lagStage_ = 0;
    escortWaiting_ = false;
    zoneTime_ = 0;
    bumpTime_ = 0;
    bumpReminded_ = false;
    strikeFlash_ = 0;
    eyeFlare_ = 0;
    strikeCooldown_ = 0;
    killStep_ = 0;
    killTime_ = 0;
    shake_ = 0;
    nightLights_ = false;
    lever_ = leverTarget_ = 0;
    level_.setLever(0);
    sectorDark_ = 0;
    cutKnock_ = -1;
    cutKnocks_ = 0;
    paTimer_ = 35.0f + static_cast<float>(state_.day() % 4) * 9.0f;
    fade_ = 1;
    fadeTarget_ = 0;
    fadeSpeed_ = 0.5f;
    dayCard_ = 4.0f;

    enterPhase(Phase::Dawn);
    saveGame(); // исход утра уже выбран и теперь хранится вместе с номером дня
}

void Game::enterPhase(Phase p) {
    phase_ = p;
    phaseTime_ = 0;
    phaseStep_ = 0;
}

void Game::setDoor(int id, bool open, bool sound) {
    Door& d = level_.doors[id];
    const float target = open ? 1.0f : 0.0f;
    if (d.target == target) return;
    d.target = target;
    if (sound) playWorld(d.heavy ? audio::HeavyDoor : audio::DoorSlide, d.base + Vec3{0, 1.2f, 0}, d.heavy ? 2.2f : 1.6f);
}

float Game::muffleFor(const Vec3& pos) const {
    // Отрезок от уха до источника, без последних 30 см: дверь, которая сама
    // издаёт звук, не должна глушить сама себя.
    const Vec3 ear = cameraEye();
    Vec3 d = pos - ear;
    const float len = length(d);
    if (len < 0.4f) return 0.0f;
    d = d * ((len - 0.3f) / len);
    return level_.lineClear(ear, ear + d) ? 0.0f : 1.0f;
}

void Game::playWorld(int sound, const Vec3& pos, float gain) {
    audio_.playAt(static_cast<audio::Sound>(sound), pos, gain, muffleFor(pos));
}

void Game::setEscortPath(const std::vector<Vec3>& path, bool leading) {
    escort_.path = path;
    escort_.next = 0;
    escort_.leading = leading;
    escort_.waitDoor = -1;
    escort_.nag = 6.0f;
}

void Game::say(const std::string& speaker, const std::string& text, float seconds, int sound) {
    lines_.push_back(Line{speaker, text, seconds, sound});
}

bool Game::controlLocked() const {
    if (monitorMode_ || monitorBlend_ > 0.02f) return true;
    if (phase_ == Phase::Sleep || phase_ == Phase::WhiteOut || phase_ == Phase::Kill) return true;
    if (phase_ == Phase::BadEnd && phaseStep_ >= 1) return true;
    return false;
}

void Game::updatePhase(float dt) {
    phaseTime_ += dt;
    const core::Morning morning = state_.morning();
    const std::string number = std::to_string(state_.prisonerNumber());
    const bool playerInCell = playerPos_.x > 5 && playerPos_.x < 8 && playerPos_.z > 11.3f;
    const bool playerInTestRoom = playerPos_.z < 5.7f && playerPos_.x > 7 && playerPos_.x < 16;

    switch (phase_) {
        case Phase::Dawn:
            if (phaseTime_ > 2.4f) enterPhase(Phase::MorningCall);
            break;

        case Phase::MorningCall:
            if (phaseStep_ == 0) {
                if (morning == core::Morning::Release)
                    say(kPa, "Номер " + number + ". Подъём. К две—", 1.9f, audio::PaChime);
                else if (morning == core::Morning::ConfirmNumber)
                    say(kPa, "Номер " + number + ". Подъём.", 2.6f, audio::PaChime);
                else
                    say(kPa, "Номер " + number + ". Подъём. К двери.", 3.0f, audio::PaChime);
                phaseStep_ = 1;
            }
            if (morning == core::Morning::Release) {
                // Обычная команда обрывается; дверь открывает человек.
                if (phaseStep_ == 1 && phaseTime_ > 1.9f) {
                    lines_.clear();
                    lineVoiceDelay_ = -1;
                    audio_.play(audio::Static, 0.8f);
                    phaseStep_ = 2;
                }
                if (phaseStep_ == 2 && phaseTime_ > 6.5f) {
                    playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                    phaseStep_ = 3;
                }
                if (phaseStep_ == 3 && phaseTime_ > 7.4f) {
                    setDoor(DoorCell, true);
                    setDoor(DoorExit, true);
                    humanVisible_ = true;
                    humanPos_ = {6.5f, 0, 8.7f};
                    humanYaw_ = kPi;
                    level_.objects[ObjDaylight].visible = true;
                    level_.objects[ObjOuterDoor].visible = false;
                    say(kHuman, "Эй. Живой?", 2.4f, audio::Voice);
                    say(kHuman, "Комплекс обесточен. Конвоиры стоят.", 3.2f, audio::Voice);
                    say(kHuman, "Выходи. Наружная дверь открыта — иди на свет.", 4.5f, audio::Voice);
                    enterPhase(Phase::FreeWalk);
                }
            } else if (morning == core::Morning::ConfirmNumber) {
                if (phaseStep_ == 1 && phaseTime_ > 3.0f) {
                    if (!state_.numberConfirmed())
                        say(kRobot, "Подтвердите номер у двери.", 3.5f, audio::PaVoice);
                    phaseStep_ = 2;
                }
                if (phaseStep_ == 2 && state_.numberConfirmed()) {
                    say(kRobot, "Номер подтверждён. Следуйте за конвоиром.", 3.5f, audio::PaVoice);
                    playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                    setDoor(DoorCell, true);
                    enterPhase(Phase::EscortOut);
                }
            } else if (morning == core::Morning::Watch) {
                if (phaseStep_ == 1 && phaseTime_ > 3.4f) {
                    playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                    setDoor(DoorCell, true);
                    phaseStep_ = 2;
                }
                // Конвоир задерживается у камеры, прежде чем заговорить.
                if (phaseStep_ == 2 && phaseTime_ > 8.0f) {
                    say(kRobot, "Номер " + number + ". Назначено дополнительное наблюдение.", 4.0f,
                        audio::PaVoice);
                    phaseStep_ = 3;
                }
                if (phaseStep_ == 3 && phaseTime_ > 12.5f) {
                    say(kRobot, "Следуйте за конвоиром.", 2.8f, audio::PaVoice);
                    setEscortPath({spot::kEscortAtCell}, false);
                    enterPhase(Phase::EscortOut);
                }
            } else {
                if (phaseStep_ == 1 && phaseTime_ > 3.2f) {
                    playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                    setDoor(DoorCell, true);
                    say(kRobot,
                        escortMode_ == EscortMode::Bad ? "Следуйте за конвоиром."
                                                       : "На проверку. Следуйте за конвоиром.",
                        3.2f, audio::PaVoice);
                    enterPhase(Phase::EscortOut);
                }
            }
            break;

        case Phase::EscortOut: {
            const bool bad = escortMode_ == EscortMode::Bad;
            if (phaseStep_ == 0) {
                // Конвоир трогается, когда игрок вышел из камеры.
                const bool escortReady = escort_.next >= escort_.path.size();
                if (playerPos_.z < 9.9f && escortReady) {
                    if (bad) {
                        setEscortPath({spot::kFork}, true);
                    } else {
                        setDoor(DoorGate, true);
                        setEscortPath({spot::kFork, spot::kTestNeck, {12.5f, 0, 5.6f}, spot::kEscortPost},
                                      true);
                    }
                    phaseStep_ = 1;
                }
            } else if (phaseStep_ == 1) {
                if (bad) {
                    // На развилке конвоир поворачивает в другую сторону.
                    if (escort_.next >= escort_.path.size()) {
                        say(kPa,
                            morning == core::Morning::BadLow ? "Рабочее назначение отменено."
                                                             : "Вас ожидает индивидуальное собеседование.",
                            4.5f, audio::PaChime);
                        setDoor(DoorBad, true); // новый звук двери
                        setEscortPath({spot::kBadNeck, {12.5f, 0, 12.4f}, {11.6f, 0, 12.7f}, {11.6f, 0, 14.2f}},
                                      true);
                        escort_.waitDoor = DoorBad;
                        phaseStep_ = 2;
                    }
                } else if (playerInTestRoom && escort_.pos.z < 5.8f) {
                    setDoor(DoorGate, false); // шлюз закрывается за спиной
                    say(kRobot, "Займите место у терминала.", 3.0f, audio::PaVoice);
                    tasksDoneTimer_ = 0;
                    enterPhase(Phase::AtTest);
                }
            } else if (phaseStep_ == 2) {
                const bool inBadRoom = playerPos_.z > 12.3f && playerPos_.x > 11 && playerPos_.x < 14;
                if (inBadRoom) {
                    setDoor(DoorBad, false);
                    enterPhase(Phase::BadEnd);
                }
            }
            break;
        }

        case Phase::AtTest: {
            if (terminal_.takeConfirmEvent()) {
                saveGame();
                tasksDoneTimer_ = 0;
                idleRuleId_ = -1; // ответ принят — простой считается заново
            }
            const core::Shift& shift = state_.shift();
            if (state_.testDone() && state_.shiftPending() && !shiftAnnounced_) {
                // Рабочее событие начинается после теста.
                tasksDoneTimer_ += dt;
                if (tasksDoneTimer_ > 1.6f) {
                    if (shift.kind == core::ShiftKind::Seal)
                        say(kSpeaker,
                            "Рабочая смена. Сверьте пломбу ящика № " + std::to_string(shift.crateNo) +
                                " с накладной. Только этот ящик.",
                            6.0f, audio::PaChime);
                    else if (shift.kind == core::ShiftKind::Signal)
                        say(kSpeaker, "Рабочая смена. Пустое крыло. Подтвердите, слышен ли сигнал.", 5.5f,
                            audio::PaChime);
                    else
                        say(kSpeaker,
                            "Рабочая смена. Плановое отключение. Обесточьте сектор Д-" +
                                std::to_string(shift.powerSector) + " рубильником на северной стене.",
                            6.5f, audio::PaChime);
                    shiftAnnounced_ = true;
                    knockTimer_ = 2.5f;
                    tasksDoneTimer_ = 0;
                }
            }
            if (shiftAnnounced_ && state_.shiftPending() && shift.kind == core::ShiftKind::Signal) {
                knockTimer_ -= dt;
                if (knockTimer_ <= 0) {
                    playWorld(audio::Knock, kWingDoorPos, 2.4f); // кто-то дважды стучит
                    knockTimer_ = 6.5f;
                }
            }
            if (state_.mandatoryDone()) {
                tasksDoneTimer_ += dt;
                if (tasksDoneTimer_ > 1.8f && lines_.empty()) {
                    say(kSpeaker, "Задания дня приняты. Возвращайтесь в камеру.", 3.8f, audio::PaChime);
                    setDoor(DoorGate, true); // шлюз открывается только теперь
                    // Дверь камеры открывается, когда игрок подходит к ней, а не сейчас:
                    // иначе её звук долетает до помещения для тестов «ниоткуда».
                    setEscortPath({{12.5f, 0, 5.6f}, spot::kTestNeck, spot::kFork, spot::kEscortAtCell}, true);
                    enterPhase(Phase::EscortBack);
                }
            }
            break;
        }

        case Phase::EscortBack:
            // Дверь камеры открывается у игрока на глазах: он уже в коридоре.
            if (playerPos_.z > 7.6f && playerPos_.x < 10.5f) {
                if (level_.doors[DoorCell].target < 1.0f)
                    playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                setDoor(DoorCell, true);
            }
            if (playerInCell && level_.doors[DoorCell].open > 0.5f) {
                setDoor(DoorCell, false);
                enterPhase(Phase::Evening);
            }
            break;

        case Phase::Evening:
            if (phaseStep_ == 0 && level_.doors[DoorCell].open <= 0.0f) {
                playWorld(audio::LockClick, {6.5f, 1.2f, 10.5f}, 2.0f);
                // Шлюз в дальнем конце коридора закрывается беззвучно: из камеры
                // его не видно, а звук двери без видимой двери сбивает с толку.
                setDoor(DoorGate, false, false);
                phaseStep_ = 1;
            }
            // Отбой: объявление и почти полная темнота в камере.
            if (phaseStep_ == 1 && phaseTime_ > 9.0f) {
                say(kPa, "Отбой. Свет в камерах отключается.", 3.5f, audio::PaChime);
                nightLights_ = true;
                phaseStep_ = 2;
            }
            break;

        case Phase::Sleep:
            if (fade_ >= 1.0f && phaseTime_ > 1.8f) {
                // Здесь, один раз, решается маршрут следующего утра.
                state_.endDay();
                startDay();
            }
            break;

        case Phase::BadEnd:
            if (phaseStep_ == 0 && (level_.doors[DoorBad].open <= 0.0f || phaseTime_ > 3.2f)) {
                // Дверь закрылась — звук обрывается, экран темнеет.
                audio_.cutAll();
                silent_ = true;
                lines_.clear();
                fadeTarget_ = 1;
                fadeSpeed_ = 0.5f;
                phaseStep_ = 1;
                phaseTime_ = 0;
            }
            if (phaseStep_ == 1 && phaseTime_ > 4.0f)
                finishRun(morning == core::Morning::BadLow ? Ending::BadLow : Ending::BadHigh);
            break;

        case Phase::FreeWalk:
            if (playerPos_.x < 2.4f && playerPos_.z > 9 && playerPos_.z < 10) enterPhase(Phase::WhiteOut);
            break;

        case Phase::WhiteOut:
            white_ = std::min(1.0f, white_ + dt * 0.6f);
            if (phaseTime_ > 2.6f) finishRun(Ending::Good);
            break;

        case Phase::Kill:
            updateKill(dt);
            break;
    }
}

void Game::finishRun(Ending ending) {
    endingGood_ = ending == Ending::Good;
    endingLow_ = ending == Ending::BadLow;
    endingKilled_ = ending == Ending::Killed;
    endingDay_ = state_.day();
    endingTime_ = 0;
    screen_ = Screen::Ending;
    monitorMode_ = false;
    lines_.clear();
    eraseSave(); // прохождение закончено
}

// ============================================================ взаимодействие

bool Game::canUse(InteractId id) const {
    const core::Shift& shift = state_.shift();
    // Оборудование смен доступно под рукой всё время проверки. Нажать не то
    // или не тогда — нарушение: конвоир выносит предупреждение (см. interact).
    // После своей смены её кнопки молчат: повторное нажатие ничего не значит.
    auto equipment = [&](core::ShiftKind kind) {
        if (phase_ != Phase::AtTest) return false;
        return !(shift.kind == kind && shift.done);
    };
    switch (id) {
        case UseTerminal: return phase_ == Phase::AtTest;
        case UseBunk: return phase_ == Phase::Evening;
        case UsePlate:
            return phase_ == Phase::MorningCall && state_.morning() == core::Morning::ConfirmNumber &&
                   phaseStep_ >= 2 && !state_.numberConfirmed();
        case UseSealConfirm:
        case UseSealMismatch:
        case UseSealReader: return equipment(core::ShiftKind::Seal);
        case UseSignalNone:
        case UseSignalReceived:
        case UseSignalRespond: return equipment(core::ShiftKind::Signal);
        case UsePowerLever:
        case UsePowerMark:
        case UsePowerBackup: return equipment(core::ShiftKind::Power);
        default: return false;
    }
}

std::string Game::promptFor(InteractId id) const {
    switch (id) {
        case UseTerminal: return "[E] Терминал";
        case UseBunk: return "[E] Лечь на койку";
        case UsePlate: return "[E] Подтвердить номер";
        case UseSealConfirm: return "[E] Нажать «Подтвердить»";
        case UseSealMismatch: return "[E] Нажать «Расхождение»";
        case UseSealReader:
            return "[E] Считать пломбу ящика № " + std::to_string(state_.shift().neighborNo);
        case UseSignalNone: return "[E] Нажать «Сигнала нет»";
        case UseSignalReceived: return "[E] Нажать «Сигнал принят»";
        case UseSignalRespond: return "[E] Запросить связь и ответить на стук";
        case UsePowerLever: return "[E] Опустить рубильник";
        case UsePowerMark: return "[E] Нажать «Сектор занят»";
        case UsePowerBackup: return "[E] Нажать «Резерв»";
        default: return "";
    }
}

void Game::interact() {
    if (controlLocked() || aimed_ == UseNone || !canUse(aimed_)) return;
    switch (aimed_) {
        case UseTerminal:
            monitorMode_ = true; // камера подъезжает к экрану
            break;
        case UseBunk:
            fadeTarget_ = 1;
            fadeSpeed_ = 0.7f;
            enterPhase(Phase::Sleep);
            break;
        case UsePlate:
            state_.confirmNumber(); // рейтинг остаётся прежним
            audio_.play(audio::Button, 0.8f);
            saveGame();
            break;
        default: {
            // Оборудование смен: годится только то, что велели, и только когда велели.
            struct Map {
                InteractId use;
                core::ShiftKind kind;
                int action;
            };
            static const Map map[] = {
                {UseSealConfirm, core::ShiftKind::Seal, core::kSealConfirm},
                {UseSealMismatch, core::ShiftKind::Seal, core::kSealMismatch},
                {UseSealReader, core::ShiftKind::Seal, core::kSealScanNeighbor},
                {UseSignalNone, core::ShiftKind::Signal, core::kSignalNone},
                {UseSignalReceived, core::ShiftKind::Signal, core::kSignalReceived},
                {UseSignalRespond, core::ShiftKind::Signal, core::kSignalRespond},
                {UsePowerLever, core::ShiftKind::Power, core::kPowerCut},
                {UsePowerMark, core::ShiftKind::Power, core::kPowerMark},
                {UsePowerBackup, core::ShiftKind::Power, core::kPowerBackup},
            };
            for (const Map& m : map) {
                if (m.use != aimed_) continue;
                const bool allowed = shiftAnnounced_ && state_.shiftPending() && state_.shift().kind == m.kind;
                if (allowed) {
                    doShiftAction(m.action);
                } else {
                    audio_.play(audio::Button, 0.6f);
                    warn("Не трогайте оборудование без распоряжения.");
                }
                break;
            }
            break;
        }
    }
}

void Game::doShiftAction(int action) {
    const core::ShiftKind kind = state_.shift().kind;
    // Первое подтверждённое действие завершает задание; ядро отклонит повтор.
    if (!state_.confirmShiftAction(action)) return;
    saveGame();
    tasksDoneTimer_ = 0;
    lines_.clear(); // распоряжение больше не нужно: сразу отчёт
    lineTime_ = 0;

    if (kind == core::ShiftKind::Seal) {
        if (action == core::kSealScanNeighbor) {
            audio_.play(audio::ReaderBeep, 0.8f);
            say(kSpeaker, "Отчёт смены: считана пломба ящика № " + std::to_string(state_.shift().neighborNo) + ".",
                3.5f);
        } else {
            audio_.play(audio::Button, 0.9f);
            say(kSpeaker,
                action == core::kSealConfirm ? "Отчёт смены: пломба подтверждена."
                                             : "Отчёт смены: расхождение отмечено.",
                3.5f);
        }
    } else if (kind == core::ShiftKind::Power) {
        const std::string sector = "Д-" + std::to_string(state_.shift().powerSector);
        if (action == core::kPowerCut) {
            // Рубильник опущен. Сектор гаснет, и за дверью крыла кто-то бьёт в дверь.
            leverTarget_ = 1;
            playWorld(audio::Lever, {9.7f, 1.3f, 1.2f}, 1.6f);
            playWorld(audio::PowerDown, kWingDoorPos, 2.0f);
            cutKnock_ = 2.4f;
            cutKnocks_ = 0;
            say(kSpeaker, "Отчёт смены: сектор " + sector + " обесточен.", 3.5f);
        } else if (action == core::kPowerMark) {
            audio_.play(audio::Button, 0.9f);
            say(kSpeaker, "Отчёт смены: сектор " + sector + " отмечен как занятый. Отключение отложено.", 4.5f);
        } else {
            audio_.play(audio::Button, 0.9f);
            audio_.play(audio::Static, 0.3f);
            eyeFlare_ = 2.5f; // конвоир поворачивается и смотрит — молча
            say(kSpeaker, "Отчёт смены: сектор " + sector + " переведён на резервное питание.", 4.0f);
        }
    } else {
        audio_.play(audio::Button, 0.9f);
        if (action == core::kSignalNone) {
            // Стук остаётся вне отчёта смены — и раздаётся ещё раз.
            say(kSpeaker, "Отчёт смены: сигналов нет.", 3.5f);
            extraKnock_ = 2.6f;
        } else if (action == core::kSignalReceived) {
            say(kSpeaker, "Отчёт смены: сигнал принят.", 3.5f);
        } else {
            audio_.play(audio::Static, 0.35f);
            playWorld(audio::Knock, playerPos_ + Vec3{0, 1.2f, 0}, 1.4f); // ответ игрока
            replyKnock_ = 1.8f;
            say(kSpeaker, "Отчёт смены: открыт канал связи с крылом.", 4.0f);
        }
    }
}


// ======================================================== дисциплина конвоя

void Game::remind(const std::string& text) {
    lines_.clear();
    lineTime_ = 0;
    say(kRobot, text, 3.2f, audio::PaVoice);
}

void Game::warn(const std::string& text) {
    if (strikeCooldown_ > 0 || phase_ == Phase::Kill) return;
    strikeCooldown_ = 2.5f;
    ++strikes_;
    const int limit = state_.balance.convoyStrikesToKill;
    if (strikes_ >= limit) {
        startKill();
        return;
    }
    const std::string label = strikes_ == limit - 1 ? "Последнее предупреждение." : "Предупреждение.";
    lines_.clear();
    lineTime_ = 0;
    say(kRobot, text + " " + label, 4.0f, audio::PaVoice);
    audio_.play(audio::Alarm, 0.55f);
    strikeFlash_ = 1.0f;
    eyeFlare_ = 2.5f;
}

void Game::idleRule(float dt, float reminder, const std::string& remindText, const std::string& warnText) {
    idle_ += dt;
    const float every = static_cast<float>(state_.balance.convoyStrikeEvery);
    if (idleStage_ == 0) {
        if (idle_ >= reminder) {
            remind(remindText);
            idleStage_ = 1;
        }
    } else if (idle_ >= reminder + every * static_cast<float>(idleStage_)) {
        warn(warnText);
        ++idleStage_;
    }
}

void Game::updateDiscipline(float dt) {
    if (strikeCooldown_ > 0) strikeCooldown_ -= dt;
    if (phase_ == Phase::Kill || escortMode_ == EscortMode::Off || screen_ != Screen::Playing) return;
    const core::Balance& b = state_.balance;
    const bool escorting = phase_ == Phase::EscortOut || phase_ == Phase::EscortBack;

    // 1. Отставание: конвоир стоит и ждёт.
    if (escorting && escortWaiting_) {
        lagTime_ += dt;
        if (lagStage_ == 0 && lagTime_ > 3.0f) {
            remind("Не отставайте.");
            lagStage_ = 1;
        } else if (lagStage_ >= 1 && lagTime_ > 3.0f + static_cast<float>(b.convoyLagSeconds * lagStage_)) {
            warn("Не отставайте.");
            ++lagStage_;
        }
    } else if (!escortWaiting_) {
        lagTime_ = 0;
        lagStage_ = 0;
    }

    // 2. Туда, куда не вели: к наружной решётке или к чужой двери на развилке.
    std::string zoneText;
    const bool doorOpen = level_.doors[DoorCell].open > 0.5f;
    if (escorting || (phase_ == Phase::MorningCall && doorOpen)) {
        if (playerPos_.x < 4.9f && playerPos_.z > 8.0f && playerPos_.z < 10.5f) zoneText = "Стоять. Выход закрыт.";
        if (escortMode_ != EscortMode::Bad && playerPos_.x > 11.4f && playerPos_.x < 13.6f && playerPos_.z > 9.7f)
            zoneText = "Отойдите от двери.";
    }
    if (!zoneText.empty()) {
        zoneTime_ += dt;
        if (zoneTime_ > 1.2f) {
            warn(zoneText);
            zoneTime_ = -4.0f; // следующее — если стоять там ещё пять секунд
        }
    } else {
        zoneTime_ = 0;
    }

    // 3. Толкать конвоира.
    if (pushing_) bumpTime_ += dt;
    else bumpTime_ = std::max(0.0f, bumpTime_ - dt * 0.5f);
    if (bumpTime_ > 3.0f) {
        // Первый раз — замечание: часто это просто шаг следом за остановившимся конвоиром.
        if (!bumpReminded_) remind("Соблюдайте дистанцию.");
        else warn("Соблюдайте дистанцию.");
        bumpReminded_ = true;
        bumpTime_ = -3.0f;
    }

    // 4. Простой: приказ отдан, а игрок ничего не делает.
    const bool playerInCell = playerPos_.x > 5 && playerPos_.x < 8 && playerPos_.z > 11.3f;
    int rule = -1;
    bool paused = false;
    switch (phase_) {
        case Phase::MorningCall:
            if (state_.morning() == core::Morning::ConfirmNumber && phaseStep_ >= 2 && !state_.numberConfirmed())
                rule = 1;
            break;
        case Phase::EscortOut:
            if (phaseStep_ == 0 && playerPos_.z >= 9.9f && doorOpen) rule = 2;
            break;
        case Phase::AtTest:
            if (!state_.testDone()) {
                rule = 3;
                paused = monitorMode_; // читать и думать за терминалом можно сколько угодно
            } else if (shiftAnnounced_ && state_.shiftPending()) {
                rule = 4;
            }
            break;
        case Phase::EscortBack:
            if (escort_.next >= escort_.path.size() && !playerInCell) rule = 5;
            break;
        case Phase::Evening:
            if (phaseStep_ >= 1) rule = 6;
            break;
        default: break;
    }
    if (rule != idleRuleId_) {
        idleRuleId_ = rule;
        idle_ = 0;
        idleStage_ = 0;
    }
    if (rule < 0 || paused) return;
    switch (rule) {
        case 1: idleRule(dt, b.convoyReminder * 0.5f, "Подтвердите номер у двери.", "Подтвердите номер."); break;
        case 2: idleRule(dt, 10.0f, "На выход. Следуйте за конвоиром.", "На выход."); break;
        case 3:
            idleRule(dt, static_cast<float>(b.convoyReminder), "Займите место у терминала.", "Займите место у терминала.");
            break;
        case 4:
            idleRule(dt, static_cast<float>(b.convoyShiftReminder), "Выполняйте рабочее задание.",
                     "Выполняйте рабочее задание.");
            break;
        case 5: idleRule(dt, 10.0f, "В камеру.", "В камеру."); break;
        case 6:
            idleRule(dt, static_cast<float>(b.convoyEveningReminder), "Отбой. Лягте на койку.", "Отбой. На койку.");
            break;
        default: break;
    }
}

void Game::startKill() {
    enterPhase(Phase::Kill);
    killStep_ = 0;
    killTime_ = 0;
    monitorMode_ = false;
    lines_.clear();
    lineTime_ = 0;
    lineVoiceDelay_ = -1;
    say(kRobot, "Нарушение порядка. Номер " + std::to_string(state_.prisonerNumber()) + " списан.", 2.8f,
        audio::PaVoice);
    audio_.play(audio::Alarm, 0.8f);
    eyeFlare_ = 5.0f;
    strikeFlash_ = 0.6f;
    escort_.path.clear();
    escort_.next = 0;
    eraseSave(); // выйти из игры посреди расправы и продолжить с утра не выйдет
}

void Game::updateKill(float dt) {
    const float t = phaseTime_;
    const Vec3 fwd{std::sin(yaw_), 0, -std::cos(yaw_)};
    auto aimPitch = [&](float target, float speed) { pitch_ += (target - pitch_) * std::min(1.0f, dt * speed); };
    if (killStep_ == 0) {
        // Конвоир разворачивается, лампы бьются в судорогах.
        aimPitch(0.0f, 3.0f);
        if (t > 1.2f) {
            killStep_ = 1;
            audio_.cutAll();
            lines_.clear();
            // В темноте конвоир оказывается прямо перед пленным.
            const Vec3 eye = cameraEye();
            float d = 1.7f;
            while (d > 0.65f) {
                const Vec3 p = playerPos_ + fwd * d;
                if (!level_.blocked(p.x, p.z, 0.22f) && level_.lineClear(eye, Vec3{p.x, 1.5f, p.z})) break;
                d -= 0.1f;
            }
            killDist_ = std::max(d, 0.65f);
            killYaw_ = yaw_ + kPi;
            killPos_ = playerPos_ + fwd * killDist_;
        }
    } else if (killStep_ == 1) {
        // Тишина и темнота. Только красная щель глаза.
        aimPitch(0.06f, 2.0f);
        if (t > 2.3f) {
            killStep_ = 2;
            audio_.play(audio::Screech, 1.0f);
            shake_ = 1.0f;
        }
    } else if (killStep_ == 2) {
        const float k = smooth01((t - 2.3f) / 0.13f);
        killPos_ = playerPos_ + fwd * (killDist_ + (0.66f - killDist_) * k);
        aimPitch(0.03f, 12.0f);
        if (t > 3.5f) {
            killStep_ = 3;
            fade_ = fadeTarget_ = 1.0f;
            audio_.cutAll();
            silent_ = true;
            shake_ = 0;
        }
    } else if (t > 5.0f) {
        finishRun(Ending::Killed);
    }
}

// ================================================================ атмосфера

void Game::updateAtmosphere(float dt) {
    strikeFlash_ = std::max(0.0f, strikeFlash_ - dt * 0.9f);
    eyeFlare_ = std::max(0.0f, eyeFlare_ - dt);
    if (shake_ > 0 && killStep_ != 2) shake_ = std::max(0.0f, shake_ - dt * 2.0f);

    // Рубильник и обесточенный сектор.
    lever_ += (leverTarget_ - lever_) * std::min(1.0f, dt * 9.0f);
    level_.setLever(lever_);
    if (leverTarget_ > 0) sectorDark_ = std::min(1.0f, sectorDark_ + dt * 1.5f);
    if (cutKnock_ > 0) {
        cutKnock_ -= dt;
        if (cutKnock_ <= 0) {
            // Кто-то за дверью крыла бьёт всё слабее — и затихает.
            playWorld(audio::Knock, kWingDoorPos, std::max(0.5f, 3.2f - 0.7f * static_cast<float>(cutKnocks_)));
            ++cutKnocks_;
            cutKnock_ = cutKnocks_ < 4 ? 0.8f + 0.6f * static_cast<float>(cutKnocks_) : -1.0f;
        }
    }

    const bool release = state_.morning() == core::Morning::Release;
    if (phase_ == Phase::Kill || silent_ || release) return;

    auto random01 = [&]() {
        noise_ = noise_ * 1664525u + 1013904223u;
        return ((noise_ >> 8) & 0xFFFF) / 65535.0f;
    };

    // Далёкие звуки комплекса: всегда за стенами, поэтому глухие.
    ambientTimer_ -= dt;
    if (ambientTimer_ <= 0) {
        static const Vec3 far[] = {{2.0f, 1.5f, 2.0f}, {20.0f, 1.5f, 5.0f}, {2.5f, 1.5f, 14.0f},
                                   {19.0f, 1.5f, 12.0f}, {9.5f, 2.0f, -2.0f}};
        const Vec3 where = far[static_cast<int>(random01() * 4.99f)];
        playWorld(random01() < 0.6f ? audio::Clang : audio::PipeGroan, where, 3.0f + random01() * 2.0f);
        ambientTimer_ = 20.0f + random01() * 35.0f;
    }

    // Объявления по громкой связи — в коридоре и после отбоя.
    paTimer_ -= dt;
    const bool paTime = phase_ == Phase::EscortOut || phase_ == Phase::EscortBack ||
                        (phase_ == Phase::Evening && phaseStep_ >= 2);
    if (paTimer_ <= 0 && paTime && lines_.empty() && escortMode_ != EscortMode::Bad) {
        static const char* const announcements[] = {
            "Камера 9 освобождена. Пленный переведён.",
            "Напоминание: молчание не является нарушением.",
            "Сектор Д числится пустым.",
            "Пленный номер 2207 больше не числится.",
            "Все пленные на местах. Расхождений нет.",
            "Повторная проверка крыла Д отменена.",
            "Норма за сутки выполнена. Благодарность не предусмотрена.",
        };
        const int count = static_cast<int>(sizeof(announcements) / sizeof(announcements[0]));
        say(kPa, announcements[(paIndex_ + state_.day()) % count], 4.2f, audio::PaChime);
        ++paIndex_;
        paTimer_ = 55.0f + random01() * 50.0f;
    }
}

// =================================================================== ввод

void Game::onKey(int sc, bool down) {
    if (sc < 0 || sc >= 512) return;
    keys_[sc] = down;
    if (!down) return;

    if (screen_ == Screen::MainMenu || screen_ == Screen::SettingsMenu || screen_ == Screen::Paused) {
        menuKey(sc);
        return;
    }
    if (screen_ == Screen::Ending) {
        if (endingTime_ > 3.0f && (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
                                   sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_E)) {
            audio_.cutAll();
            screen_ = Screen::MainMenu;
            menuIndex_ = 0;
        }
        return;
    }

    // --- идёт игра ---
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (monitorMode_) {
            monitorMode_ = false; // прогресс теста остаётся в ядре
        } else {
            screen_ = Screen::Paused;
            menuIndex_ = 0;
        }
        return;
    }

    if (monitorMode_) {
        if (monitorBlend_ < 0.6f) return; // камера ещё подъезжает
        if (sc == SDL_SCANCODE_1 || sc == SDL_SCANCODE_KP_1) terminal_.pressNumber(0, audio_);
        if (sc == SDL_SCANCODE_2 || sc == SDL_SCANCODE_KP_2) terminal_.pressNumber(1, audio_);
        if (sc == SDL_SCANCODE_3 || sc == SDL_SCANCODE_KP_3) terminal_.pressNumber(2, audio_);
        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) terminal_.pressEnter(state_, audio_);
        if (sc == SDL_SCANCODE_R) terminal_.pressReplay(state_, audio_);
        return;
    }

    if (sc == SDL_SCANCODE_E) interact();

    // --- клавиши разработчика: только при запуске с --dev ---
    if (!options_.dev) return;
    const core::Balance& b = state_.balance;
    switch (sc) {
        case SDL_SCANCODE_TAB: devOverlay_ = !devOverlay_; break;
        case SDL_SCANCODE_LEFTBRACKET: state_.debugSetRating(state_.rating() - 5); break;
        case SDL_SCANCODE_RIGHTBRACKET: state_.debugSetRating(state_.rating() + 5); break;
        case SDL_SCANCODE_5: state_.debugSetRating(b.badLowMax); break;
        case SDL_SCANCODE_6: state_.debugSetRating(b.badLowMax + 1); break;
        case SDL_SCANCODE_7: state_.debugSetRating(b.watchMax); break;
        case SDL_SCANCODE_8: state_.debugSetRating(b.watchMax + 1); break;
        case SDL_SCANCODE_9: devCommand("release"); break;
        case SDL_SCANCODE_0: devCommand("endday"); break;
        case SDL_SCANCODE_MINUS: devCommand("finish"); break;
        default: break;
    }
}

void Game::onMouseMove(float dx, float dy) {
    if (screen_ != Screen::Playing) return;
    if (monitorMode_) {
        terminal_.moveCursor(dx * 1.3f * settings_.mouse, dy * 1.3f * settings_.mouse);
        return;
    }
    if (controlLocked()) return;
    yaw_ += dx * 0.0022f * settings_.mouse;
    pitch_ -= dy * 0.0022f * settings_.mouse;
    pitch_ = std::clamp(pitch_, -1.45f, 1.45f);
}

void Game::onMouseButton(bool down) {
    if (!down || screen_ != Screen::Playing) return;
    if (monitorMode_ && monitorBlend_ > 0.6f) terminal_.click(audio_);
}

void Game::devCommand(const std::string& command) {
    if (!options_.dev) return;
    std::istringstream in(command);
    std::string name;
    in >> name;
    if (name == "new") {
        startNewGame();
    } else if (name == "continue") {
        continueGame();
    } else if (name == "rating") {
        int v = 50;
        in >> v;
        state_.debugSetRating(v);
    } else if (name == "release") {
        state_.debugForceRelease(); // сработает на следующем рассвете
    } else if (name == "finish") {
        state_.debugCompleteTasks();
    } else if (name == "endday") {
        if (screen_ == Screen::Playing && !state_.isEnding()) {
            state_.debugEndDay();
            startDay();
        }
    } else if (name == "tp") {
        float x = 0, z = 0, yawDeg = 0, pitchDeg = 0;
        in >> x >> z >> yawDeg >> pitchDeg;
        playerPos_ = {x, 0, z};
        yaw_ = yawDeg * kPi / 180.0f;
        pitch_ = pitchDeg * kPi / 180.0f;
    } else if (name == "tasks") {
        int a = 0, b = 1;
        in >> a >> b;
        state_.debugSetTasks(a, b);
        terminal_.reset();
    } else if (name == "strike") {
        warn("Отладка.");
    } else if (name == "kill") {
        if (screen_ == Screen::Playing && phase_ != Phase::Kill) startKill();
    } else if (name == "skipfade") {
        fade_ = fadeTarget_;
        dayCard_ = 0;
    } else if (name == "state") {
        static const char* const phases[] = {"Dawn",    "MorningCall", "EscortOut", "AtTest",   "EscortBack",
                                             "Evening", "Sleep",       "BadEnd",    "FreeWalk", "WhiteOut", "Kill"};
        static const char* const screens[] = {"MainMenu", "SettingsMenu", "Playing", "Paused", "Ending"};
        const core::Shift& s = state_.shift();
        std::printf("STATE screen=%s day=%d rating=%d morning=%s phase=%s task=%d shift=%s%s monitor=%d "
                    "strikes=%d pos=%.2f,%.2f robot=%.2f,%.2f gate=%.2f cell=%.2f bad=%.2f exit=%.2f ending=%s\n",
                    screens[static_cast<int>(screen_)], state_.day(), state_.rating(),
                    morningName(state_.morning()), phases[static_cast<int>(phase_)], state_.currentTask(),
                    s.kind == core::ShiftKind::None   ? "none"
                    : s.kind == core::ShiftKind::Seal ? "seal"
                    : s.kind == core::ShiftKind::Signal ? "signal"
                                                        : "power",
                    s.kind == core::ShiftKind::None ? "" : (s.done ? "/done" : "/pending"), monitorMode_ ? 1 : 0,
                    strikes_, playerPos_.x, playerPos_.z, escort_.pos.x, escort_.pos.z, level_.doors[DoorGate].open,
                    level_.doors[DoorCell].open, level_.doors[DoorBad].open, level_.doors[DoorExit].open,
                    screen_ != Screen::Ending ? "-"
                    : endingGood_             ? "good"
                    : endingKilled_           ? "killed"
                    : endingLow_              ? "bad-low"
                                              : "bad-high");
        std::fflush(stdout);
    }
}

// ============================================================== обновление

void Game::update(float dt) {
    time_ += dt;
    if (screen_ == Screen::Playing) {
        updatePlaying(dt);
    } else if (screen_ == Screen::Ending) {
        endingTime_ += dt;
        // После хорошей концовки слышен ветер; после плохой — тишина.
        audio_.loop(audio::Wind, endingGood_ ? 0.5f : 0.0f);
        audio_.setMusic(0.0f);
        audio_.loop(audio::Drone, 0.0f);
        audio_.loop(audio::LampHum, 0.0f);
        audio_.loop(audio::MonitorHum, 0.0f);
    } else if (screen_ == Screen::MainMenu || screen_ == Screen::SettingsMenu) {
        if (menuReturn_ != Screen::Paused || screen_ == Screen::MainMenu) {
            audio_.loop(audio::Drone, 0.18f);
            audio_.setMusic(settings_.music * 0.8f);
            audio_.loop(audio::LampHum, 0.0f);
            audio_.loop(audio::MonitorHum, 0.0f);
            audio_.loop(audio::Wind, 0.0f);
        }
    }
}

void Game::updatePlaying(float dt) {
    updatePhase(dt);
    if (screen_ != Screen::Playing) return; // день закончился концовкой

    updatePlayer(dt);
    updateEscort(dt);
    updateDiscipline(dt);
    if (screen_ != Screen::Playing) return;
    updateAtmosphere(dt);
    level_.updateDoors(dt);
    level_.placeHuman(humanPos_, humanYaw_, humanVisible_);

    terminal_.update(dt, state_, settings_, phase_ == Phase::AtTest, monitorMode_ && monitorBlend_ > 0.6f,
                     audio_);

    // Отложенные стуки после действий смены «сигнал».
    if (extraKnock_ > 0) {
        extraKnock_ -= dt;
        if (extraKnock_ <= 0) playWorld(audio::Knock, kWingDoorPos, 1.2f);
    }
    if (replyKnock_ > 0) {
        replyKnock_ -= dt;
        if (replyKnock_ <= 0) playWorld(audio::Knock, kWingDoorPos, 3.0f);
    }

    // Субтитры: по одной реплике за раз.
    if (!lines_.empty()) {
        Line& line = lines_.front();
        if (lineTime_ == 0 && line.sound >= 0) {
            if (line.sound == audio::PaChime) {
                audio_.play(audio::PaChime, 0.7f);
                lineVoiceDelay_ = 0.5f; // «речь» после сигнала
            } else if (line.sound == audio::PaVoice) {
                playWorld(audio::PaVoice, escort_.pos + Vec3{0, 1.7f, 0}, 2.2f);
            } else if (line.sound == audio::Voice) {
                playWorld(audio::Voice, humanPos_ + Vec3{0, 1.6f, 0}, 1.8f);
            }
        }
        lineTime_ += dt;
        if (lineTime_ >= line.seconds) {
            lines_.pop_front();
            lineTime_ = 0;
        }
    }
    if (lineVoiceDelay_ > 0) {
        lineVoiceDelay_ -= dt;
        if (lineVoiceDelay_ <= 0) audio_.play(audio::PaVoice, 0.55f);
    }

    if (fade_ < fadeTarget_) fade_ = std::min(fadeTarget_, fade_ + fadeSpeed_ * dt);
    if (fade_ > fadeTarget_) fade_ = std::max(fadeTarget_, fade_ - fadeSpeed_ * dt);
    if (dayCard_ > 0) dayCard_ -= dt;

    updateLights(dt);
    updateSound();
}

Vec3 Game::cameraEye() const {
    const float bob = std::sin(bobPhase_) * 0.025f;
    const Vec3 own{playerPos_.x, kEyeHeight + bob, playerPos_.z};
    return lerp(own, spot::kMonitorEye, smooth01(monitorBlend_));
}

void Game::updatePlayer(float dt) {
    // Камера плавно подъезжает к монитору и возвращается.
    const float blendTarget = monitorMode_ ? 1.0f : 0.0f;
    if (monitorBlend_ < blendTarget) monitorBlend_ = std::min(blendTarget, monitorBlend_ + dt * 2.2f);
    if (monitorBlend_ > blendTarget) monitorBlend_ = std::max(blendTarget, monitorBlend_ - dt * 2.6f);

    aimed_ = UseNone;
    pushing_ = false;
    if (controlLocked()) return; // в режиме монитора движение и поворот отключены

    const Vec3 forward{std::sin(yaw_), 0, -std::cos(yaw_)};
    const Vec3 right{std::cos(yaw_), 0, std::sin(yaw_)};
    Vec3 wish{0, 0, 0};
    if (keys_[SDL_SCANCODE_W] || keys_[SDL_SCANCODE_UP]) wish += forward;
    if (keys_[SDL_SCANCODE_S] || keys_[SDL_SCANCODE_DOWN]) wish += forward * -1.0f;
    if (keys_[SDL_SCANCODE_D]) wish += right;
    if (keys_[SDL_SCANCODE_A]) wish += right * -1.0f;

    const float wishLen = length(wish);
    if (wishLen > 0.01f) {
        wish = wish * (1.0f / wishLen);
        const float oldX = playerPos_.x, oldZ = playerPos_.z;
        level_.moveWithCollision(&playerPos_.x, &playerPos_.z, wish.x * kWalkSpeed * dt,
                                 wish.z * kWalkSpeed * dt, kPlayerRadius);
        const float moved = std::hypot(playerPos_.x - oldX, playerPos_.z - oldZ);
        bobPhase_ += moved * 7.0f;
        stepDist_ += moved;
        if (stepDist_ > 0.8f) {
            stepDist_ = 0;
            audio_.play(audio::StepPlayer, 0.3f);
        }
    }

    // Конвоир — препятствие: сквозь него не пройти.
    Vec3 away = playerPos_ - escort_.pos;
    away.y = 0;
    const float dist = length(away);
    const float minDist = 0.74f;
    pushing_ = wishLen > 0.01f && dist < minDist + 0.04f && escortMode_ != EscortMode::Off;
    if (dist < minDist) {
        const Vec3 n = dist > 0.001f ? away * (1.0f / dist) : Vec3{1, 0, 0};
        const float push = minDist - dist;
        level_.moveWithCollision(&playerPos_.x, &playerPos_.z, n.x * push, n.z * push, kPlayerRadius);
    }

    // Что под прицелом.
    const Vec3 eye = cameraEye();
    const Vec3 dir{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)};
    const InteractId hit = level_.pick(eye, dir, kReach);
    if (hit != UseNone && canUse(hit)) aimed_ = hit;
}

void Game::updateEscort(float dt) {
    Escort& e = escort_;
    e.moving = false;
    escortWaiting_ = false;
    if (escortMode_ == EscortMode::Off) {
        level_.placeRobot(e.pos, e.yaw, 0, 0); // обесточен: глаз погас
        return;
    }
    if (phase_ == Phase::Kill && killStep_ >= 1) {
        // Скример: голова на уровне глаз, щель глаза раскрывается.
        const bool lunge = killStep_ >= 2;
        level_.placeRobot(killPos_, killYaw_, -0.15f, lunge ? 2.6f : 2.0f, lunge ? 2.2f : 1.4f);
        return;
    }

    Vec3 toPlayer = playerPos_ - e.pos;
    toPlayer.y = 0;
    const float distPlayer = length(toPlayer);
    float faceYaw = std::atan2(toPlayer.x, -toPlayer.z); // стоя на месте, смотрит на игрока
    const bool hasPath = e.next < e.path.size();
    bool hold = false;

    if (hasPath) {
        if (e.waitDoor >= 0) {
            if (level_.doors[e.waitDoor].open < 0.85f) hold = true;
            else e.waitDoor = -1;
        }
        if (e.leading) {
            // Ждёт, если игрок отстал: тот дальше от цели, чем сам конвоир.
            const Vec3 dest = e.path.back();
            const float robotToDest = length(dest - e.pos);
            const float playerToDest = length(dest - playerPos_);
            if (distPlayer > 4.2f && playerToDest > robotToDest + 0.5f) {
                hold = true;
                escortWaiting_ = true; // напоминания и предупреждения — в updateDiscipline
            }
        }
        if (e.pause > 0) {
            e.pause -= dt;
            hold = true;
        }
    }

    if (hasPath && !hold) {
        // Режим конвоя задаёт скорость и ритм шагов.
        float speed = 1.5f, interval = 0.42f;
        if (escortMode_ == EscortMode::Watch) {
            speed = 1.15f;
            interval = (e.stepCount % 2 == 0) ? 0.50f : 0.66f; // неровный шаг
        } else if (escortMode_ == EscortMode::Bad) {
            speed = 1.0f;
            interval = 0.72f;
        }
        Vec3 d = e.path[e.next] - e.pos;
        d.y = 0;
        const float dist = length(d);
        const float step = speed * dt;
        if (dist <= step) {
            e.pos = e.path[e.next];
            ++e.next;
        } else {
            e.pos += d * (step / dist);
            faceYaw = std::atan2(d.x, -d.z);
        }
        if (dist > step) e.moving = true;
        e.sincePause += step;
        e.stepTimer -= dt;
        if (e.stepTimer <= 0) {
            const bool heavy = escortMode_ == EscortMode::Bad;
            playWorld(heavy ? audio::StepRobotHeavy : audio::StepRobot, e.pos + Vec3{0, 0.3f, 0},
                          heavy ? 2.6f : 2.0f);
            e.stepTimer = interval;
            ++e.stepCount;
        }
        // Дополнительное наблюдение: конвоир то и дело останавливается и оборачивается.
        if (escortMode_ == EscortMode::Watch && e.leading && e.sincePause > 3.0f) {
            e.pause = 1.4f;
            e.sincePause = 0;
        }
    }

    const float diff = angleDiff(faceYaw, e.yaw);
    const float maxTurn = 3.2f * dt;
    e.yaw += std::clamp(diff, -maxTurn, maxTurn);
    e.bob = e.moving ? 0.018f * std::fabs(std::sin(time_ * 8.0f)) : 0.0f;

    float glow = 1.0f;
    if (escortMode_ == EscortMode::Watch) glow = 1.3f + 0.45f * std::sin(time_ * 5.0f);
    if (escortMode_ == EscortMode::Bad) glow = 1.4f;
    glow += eyeFlare_ * (0.8f + 0.4f * std::sin(time_ * 31.0f)); // предупреждение: глаз разгорается
    level_.placeRobot(e.pos, e.yaw, e.bob, glow, 1.0f + 0.15f * std::min(eyeFlare_, 2.0f));
}

void Game::updateLights(float dt) {
    // Лампа в камере мерцает: редкие провалы яркости.
    flickerTimer_ -= dt;
    if (flickerTimer_ <= 0) {
        noise_ = noise_ * 1664525u + 1013904223u;
        const float r = ((noise_ >> 8) & 0xFFFF) / 65535.0f;
        lampFlicker_ = r < 0.05f ? 0.40f : (r < 0.12f ? 0.75f : 1.0f);
        noise_ = noise_ * 1664525u + 1013904223u;
        const float c = ((noise_ >> 8) & 0xFFFF) / 65535.0f;
        // Коридорные лампы иногда проваливаются почти в ноль.
        corridorFlicker_ = c < 0.025f ? 0.12f : (c < 0.07f ? 0.6f : 1.0f);
        flickerTimer_ = 0.07f;
    }
    const core::Morning morning = state_.morning();
    const bool bad = morning == core::Morning::BadLow || morning == core::Morning::BadHigh;
    const bool release = morning == core::Morning::Release;
    const core::Shift& shift = state_.shift();
    const bool shiftActive = phase_ == Phase::AtTest && shiftAnnounced_ && state_.shiftPending();
    const bool sealOn = shiftActive && shift.kind == core::ShiftKind::Seal;
    const bool signalOn = shiftActive && shift.kind == core::ShiftKind::Signal;
    const bool powerOn = shiftActive && shift.kind == core::ShiftKind::Power;
    const bool powerShift = shift.kind == core::ShiftKind::Power && phase_ == Phase::AtTest && shiftAnnounced_;
    const float night = nightLights_ ? 0.22f : 1.0f;
    const bool dayOn = level_.objects[ObjDaylight].visible;
    const float pulse = 0.6f + 0.4f * std::sin(time_ * 3.0f);

    // В обесточенном комплексе (хорошая концовка) горит только аварийный свет.
    const float power = release ? 0.25f : 1.0f;

    struct Setup {
        LightId light;
        ObjectId lamp;
        Vec3 color;
        float level;
    };
    const Setup setups[] = {
        {LightCell, ObjLampCell, {0.95f, 0.84f, 0.66f}, 1.35f * lampFlicker_ * power * night},
        {LightCellDoor, ObjLampCellDoor, {0.92f, 0.80f, 0.60f}, 1.25f * power * corridorFlicker_},
        // Маршрут читается по лампам: к тестам — холодная над проходом,
        // в плохое утро вместо неё горит красная над другой дверью.
        {LightFork, ObjLampFork, {0.70f, 0.84f, 1.0f}, (bad || release) ? 0.0f : 1.5f * corridorFlicker_},
        {LightBadDoor, ObjLampBadDoor, {1.0f, 0.16f, 0.10f}, bad ? 2.0f : 0.0f},
        {LightTestMain, ObjLampTest, {0.78f, 0.88f, 1.0f}, 1.05f * power * (1.0f - 0.35f * sectorDark_)},
        {LightSeal, ObjLampSeal, {1.0f, 0.88f, 0.60f}, sealOn ? 1.5f : 0.0f},
        {LightWing, ObjLampWing, {1.0f, 0.20f, 0.12f}, signalOn ? 0.5f + 0.7f * pulse : 0.0f},
        {LightBadRoom, ObjLampBadRoom, {1.0f, 0.22f, 0.14f}, 1.2f},
        {LightPower, ObjLampPower, {1.0f, 0.62f, 0.18f}, powerShift ? (powerOn ? 0.7f + 0.6f * pulse : 0.35f) : 0.0f},
    };
    for (const Setup& s : setups) {
        level_.lights[s.light].color = s.color * s.level;
        level_.objects[s.lamp].tint = s.color;
        level_.objects[s.lamp].emissive = 0.06f + 1.1f * s.level;
    }
    level_.lights[LightMonitor].color = Vec3{0.25f, 0.9f, 0.4f} * (release ? 0.0f : 0.45f);
    level_.lights[LightDay].color = Vec3{1.0f, 0.97f, 0.9f} * (dayOn ? 2.4f : 0.0f);

    // Свет из-под дверей: тусклое пятно на полу, а не полоса.
    level_.objects[ObjStripCell].tint = {0.95f, 0.80f, 0.58f};
    level_.objects[ObjStripCell].emissive = 0.42f * power * corridorFlicker_;
    level_.objects[ObjStripCell].visible = level_.doors[DoorCell].open < 0.1f;
    level_.objects[ObjStripBad].tint = {1.0f, 0.20f, 0.12f};
    level_.objects[ObjStripBad].emissive = bad ? 0.6f : 0.10f;
    level_.objects[ObjStripBad].visible = level_.doors[DoorBad].open < 0.1f;
    const bool wingLit = signalOn || (powerOn && sectorDark_ < 0.5f);
    level_.objects[ObjStripWing].tint = {1.0f, 0.75f, 0.45f};
    level_.objects[ObjStripWing].emissive = signalOn ? 0.2f + 0.4f * pulse : 0.35f;
    level_.objects[ObjStripWing].visible = wingLit;
    level_.setPowerPanel(shift, powerShift && sectorDark_ < 0.5f);

    // Экран монитора и подсветка кнопок активной смены.
    level_.objects[ObjScreen].emissive = release ? 0.0f : 1.0f;
    level_.objects[ObjReaderLed].emissive = sealOn ? 1.2f : 0.1f;
    const float sealGlow = sealOn ? 0.25f : 0.0f;
    const float signalGlow = signalOn ? 0.25f : 0.0f;
    level_.objects[ObjBtnSealConfirm].emissive = sealGlow;
    level_.objects[ObjBtnSealMismatch].emissive = sealGlow;
    level_.objects[ObjBtnSignalNone].emissive = signalGlow;
    level_.objects[ObjBtnSignalReceived].emissive = signalGlow;
    level_.objects[ObjBtnSignalRespond].emissive = signalGlow;
    const float powerGlow = powerOn ? 0.25f : 0.0f;
    level_.objects[ObjBtnPowerMark].emissive = powerGlow;
    level_.objects[ObjBtnPowerBackup].emissive = powerGlow;
    level_.objects[ObjPowerPanel].emissive = powerShift && sectorDark_ < 0.5f ? 0.15f : 0.0f;

    // Расправа: свет бьётся, гаснет, затем красный стробоскоп.
    if (phase_ == Phase::Kill) {
        const bool strobe = std::fmod(time_ * 13.0f, 1.0f) < 0.5f;
        for (int i = 0; i < kLightCount; ++i) {
            if (i == LightRobot) continue;
            Vec3& c = level_.lights[i].color;
            if (killStep_ == 0) c = c * (lampFlicker_ < 1.0f || strobe ? 0.15f : 1.0f);
            else if (killStep_ == 1) c = Vec3{0, 0, 0};
            else c = strobe ? Vec3{1.6f, 0.05f, 0.03f} : Vec3{0.05f, 0, 0};
        }
        for (int id = ObjLampCell; id <= ObjLampPower; ++id) level_.objects[id].emissive = killStep_ >= 1 ? 0.0f : 0.3f;
        for (ObjectId id : {ObjStripCell, ObjStripBad, ObjStripWing}) level_.objects[id].emissive = 0.0f;
        level_.lights[LightMonitor].color = Vec3{0, 0, 0};
    }
}

void Game::updateSound() {
    audio_.setListener(cameraEye(), yaw_);
    if (silent_ || (phase_ == Phase::Kill && killStep_ >= 1)) { // после обрыва звука — тишина
        audio_.setMusic(0.0f);
        return;
    }
    const bool release = state_.morning() == core::Morning::Release;
    audio_.loopAt(audio::LampHum, kCellLampPos, release ? 0.3f : (nightLights_ ? 0.45f : 1.6f), muffleFor(kCellLampPos));
    audio_.loopAt(audio::MonitorHum, kMonitorPos, release ? 0.0f : 1.2f, muffleFor(kMonitorPos));
    audio_.loop(audio::Drone, release ? 0.04f : 0.24f);
    if (level_.objects[ObjDaylight].visible) {
        const Vec3 windPos = spot::kExitLight + Vec3{0, 1.5f, 0};
        audio_.loopAt(audio::Wind, windPos, 2.5f, muffleFor(windPos));
    } else {
        audio_.loop(audio::Wind, 0.0f);
    }
    // Эмбиент: тише за терминалом, чтобы не мешал читать; при освобождении стихает.
    float music = settings_.music * 0.55f;
    if (monitorMode_) music *= 0.6f;
    if (release) music *= 0.3f;
    audio_.setMusic(music);
}

// ==================================================================== меню

std::vector<std::string> Game::menuItems() const {
    if (screen_ == Screen::MainMenu) {
        std::vector<std::string> items;
        if (saveExists()) items.push_back("Продолжить");
        items.push_back("Новая игра");
        items.push_back("Настройки");
        items.push_back("Выход");
        return items;
    }
    if (screen_ == Screen::Paused) return {"Вернуться", "Настройки", "В главное меню", "Выйти из игры"};
    char mouse[64], volume[64], music[96];
    std::snprintf(mouse, sizeof(mouse), "Чувствительность мыши: %.1f", settings_.mouse);
    std::snprintf(volume, sizeof(volume), "Громкость: %d%%", static_cast<int>(settings_.volume * 100 + 0.5f));
    if (audio_.hasMusic())
        std::snprintf(music, sizeof(music), "Эмбиент: %d%%", static_cast<int>(settings_.music * 100 + 0.5f));
    else
        std::snprintf(music, sizeof(music), "Эмбиент: нет файла");
    return {"Рябь экрана терминала: " + onOff(settings_.ripple),
            "Посимвольная печать: " + onOff(settings_.typewriter),
            mouse,
            volume,
            music,
            "Полный экран: " + onOff(settings_.fullscreen),
            "Назад"};
}

void Game::menuKey(int sc) {
    const int count = static_cast<int>(menuItems().size());
    if (sc == SDL_SCANCODE_W || sc == SDL_SCANCODE_UP) {
        menuIndex_ = (menuIndex_ + count - 1) % count;
        audio_.play(audio::UiSelect, 0.4f);
    } else if (sc == SDL_SCANCODE_S || sc == SDL_SCANCODE_DOWN) {
        menuIndex_ = (menuIndex_ + 1) % count;
        audio_.play(audio::UiSelect, 0.4f);
    } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_E ||
               sc == SDL_SCANCODE_SPACE) {
        menuActivate(menuIndex_, 0);
    } else if (sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_LEFT) {
        menuActivate(menuIndex_, -1);
    } else if (sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_RIGHT) {
        menuActivate(menuIndex_, +1);
    } else if (sc == SDL_SCANCODE_ESCAPE) {
        if (screen_ == Screen::Paused) {
            screen_ = Screen::Playing; // Esc ещё раз — вернуться в игру
        } else if (screen_ == Screen::SettingsMenu) {
            storeSettings();
            screen_ = menuReturn_;
            menuIndex_ = 0;
        }
    }
}

void Game::menuActivate(int index, int direction) {
    const std::vector<std::string> items = menuItems();
    if (index < 0 || index >= static_cast<int>(items.size())) return;
    const std::string& item = items[index];

    if (screen_ == Screen::SettingsMenu) {
        const int step = direction == 0 ? 1 : direction;
        switch (index) {
            case 0: settings_.ripple = !settings_.ripple; break;
            case 1: settings_.typewriter = !settings_.typewriter; break;
            case 2:
                settings_.mouse = std::clamp(settings_.mouse + 0.1f * step, 0.2f, 3.0f);
                if (direction == 0 && settings_.mouse > 2.95f) settings_.mouse = 0.2f;
                break;
            case 3:
                settings_.volume = std::clamp(settings_.volume + 0.1f * step, 0.0f, 1.0f);
                if (direction == 0 && settings_.volume > 0.95f) settings_.volume = 0.0f;
                audio_.setMasterVolume(settings_.volume);
                break;
            case 4:
                settings_.music = std::clamp(settings_.music + 0.1f * step, 0.0f, 1.0f);
                if (direction == 0 && settings_.music > 0.95f) settings_.music = 0.0f;
                break;
            case 5: settings_.fullscreen = !settings_.fullscreen; break;
            default:
                if (direction == 0) {
                    storeSettings();
                    screen_ = menuReturn_;
                    menuIndex_ = 0;
                }
                break;
        }
        audio_.play(audio::UiSelect, 0.5f);
        return;
    }

    if (direction != 0) return;
    audio_.play(audio::UiConfirm, 0.5f);
    if (item == "Продолжить") {
        notice_.clear();
        continueGame();
    } else if (item == "Новая игра") {
        notice_.clear();
        startNewGame();
    } else if (item == "Настройки") {
        menuReturn_ = screen_;
        screen_ = Screen::SettingsMenu;
        menuIndex_ = 0;
    } else if (item == "Выход" || item == "Выйти из игры") {
        quit_ = true;
    } else if (item == "Вернуться") {
        screen_ = Screen::Playing;
    } else if (item == "В главное меню") {
        audio_.cutAll();
        screen_ = Screen::MainMenu;
        menuIndex_ = 0;
    }
}

// =============================================================== отрисовка

void Game::render(int width, int height) {
    const bool world = screen_ == Screen::Playing || screen_ == Screen::Paused ||
                       (screen_ == Screen::SettingsMenu && menuReturn_ == Screen::Paused);
    if (world) {
        // Сначала — содержимое монитора в текстуру, затем сцена.
        renderer_.bindTarget(screenTarget_);
        terminal_.draw(renderer_, state_, settings_, time_, phase_ == Phase::AtTest,
                       monitorMode_ && monitorBlend_ > 0.6f);
        renderer_.bindScreen(width, height);
        drawWorld(width, height);
        drawHud(width, height);
    } else {
        renderer_.bindScreen(width, height);
        renderer_.clear(0, 0, 0);
    }
    if (screen_ == Screen::Ending) drawEnding(width, height);
    if (screen_ == Screen::MainMenu || screen_ == Screen::SettingsMenu || screen_ == Screen::Paused)
        drawMenu(width, height);
}

void Game::drawWorld(int width, int height) {
    const float t = smooth01(monitorBlend_);
    const Vec3 eye = cameraEye();
    // У монитора камера смотрит строго на экран (на север, без наклона).
    float camYaw = yaw_ + angleDiff(0.0f, yaw_) * t;
    float camPitch = pitch_ * (1.0f - t);
    if (shake_ > 0) { // тряска при скримере
        camYaw += shake_ * 0.035f * std::sin(time_ * 71.0f);
        camPitch += shake_ * 0.03f * std::sin(time_ * 53.0f + 1.3f);
    }
    const Vec3 dir{std::sin(camYaw) * std::cos(camPitch), std::sin(camPitch),
                   -std::cos(camYaw) * std::cos(camPitch)};
    const float fov = (phase_ == Phase::Kill && killStep_ >= 2) ? kFov * 0.78f : kFov;
    const Mat4 proj = Mat4::perspective(fov, static_cast<float>(width) / static_cast<float>(height), 0.05f, 60.0f);
    const Mat4 view = Mat4::lookAt(eye, eye + dir, {0, 1, 0});

    std::vector<gfx::Light> lights;
    for (const gfx::Light& l : level_.lights)
        if (l.color.x + l.color.y + l.color.z > 0.001f) lights.push_back(l);

    renderer_.clear(0, 0, 0);
    // Почти без рассеянного света: темнота — это темнота, путь читается по лампам.
    const bool dark = phase_ == Phase::Kill && killStep_ >= 1;
    const Vec3 ambient = dark ? Vec3{0.004f, 0.0f, 0.0f} : Vec3{0.019f, 0.021f, 0.025f};
    renderer_.beginWorld(proj * view, eye, lights, ambient, 0.085f);
    level_.draw(renderer_);
    renderer_.vignette(time_, 1.0f * (1.0f - 0.6f * t));
}

void Game::drawHud(int width, int height) {
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float s = std::max(1.0f, std::floor(h / 360.0f)); // размер пикселя интерфейса
    renderer_.begin2D(width, height);

    const bool playing = screen_ == Screen::Playing;
    if (playing && !monitorMode_ && monitorBlend_ < 0.02f && !controlLocked()) {
        // Прицел и подсказка взаимодействия.
        renderer_.rect(w * 0.5f - s, h * 0.5f - s, 2 * s, 2 * s, gfx::Rgba{0.8f, 0.85f, 0.8f, 0.55f});
        if (aimed_ != UseNone) {
            const std::string prompt = promptFor(aimed_);
            const float tw = font::length(prompt) * 8 * s;
            renderer_.rect(w * 0.5f - tw * 0.5f - 6 * s, h * 0.60f - 3 * s, tw + 12 * s, 22 * s,
                           gfx::Rgba{0, 0, 0, 0.6f});
            renderer_.textCentered(w * 0.5f, h * 0.60f, s, gfx::Rgba{0.9f, 0.95f, 0.88f, 1}, prompt);
        }
    }

    // Предупреждение конвоира: красная вспышка по краям экрана.
    if (playing && strikeFlash_ > 0.001f) {
        const float a = 0.35f * strikeFlash_;
        const float edge = h * 0.12f;
        renderer_.rect(0, 0, w, edge, gfx::Rgba{0.8f, 0.02f, 0.02f, a});
        renderer_.rect(0, h - edge, w, edge, gfx::Rgba{0.8f, 0.02f, 0.02f, a});
        renderer_.rect(0, edge, edge, h - 2 * edge, gfx::Rgba{0.8f, 0.02f, 0.02f, a});
        renderer_.rect(w - edge, edge, edge, h - 2 * edge, gfx::Rgba{0.8f, 0.02f, 0.02f, a});
    }
    // Скример: красная заливка и рваные полосы помех.
    if (playing && phase_ == Phase::Kill && killStep_ == 2) {
        const bool strobe = std::fmod(time_ * 13.0f, 1.0f) < 0.5f;
        renderer_.rect(0, 0, w, h, gfx::Rgba{0.6f, 0.0f, 0.0f, strobe ? 0.28f : 0.10f});
        uint32_t g = static_cast<uint32_t>(time_ * 60.0f) * 2654435761u;
        for (int i = 0; i < 9; ++i) {
            g = g * 1664525u + 1013904223u;
            const float y = static_cast<float>((g >> 8) % 1000) / 1000.0f * h;
            const float bh = (2.0f + static_cast<float>((g >> 4) % 18)) * s;
            const float dx = (static_cast<float>((g >> 12) % 200) - 100.0f) * s;
            renderer_.rect(dx, y, w, bh, (i % 3 == 0) ? gfx::Rgba{0, 0, 0, 0.85f} : gfx::Rgba{0.9f, 0.1f, 0.08f, 0.35f});
        }
    }

    // Затемнение и белая вспышка — под текстом, чтобы реплики читались.
    if (fade_ > 0.001f) renderer_.rect(0, 0, w, h, gfx::Rgba{0, 0, 0, fade_});
    if (white_ > 0.001f) renderer_.rect(0, 0, w, h, gfx::Rgba{1, 1, 0.96f, white_});

    // Субтитры.
    if (playing && !lines_.empty()) {
        const Line& line = lines_.front();
        const int maxChars = std::max(24, static_cast<int>((w * 0.8f) / (8 * s)));
        const std::vector<std::string> wrapped = font::wrap(line.text, maxChars);
        const float lineH = 18 * s;
        float y = h - 28 * s - lineH * static_cast<float>(wrapped.size()) - 14 * s;
        float widest = static_cast<float>(font::length(line.speaker));
        for (const std::string& part : wrapped) widest = std::max(widest, static_cast<float>(font::length(part)));
        const float boxW = widest * 8 * s + 20 * s;
        renderer_.rect(w * 0.5f - boxW * 0.5f, y - 6 * s, boxW,
                       lineH * static_cast<float>(wrapped.size() + 1) + 10 * s, gfx::Rgba{0, 0, 0, 0.62f});
        renderer_.textCentered(w * 0.5f, y, s, gfx::Rgba{0.55f, 0.62f, 0.56f, 1}, line.speaker);
        y += lineH;
        for (const std::string& part : wrapped) {
            renderer_.textCentered(w * 0.5f, y, s, gfx::Rgba{0.92f, 0.95f, 0.90f, 1}, part);
            y += lineH;
        }
    }

    // Номер дня — единственное число, которое игрок видит.
    if (playing && dayCard_ > 0) {
        const float alpha = clamp01(dayCard_ / 1.2f) * clamp01((4.0f - dayCard_) / 0.6f);
        renderer_.textCentered(w * 0.5f, h * 0.40f, s * 3, gfx::Rgba{0.85f, 0.9f, 0.85f, alpha},
                               "ДЕНЬ " + std::to_string(state_.day()));
    }
    renderer_.end2D();

    if (options_.dev && devOverlay_) drawDev(width, height);
}

void Game::drawDev(int width, int height) {
    (void)height;
    static const char* const phases[] = {"рассвет", "вызов", "конвой туда", "тесты",    "конвой обратно", "вечер",
                                         "сон",     "плохая концовка", "выход", "свет", "расправа"};
    static const char* const bands[] = {"0-20 непригоден", "21-34 подтверждение", "35-65 норма",
                                        "66-79 наблюдение", "80-100 угроза"};
    const core::Shift& s = state_.shift();
    char buf[4][200];
    std::snprintf(buf[0], sizeof(buf[0]), "DEV  день %d   рейтинг %d (%s)", state_.day(), state_.rating(),
                  bands[static_cast<int>(core::bandOf(state_.balance, state_.rating()))]);
    std::snprintf(buf[1], sizeof(buf[1]), "утро: %s   этап: %s   задание %d/2   смена: %s",
                  morningName(state_.morning()), phases[static_cast<int>(phase_)],
                  std::min(state_.currentTask() + 1, 2),
                  s.kind == core::ShiftKind::None     ? "нет"
                  : s.kind == core::ShiftKind::Seal   ? "пломба"
                  : s.kind == core::ShiftKind::Signal ? "сигнал"
                                                      : "рубильник");
    std::snprintf(buf[2], sizeof(buf[2]), "освобождение на рассвете: %s   предупреждения конвоира: %d из %d",
                  state_.debugReleaseForced() ? "включено принудительно" : "по шансу", strikes_,
                  state_.balance.convoyStrikesToKill - 1);
    std::snprintf(buf[3], sizeof(buf[3]),
                  "[ ] -5/+5   5/6 нижний порог   7/8 верхний   9 освобождение   0 конец дня   - закрыть задания");
    renderer_.begin2D(width, height);
    renderer_.rect(0, 0, static_cast<float>(width), 76, gfx::Rgba{0, 0, 0, 0.7f});
    for (int i = 0; i < 4; ++i)
        renderer_.text(8, 4 + i * 17.0f, 1, gfx::Rgba{1.0f, 0.85f, 0.3f, 1}, buf[i]);
    renderer_.end2D();
}

void Game::drawMenu(int width, int height) {
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float s = std::max(1.0f, std::floor(h / 360.0f));
    const std::vector<std::string> items = menuItems();
    renderer_.begin2D(width, height);
    const bool overWorld = screen_ == Screen::Paused ||
                           (screen_ == Screen::SettingsMenu && menuReturn_ == Screen::Paused);
    if (overWorld) renderer_.rect(0, 0, w, h, gfx::Rgba{0, 0, 0, 0.72f});

    const gfx::Rgba green{0.475f, 1.0f, 0.557f, 1}, dim{0.26f, 0.60f, 0.33f, 1}, faint{0.16f, 0.36f, 0.2f, 1};
    const char* title = screen_ == Screen::MainMenu ? "В ПРЕДЕЛАХ НОРМЫ"
                        : screen_ == Screen::Paused ? "ПАУЗА"
                                                    : "НАСТРОЙКИ";
    renderer_.textCentered(w * 0.5f, h * 0.20f, s * 3, green, title);
    if (screen_ == Screen::MainMenu)
        renderer_.textCentered(w * 0.5f, h * 0.20f + 54 * s, s, faint, "прототип 0.2");

    float y = h * 0.44f;
    for (size_t i = 0; i < items.size(); ++i) {
        const bool chosen = static_cast<int>(i) == menuIndex_;
        renderer_.textCentered(w * 0.5f, y, s * 1.0f * (chosen ? 1.0f : 1.0f), chosen ? green : dim,
                               chosen ? "> " + items[i] + " <" : items[i]);
        y += 24 * s;
    }
    renderer_.textCentered(w * 0.5f, h - 40 * s, s, faint,
                           screen_ == Screen::SettingsMenu ? "W/S — выбор   A/D или Enter — изменить   Esc — назад"
                                                           : "W/S — выбор   Enter — подтвердить");
    if (!notice_.empty() && screen_ == Screen::MainMenu)
        renderer_.textCentered(w * 0.5f, h - 64 * s, s, gfx::Rgba{1.0f, 0.6f, 0.3f, 1}, notice_);
    renderer_.end2D();
}

void Game::drawEnding(int width, int height) {
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float s = std::max(1.0f, std::floor(h / 360.0f));
    renderer_.begin2D(width, height);
    const float appear = clamp01((endingTime_ - 1.2f) / 1.5f);
    const int lived = std::max(0, endingDay_ - 1);
    if (endingGood_) {
        renderer_.rect(0, 0, w, h, gfx::Rgba{1, 1, 0.96f, 1});
        const gfx::Rgba ink{0.1f, 0.12f, 0.1f, appear};
        renderer_.textCentered(w * 0.5f, h * 0.30f, s * 3, ink, "ДЕНЬ " + std::to_string(endingDay_));
        renderer_.textCentered(w * 0.5f, h * 0.48f, s, ink, "Дверь камеры открыл человек.");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 22 * s, s, ink, "Снаружи день.");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 56 * s, s, ink,
                               "Прожито дней в комплексе: " + std::to_string(lived));
        if (endingTime_ > 3.0f)
            renderer_.textCentered(w * 0.5f, h - 40 * s, s, gfx::Rgba{0.3f, 0.34f, 0.3f, 1}, "Enter — в главное меню");
    } else if (endingKilled_) {
        renderer_.rect(0, 0, w, h, gfx::Rgba{0, 0, 0, 1});
        const gfx::Rgba red{0.78f, 0.12f, 0.10f, appear};
        const gfx::Rgba text{0.62f, 0.60f, 0.58f, appear};
        renderer_.textCentered(w * 0.5f, h * 0.30f, s * 3, red, "НОМЕР СПИСАН");
        renderer_.textCentered(w * 0.5f, h * 0.48f, s, text, "Пленный не выполнил распоряжение конвоира.");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 22 * s, s, text, "День " + std::to_string(endingDay_) + ".");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 56 * s, s, text, "Прожито дней: " + std::to_string(lived));
        if (endingTime_ > 3.0f)
            renderer_.textCentered(w * 0.5f, h - 40 * s, s, gfx::Rgba{0.3f, 0.3f, 0.3f, 1}, "Enter — в главное меню");
    } else {
        const gfx::Rgba text{0.75f, 0.78f, 0.74f, appear};
        renderer_.textCentered(w * 0.5f, h * 0.30f, s * 3, text, "ДЕНЬ " + std::to_string(endingDay_));
        renderer_.textCentered(w * 0.5f, h * 0.48f, s, text,
                               endingLow_ ? "Рабочее назначение отменено." : "Индивидуальное собеседование.");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 22 * s, s, text,
                               endingLow_ ? "Комплекс счёл вас непригодным к работе."
                                          : "Комплекс счёл вас потенциальной угрозой.");
        renderer_.textCentered(w * 0.5f, h * 0.48f + 56 * s, s, text, "Прожито дней: " + std::to_string(lived));
        if (endingTime_ > 3.0f)
            renderer_.textCentered(w * 0.5f, h - 40 * s, s, gfx::Rgba{0.3f, 0.34f, 0.3f, 1}, "Enter — в главное меню");
    }
    renderer_.end2D();
}

} // namespace game
