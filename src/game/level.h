// Комплекс: план помещений, геометрия, коллизии, двери, источники света,
// предметы для взаимодействия. План задаётся текстовой сеткой в level.cpp.
#pragma once

#include <string>
#include <vector>

#include "../core/tasks.h"
#include "../engine/renderer.h"

namespace game {

// --- ключевые точки маршрутов (метры; x — на восток, z — на юг) ---
namespace spot {
const Vec3 kBunkWake{6.55f, 0, 12.6f};      // игрок встаёт с койки
const Vec3 kEscortAtCell{7.7f, 0, 9.0f};     // конвоир ждёт у двери камеры
const Vec3 kCorridorAtCell{6.5f, 0, 9.0f};
const Vec3 kFork{12.5f, 0, 9.0f};            // развилка
const Vec3 kTestNeck{12.5f, 0, 7.5f};
const Vec3 kTestInside{12.5f, 0, 5.2f};
const Vec3 kEscortPost{14.3f, 0, 5.0f};      // место конвоира в помещении для тестов
const Vec3 kBadNeck{12.5f, 0, 11.4f};
const Vec3 kBadInside{12.5f, 0, 13.9f};
const Vec3 kExitLight{1.3f, 0, 9.5f};        // дневной свет у наружной двери
const Vec3 kHumanAtCell{7.4f, 0, 9.2f};      // человек, открывший камеру
// Положение камеры перед экраном монитора.
const Vec3 kMonitorEye{12.5f, 0.9777f, 1.757f};
} // namespace spot

enum DoorId { DoorCell, DoorGate, DoorBad, DoorExit, kDoorCount };

struct Door {
    Vec3 base;          // центр створки на полу в закрытом положении
    Vec3 slide;         // куда уезжает при открытии (вектор на полную ширину)
    bool alongX = true; // створка стоит поперёк оси Z (ширина по X)
    float open = 0;     // 0 — закрыта, 1 — открыта
    float target = 0;
    float speed = 1.1f;
    bool heavy = false; // звук тяжёлой двери
};

// Чем можно пользоваться клавишей E.
enum InteractId {
    UseNone = -1,
    UseTerminal,
    UseBunk,
    UsePlate,
    UseSealConfirm,
    UseSealMismatch,
    UseSealReader,
    UseSignalNone,
    UseSignalReceived,
    UseSignalRespond,
    UsePowerLever,
    UsePowerMark,
    UsePowerBackup,
    kUseCount
};

struct Interactable {
    InteractId id;
    Vec3 mn, mx;
};

// Источники света; яркостью управляет игра.
enum LightId {
    LightCell,
    LightCellDoor,
    LightFork,
    LightBadDoor,
    LightTestMain,
    LightMonitor,
    LightSeal,
    LightWing,
    LightBadRoom,
    LightDay,
    LightPower,
    LightRobot,
    kLightCount
};

// Отдельно рисуемый предмет: своя текстура, положение и свечение.
struct Object {
    gfx::Mesh mesh;
    gfx::Texture tex;
    Mat4 model;
    Vec3 tint{1, 1, 1};
    float emissive = 0;
    float lit = 1;
    bool visible = true;
};

enum ObjectId {
    ObjScreen,
    ObjPlate,
    ObjInvoice,
    ObjSealTag,
    ObjSealTagNeighbor,
    ObjCrateLabel,
    ObjCrateLabelNeighbor,
    ObjReaderLed,
    ObjBtnSealConfirm,
    ObjBtnSealMismatch,
    ObjBtnSignalNone,
    ObjBtnSignalReceived,
    ObjBtnSignalRespond,
    ObjPowerPanel,
    ObjPowerLever,
    ObjPowerGrip,
    ObjBtnPowerMark,
    ObjBtnPowerBackup,
    ObjLampCell,
    ObjLampCellDoor,
    ObjLampFork,
    ObjLampBadDoor,
    ObjLampTest,
    ObjLampSeal,
    ObjLampWing,
    ObjLampBadRoom,
    ObjLampPower,
    ObjStripCell,
    ObjStripBad,
    ObjStripWing,
    ObjDaylight,
    ObjOuterDoor,
    ObjDoorCell,
    ObjDoorGate,
    ObjDoorBad,
    ObjDoorExit,
    ObjRobotBody,
    ObjRobotDark,
    ObjRobotEye,
    ObjHumanJacket,
    ObjHumanPants,
    ObjHumanSkin,
    kObjectCount
};

class Level {
public:
    void build(int prisonerNumber);

    // Обновляет надписи на накладной, пломбах и ящиках под смену дня.
    void setShiftTextures(const core::Shift& shift);
    void setScreenTexture(const gfx::Texture& tex) { objects[ObjScreen].tex = tex; }
    // Перерисовывает номерную табличку под номер пленного.
    void setPlate(int prisonerNumber);
    // Табло пульта «рубильник»; active — смена идёт, табло горит.
    void setPowerPanel(const core::Shift& shift, bool active);
    // Положение рубильника: 0 — поднят, 1 — опущен.
    void setLever(float down);

    // --- коллизии ---
    bool solidCell(int cx, int cz) const;
    // Пересекает ли круг (x, z, r) стены, мебель или закрытые двери.
    bool blocked(float x, float z, float r) const;
    // Двигает точку с учётом препятствий; скользит вдоль стен.
    void moveWithCollision(float* x, float* z, float dx, float dz, float r) const;
    // Виден ли отрезок без стен на пути (для взаимодействия через «прицел»).
    bool lineClear(const Vec3& from, const Vec3& to) const;

    // Какой предмет под прицелом, не дальше reach метров.
    InteractId pick(const Vec3& eye, const Vec3& dir, float reach) const;

    void updateDoors(float dt);
    Vec3 doorPos(int id) const { return doors[id].base + doors[id].slide * doors[id].open; }

    void draw(gfx::Renderer& r) const;

    Door doors[kDoorCount];
    gfx::Light lights[kLightCount];
    Object objects[kObjectCount];
    std::vector<Interactable> interactables;

    // Положение конвоира и человека задаёт игра перед отрисовкой.
    // eyeOpen > 1 — глаз-щель раскрывается (скример).
    void placeRobot(const Vec3& pos, float yaw, float bob, float eyeGlow, float eyeOpen = 1.0f);
    void placeHuman(const Vec3& pos, float yaw, bool visible);

private:
    struct Box2 {
        float x0, z0, x1, z1;
    };
    std::vector<Box2> colliders_; // мебель
    struct StaticBatch {
        gfx::Mesh mesh;
        gfx::Texture tex;
    };
    std::vector<StaticBatch> statics_;

    gfx::Texture texCrateLabel_[2];
    gfx::Texture texSealTag_[2];
    gfx::Texture texInvoice_;
    gfx::Texture texPlate_;
    gfx::Texture texPower_;
    int powerShown_ = -1; // что сейчас нарисовано на табло (чтобы не перерисовывать каждый кадр)
};

} // namespace game
