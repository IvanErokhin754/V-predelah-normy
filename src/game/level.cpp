#include "level.h"

#include <algorithm>
#include <cmath>

#include "../engine/texgen.h"

namespace game {

namespace {

// План комплекса. Клетка — 1x1 м. Север сверху.
//   #  стена          .  пол
//   C  дверь камеры   G  шлюз (решётка) в помещение для тестов
//   B  дверь комнаты плохой концовки
//   E  решётка выхода
const int kMapW = 25;
const int kMapH = 16;
const char* const kMap[kMapH] = {
    "#########################", //  0
    "#######.........#########", //  1  помещение для тестов: x 7..15, z 1..5
    "#######.........#########", //  2
    "#######.........#########", //  3
    "#######.........#########", //  4
    "#######.........#########", //  5
    "############G############", //  6  шлюз
    "############.############", //  7
    "#####.........###########", //  8  коридор: x 5..13, z 8..9
    "#...E.........###########", //  9  слева — проход к наружной двери
    "######C#####B############", // 10  дверь камеры и дверь на развилке
    "#####...####.############", // 11  камера: x 5..7, z 11..14
    "#####...###...###########", // 12  комната плохой концовки: x 11..13, z 12..14
    "#####...###...###########", // 13
    "#####...###...###########", // 14
    "#########################", // 15
};

const float kWallH = 3.0f;   // высота потолка
const float kDoorH = 2.2f;   // высота дверного проёма
const float kPaintH = 1.1f;  // высота окрашенной полосы
const float kTile = 0.5f;    // грубые текстуры: один повтор на 2 м (32 px на метр)

char cellAt(int x, int z) {
    if (x < 0 || z < 0 || x >= kMapW || z >= kMapH) return '#';
    return kMap[z][x];
}

bool isSolid(int x, int z) { return cellAt(x, z) == '#'; }

// Клетки с низким потолком: двери и короткие проходы за ними.
bool isLow(int x, int z) {
    const char c = cellAt(x, z);
    if (c == 'C' || c == 'G' || c == 'B' || c == 'E') return true;
    return (x == 12 && z == 7) || (x == 12 && z == 11);
}

// Голый бетон без краски: комната плохой концовки и проход к выходу.
bool isBare(int x, int z) { return (x >= 11 && z >= 11) || (x <= 4 && z == 9); }

enum Mat {
    MConcrete,
    MPaint,
    MFloor,
    MCeiling,
    MSteel,
    MBlanket,
    MPillow,
    MTable,
    MMonCase,
    MMonBezel,
    MKeyboard,
    MCrate,
    MSpeaker,
    MWingDoor,
    MPanelSeal,
    MPanelWing,
    MDark,
    kMatCount
};

gfx::Mesh boxMesh(const Vec3& mn, const Vec3& mx, unsigned faces = gfx::MeshBuilder::AllFaces) {
    gfx::MeshBuilder b;
    b.box(mn, mx, 0, faces);
    return gfx::uploadMesh(b);
}

// Решётка: вертикальные прутья и рама. Локальные координаты, ширина по X.
gfx::Mesh grateMesh() {
    gfx::MeshBuilder b;
    for (int i = 0; i < 9; ++i) {
        const float x = -0.44f + i * 0.11f;
        b.box({x - 0.02f, 0, -0.02f}, {x + 0.02f, kDoorH, 0.02f}, 2);
    }
    b.box({-0.5f, 0, -0.03f}, {0.5f, 0.06f, 0.03f}, 2);
    b.box({-0.5f, 1.05f, -0.03f}, {0.5f, 1.11f, 0.03f}, 2);
    b.box({-0.5f, kDoorH - 0.06f, -0.03f}, {0.5f, kDoorH, 0.03f}, 2);
    b.box({-0.5f, 0, -0.03f}, {-0.46f, kDoorH, 0.03f}, 2);
    b.box({0.46f, 0, -0.03f}, {0.5f, kDoorH, 0.03f}, 2);
    return gfx::uploadMesh(b);
}

bool circleHitsBox(float x, float z, float r, float x0, float z0, float x1, float z1) {
    const float cx = std::clamp(x, x0, x1);
    const float cz = std::clamp(z, z0, z1);
    const float dx = x - cx, dz = z - cz;
    return dx * dx + dz * dz < r * r;
}

// Пересечение луча с коробкой; возвращает расстояние или -1.
float rayBox(const Vec3& o, const Vec3& d, const Vec3& mn, const Vec3& mx) {
    float t0 = 0, t1 = 1e9f;
    const float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z};
    const float lo[3] = {mn.x, mn.y, mn.z}, hi[3] = {mx.x, mx.y, mx.z};
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(dd[a]) < 1e-6f) {
            if (oo[a] < lo[a] || oo[a] > hi[a]) return -1;
            continue;
        }
        float ta = (lo[a] - oo[a]) / dd[a];
        float tb = (hi[a] - oo[a]) / dd[a];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return -1;
    }
    return t0;
}

} // namespace

void Level::build(int prisonerNumber) {
    gfx::Texture tex[kMatCount];
    tex[MConcrete] = gfx::makeTexture(texgen::concrete(), true, true);
    tex[MPaint] = gfx::makeTexture(texgen::paint(), true, true);
    tex[MFloor] = gfx::makeTexture(texgen::floorTiles(), true, true);
    tex[MCeiling] = gfx::makeTexture(texgen::ceiling(), true, true);
    tex[MSteel] = gfx::makeTexture(texgen::steel(), true, true);
    tex[MBlanket] = gfx::makeTexture(texgen::blanket(), true, true);
    tex[MPillow] = gfx::makeTexture(texgen::pillow(), true, false);
    tex[MTable] = gfx::makeTexture(texgen::tableTop(), true, false);
    tex[MMonCase] = gfx::makeTexture(texgen::monitorCase(), true, false);
    tex[MMonBezel] = gfx::makeTexture(texgen::monitorBezel(), false, false);
    tex[MKeyboard] = gfx::makeTexture(texgen::keyboard(), true, false);
    tex[MCrate] = gfx::makeTexture(texgen::crate(), true, false);
    tex[MSpeaker] = gfx::makeTexture(texgen::speaker(), true, false);
    tex[MWingDoor] = gfx::makeTexture(texgen::wingDoor(), true, false);
    tex[MPanelSeal] = gfx::makeTexture(texgen::controlPanel("СВЕРКА ПЛОМБ"), false, false);
    tex[MPanelWing] = gfx::makeTexture(texgen::controlPanel("ПУСТОЕ КРЫЛО · СВЯЗЬ"), false, false);
    tex[MDark] = gfx::makeTexture(texgen::flat(Color{26, 28, 28, 255}), true, true);

    gfx::MeshBuilder mb[kMatCount];
    const unsigned All = gfx::MeshBuilder::AllFaces;

    // ---------- стены, пол и потолок по сетке ----------
    for (int z = 0; z < kMapH; ++z) {
        for (int x = 0; x < kMapW; ++x) {
            if (isSolid(x, z)) continue;
            const float fx = static_cast<float>(x), fz = static_cast<float>(z);
            const bool low = isLow(x, z);
            const float top = low ? kDoorH : kWallH;

            mb[MFloor].quad({fx + 0.5f, 0, fz + 0.5f}, {0.5f, 0, 0}, {0, 0, -0.5f}, {0, 1, 0},
                            fx * kTile, fz * kTile, (fx + 1) * kTile, (fz + 1) * kTile);
            if (low) {
                // Перемычка над проходом.
                mb[MConcrete].box({fx, kDoorH, fz}, {fx + 1, kWallH, fz + 1}, kTile);
            } else {
                mb[MCeiling].quad({fx + 0.5f, kWallH, fz + 0.5f}, {0.5f, 0, 0}, {0, 0, 0.5f},
                                  {0, -1, 0}, fx * kTile, fz * kTile, (fx + 1) * kTile,
                                  (fz + 1) * kTile);
            }

            // Стена на границе с каждой соседней сплошной клеткой.
            struct Side {
                int dx, dz;
                Vec3 center, right, normal;
                float u0;
            };
            const Side sides[4] = {
                {1, 0, {fx + 1, 0, fz + 0.5f}, {0, 0, 0.5f}, {-1, 0, 0}, fz * kTile},
                {-1, 0, {fx, 0, fz + 0.5f}, {0, 0, -0.5f}, {1, 0, 0}, -(fz + 1) * kTile},
                {0, 1, {fx + 0.5f, 0, fz + 1}, {-0.5f, 0, 0}, {0, 0, -1}, -(fx + 1) * kTile},
                {0, -1, {fx + 0.5f, 0, fz}, {0.5f, 0, 0}, {0, 0, 1}, fx * kTile},
            };
            const float paintTop = isBare(x, z) ? 0.0f : kPaintH;
            for (const Side& s : sides) {
                if (!isSolid(x + s.dx, z + s.dz)) continue;
                if (paintTop > 0) {
                    Vec3 c = s.center;
                    c.y = paintTop * 0.5f;
                    mb[MPaint].quad(c, s.right, {0, paintTop * 0.5f, 0}, s.normal, s.u0, 0,
                                    s.u0 + kTile, 1);
                }
                Vec3 c = s.center;
                c.y = (paintTop + top) * 0.5f;
                mb[MConcrete].quad(c, s.right, {0, (top - paintTop) * 0.5f, 0}, s.normal, s.u0,
                                   (kWallH - top) * kTile, s.u0 + kTile,
                                   (kWallH - paintTop) * kTile);
            }
        }
    }

    // ---------- камера ----------
    // Койка у западной стены.
    mb[MSteel].box({5.0f, 0.25f, 12.0f}, {5.95f, 0.32f, 13.95f}, 2);
    for (float lx : {5.02f, 5.87f})
        for (float lz : {12.02f, 13.87f})
            mb[MSteel].box({lx, 0, lz}, {lx + 0.06f, 0.25f, lz + 0.06f}, 2);
    mb[MBlanket].box({5.02f, 0.32f, 12.02f}, {5.93f, 0.45f, 13.93f}, 1);
    mb[MPillow].box({5.12f, 0.45f, 12.08f}, {5.83f, 0.55f, 12.48f});
    colliders_.push_back({5.0f, 12.0f, 5.95f, 13.95f});

    // ---------- помещение для тестов ----------
    // Стол с монитором у северной стены.
    mb[MTable].box({11.6f, 0.70f, 1.0f}, {13.4f, 0.75f, 1.85f});
    for (float lx : {11.64f, 13.30f})
        for (float lz : {1.04f, 1.75f})
            mb[MSteel].box({lx, 0, lz}, {lx + 0.06f, 0.70f, lz + 0.06f}, 2);
    colliders_.push_back({11.6f, 1.0f, 13.4f, 1.85f});
    // Корпус ЭЛТ-монитора: рамка спереди, пластик по бокам.
    mb[MMonBezel].box({12.27f, 0.75f, 1.10f}, {12.73f, 1.15f, 1.50f}, 0, gfx::MeshBuilder::PosZ);
    mb[MMonCase].box({12.27f, 0.75f, 1.10f}, {12.73f, 1.15f, 1.50f}, 0, All & ~gfx::MeshBuilder::PosZ);
    // Клавиатура.
    mb[MKeyboard].box({12.30f, 0.75f, 1.58f}, {12.70f, 0.78f, 1.73f}, 0, gfx::MeshBuilder::PosY);
    mb[MMonCase].box({12.30f, 0.75f, 1.58f}, {12.70f, 0.78f, 1.73f}, 0, All & ~gfx::MeshBuilder::PosY);
    // Динамик над столом.
    mb[MSpeaker].box({12.32f, 2.15f, 1.0f}, {12.68f, 2.50f, 1.08f});

    // Оборудование смены «пломба»: два ящика, пульт и накладная на восточной стене.
    mb[MCrate].box({14.9f, 0, 1.2f}, {15.9f, 1.0f, 2.2f});
    mb[MCrate].box({14.9f, 0, 2.5f}, {15.9f, 1.0f, 3.5f});
    colliders_.push_back({14.9f, 1.2f, 15.9f, 2.2f});
    colliders_.push_back({14.9f, 2.5f, 15.9f, 3.5f});
    mb[MPanelSeal].box({15.8f, 0.9f, 4.0f}, {16.0f, 1.3f, 5.0f}, 0, gfx::MeshBuilder::NegX);
    mb[MSteel].box({15.8f, 0.9f, 4.0f}, {16.0f, 1.3f, 5.0f}, 2, All & ~gfx::MeshBuilder::NegX);
    mb[MSteel].box({15.86f, 0, 4.4f}, {15.96f, 0.9f, 4.6f}, 2);
    colliders_.push_back({15.8f, 4.0f, 16.0f, 5.0f});
    // Считыватель на соседнем ящике.
    mb[MDark].box({15.05f, 1.0f, 2.85f}, {15.35f, 1.07f, 3.15f});

    // Оборудование смены «сигнал»: дверь пустого крыла, панель связи, динамик.
    mb[MWingDoor].box({7.0f, 0, 2.5f}, {7.06f, kDoorH, 3.5f}, 0, gfx::MeshBuilder::PosX);
    mb[MSteel].box({7.0f, 0, 2.42f}, {7.08f, kDoorH + 0.08f, 2.5f}, 2);
    mb[MSteel].box({7.0f, 0, 3.5f}, {7.08f, kDoorH + 0.08f, 3.58f}, 2);
    mb[MSteel].box({7.0f, kDoorH, 2.5f}, {7.08f, kDoorH + 0.08f, 3.5f}, 2);
    mb[MPanelWing].box({7.0f, 0.95f, 3.85f}, {7.08f, 1.80f, 4.70f}, 0, gfx::MeshBuilder::PosX);
    mb[MSteel].box({7.0f, 0.95f, 3.85f}, {7.08f, 1.80f, 4.70f}, 2, All & ~gfx::MeshBuilder::PosX);
    mb[MSpeaker].box({7.0f, 2.35f, 3.95f}, {7.08f, 2.65f, 4.25f});

    // Оборудование смены «рубильник»: пульт на северной стене, слева от стола.
    mb[MSteel].box({9.55f, 0.95f, 1.0f}, {9.85f, 1.65f, 1.10f}, 2); // короб рубильника
    mb[MDark].box({9.62f, 1.02f, 1.10f}, {9.78f, 1.58f, 1.11f});    // прорезь
    mb[MSteel].box({8.32f, 0.55f, 1.0f}, {9.48f, 0.86f, 1.06f}, 2);  // полка с кнопками

    // ---------- комната плохой концовки ----------
    // Один стул посреди пустой комнаты.
    mb[MSteel].box({12.27f, 0.42f, 13.15f}, {12.73f, 0.46f, 13.60f}, 2);
    mb[MSteel].box({12.27f, 0.46f, 13.56f}, {12.73f, 0.95f, 13.60f}, 2);
    for (float lx : {12.27f, 12.69f})
        for (float lz : {13.15f, 13.56f})
            mb[MSteel].box({lx, 0, lz}, {lx + 0.04f, 0.42f, lz + 0.04f}, 2);
    colliders_.push_back({12.27f, 13.15f, 12.73f, 13.60f});

    for (int m = 0; m < kMatCount; ++m)
        if (!mb[m].empty()) statics_.push_back({gfx::uploadMesh(mb[m]), tex[m]});

    // ---------- отдельные предметы ----------
    const gfx::Texture white = gfx::makeTexture(texgen::flat(Color{255, 255, 255, 255}), true, false);

    // Экран монитора: на него выводится текстура терминала.
    objects[ObjScreen].mesh =
        boxMesh({12.3104f, 0.83661f, 1.500f}, {12.6896f, 1.11875f, 1.502f}, gfx::MeshBuilder::PosZ);
    objects[ObjScreen].emissive = 1.0f;
    objects[ObjScreen].lit = 0.0f;

    // Номерная табличка в камере, справа от двери.
    objects[ObjPlate].mesh = boxMesh({7.15f, 1.45f, 11.0f}, {7.65f, 1.70f, 11.02f});
    texPlate_ = gfx::makeTexture(texgen::numberPlate(12, prisonerNumber), false, false);
    objects[ObjPlate].tex = texPlate_;

    // Накладная и пломбы; надписи обновляются под смену.
    objects[ObjInvoice].mesh =
        boxMesh({15.985f, 1.40f, 4.34f}, {16.0f, 1.80f, 4.66f}, gfx::MeshBuilder::NegX);
    texInvoice_ = gfx::makeTexture(texgen::invoice(12, 317), false, false);
    objects[ObjInvoice].tex = texInvoice_;
    const float crateZ[2] = {1.7f, 3.0f};
    for (int i = 0; i < 2; ++i) {
        const float z = crateZ[i];
        texSealTag_[i] = gfx::makeTexture(texgen::sealTag(371), true, false);
        texCrateLabel_[i] = gfx::makeTexture(texgen::crateLabel(12 + i), true, false);
        Object& tag = objects[i == 0 ? ObjSealTag : ObjSealTagNeighbor];
        tag.mesh = boxMesh({14.885f, 0.62f, z - 0.16f}, {14.9f, 0.78f, z + 0.16f}, gfx::MeshBuilder::NegX);
        tag.tex = texSealTag_[i];
        Object& label = objects[i == 0 ? ObjCrateLabel : ObjCrateLabelNeighbor];
        label.mesh = boxMesh({14.885f, 0.26f, z - 0.16f}, {14.9f, 0.42f, z + 0.16f}, gfx::MeshBuilder::NegX);
        label.tex = texCrateLabel_[i];
    }
    objects[ObjReaderLed].mesh = boxMesh({15.26f, 1.07f, 2.90f}, {15.31f, 1.085f, 2.95f});
    objects[ObjReaderLed].tex = white;
    objects[ObjReaderLed].tint = {0.3f, 1.0f, 0.4f};

    // Пульт «рубильник»: табло обновляется под смену.
    objects[ObjPowerPanel].mesh = boxMesh({8.30f, 0.92f, 1.0f}, {9.50f, 1.92f, 1.06f});
    texPower_ = gfx::makeTexture(texgen::powerPanel(3, 2, false), false, false);
    objects[ObjPowerPanel].tex = texPower_;
    {
        // Рычаг: ось на уровне 1.3 м, в покое смотрит вверх.
        gfx::MeshBuilder arm, grip;
        arm.box({-0.025f, 0.0f, -0.025f}, {0.025f, 0.30f, 0.025f}, 2);
        arm.box({-0.06f, -0.03f, -0.03f}, {0.06f, 0.03f, 0.03f}, 2);
        grip.box({-0.045f, 0.28f, -0.045f}, {0.045f, 0.40f, 0.045f});
        objects[ObjPowerLever].mesh = gfx::uploadMesh(arm);
        objects[ObjPowerLever].tex = tex[MSteel];
        objects[ObjPowerGrip].mesh = gfx::uploadMesh(grip);
        objects[ObjPowerGrip].tex = gfx::makeTexture(texgen::flat(Color{150, 34, 28, 255}), true, false);
        setLever(0);
    }
    interactables.push_back({UsePowerLever, {9.50f, 1.0f, 1.0f}, {9.90f, 1.85f, 1.55f}});

    // Кнопки. Каждая — отдельный предмет со своей надписью.
    struct ButtonDef {
        ObjectId obj;
        InteractId use;
        const char* text;
        Color color;
        Vec3 mn, mx;
        unsigned face;
    };
    const Color kGreen{52, 96, 60, 255}, kAmber{132, 96, 34, 255}, kRed{120, 44, 38, 255},
        kBlue{44, 70, 110, 255};
    const ButtonDef buttons[] = {
        {ObjBtnSealConfirm, UseSealConfirm, "ПОДТВЕРДИТЬ", kGreen, {15.77f, 1.02f, 4.06f}, {15.8f, 1.18f, 4.48f}, gfx::MeshBuilder::NegX},
        {ObjBtnSealMismatch, UseSealMismatch, "РАСХОЖДЕНИЕ", kAmber, {15.77f, 1.02f, 4.52f}, {15.8f, 1.18f, 4.94f}, gfx::MeshBuilder::NegX},
        {ObjBtnSignalNone, UseSignalNone, "СИГНАЛА НЕТ", kRed, {7.08f, 1.50f, 4.065f}, {7.11f, 1.66f, 4.485f}, gfx::MeshBuilder::PosX},
        {ObjBtnSignalReceived, UseSignalReceived, "СИГНАЛ ПРИНЯТ", kGreen, {7.08f, 1.29f, 4.065f}, {7.11f, 1.45f, 4.485f}, gfx::MeshBuilder::PosX},
        {ObjBtnSignalRespond, UseSignalRespond, "ЗАПРОС СВЯЗИ", kBlue, {7.08f, 1.08f, 4.065f}, {7.11f, 1.24f, 4.485f}, gfx::MeshBuilder::PosX},
        {ObjBtnPowerMark, UsePowerMark, "СЕКТОР ЗАНЯТ", kAmber, {8.38f, 0.62f, 1.06f}, {8.86f, 0.80f, 1.09f}, gfx::MeshBuilder::PosZ},
        {ObjBtnPowerBackup, UsePowerBackup, "РЕЗЕРВ", kBlue, {8.94f, 0.62f, 1.06f}, {9.42f, 0.80f, 1.09f}, gfx::MeshBuilder::PosZ},
    };
    for (const ButtonDef& b : buttons) {
        objects[b.obj].mesh = boxMesh(b.mn, b.mx);
        objects[b.obj].tex = gfx::makeTexture(texgen::buttonLabel(b.text, b.color), false, false);
        interactables.push_back({b.use, b.mn, b.mx});
    }

    // Лампы: светящиеся коробки, яркость задаёт игра.
    struct LampDef {
        ObjectId obj;
        Vec3 mn, mx;
    };
    const LampDef lamps[] = {
        {ObjLampCell, {6.3f, 2.92f, 12.6f}, {6.7f, 3.0f, 13.0f}},
        {ObjLampCellDoor, {6.35f, 2.30f, 9.92f}, {6.65f, 2.42f, 10.0f}},
        {ObjLampFork, {12.35f, 2.30f, 8.0f}, {12.65f, 2.42f, 8.08f}},
        {ObjLampBadDoor, {12.35f, 2.30f, 9.92f}, {12.65f, 2.42f, 10.0f}},
        {ObjLampTest, {11.2f, 2.92f, 3.0f}, {11.8f, 3.0f, 3.4f}},
        {ObjLampSeal, {15.0f, 2.92f, 2.6f}, {15.4f, 3.0f, 3.0f}},
        {ObjLampWing, {7.0f, 2.36f, 2.88f}, {7.07f, 2.46f, 3.12f}},
        {ObjLampBadRoom, {12.3f, 2.92f, 13.3f}, {12.7f, 3.0f, 13.7f}},
        {ObjLampPower, {8.75f, 2.10f, 1.0f}, {9.05f, 2.20f, 1.08f}},
    };
    for (const LampDef& l : lamps) {
        objects[l.obj].mesh = boxMesh(l.mn, l.mx);
        objects[l.obj].tex = white;
        objects[l.obj].lit = 0.0f;
    }

    // Свет из-под дверей: пятно на полу, которое гаснет к краю и
    // рассыпается на пиксели. Верхний край текстуры — у самой двери.
    {
        const gfx::Texture spill = gfx::makeTexture(texgen::lightSpill(), true, false);
        auto floorSpill = [&](ObjectId id, const Vec3& center, const Vec3& halfRight, const Vec3& towardDoor) {
            gfx::MeshBuilder b;
            b.quad(center, halfRight, towardDoor, {0, 1, 0}, 0, 0, 1, 1);
            objects[id].mesh = gfx::uploadMesh(b);
            objects[id].tex = spill;
            objects[id].lit = 0.0f;
        };
        // В камеру — свет коридора; в коридор — свет комнаты за развилкой;
        // в помещение для тестов — из пустого крыла.
        floorSpill(ObjStripCell, {6.5f, 0.004f, 10.80f}, {0.46f, 0, 0}, {0, 0, -0.24f});
        floorSpill(ObjStripBad, {12.5f, 0.004f, 10.20f}, {-0.46f, 0, 0}, {0, 0, 0.24f});
        floorSpill(ObjStripWing, {7.30f, 0.004f, 3.0f}, {0, 0, -0.46f}, {-0.24f, 0, 0});
    }

    // Наружная дверь: закрытая створка или дневной свет.
    objects[ObjOuterDoor].mesh = boxMesh({1.0f, 0, 9.0f}, {1.08f, kDoorH, 10.0f});
    objects[ObjOuterDoor].tex = gfx::makeTexture(texgen::cellDoor(), true, false);
    objects[ObjDaylight].mesh = boxMesh({1.0f, 0, 9.0f}, {1.03f, kDoorH, 10.0f});
    objects[ObjDaylight].tex = white;
    objects[ObjDaylight].lit = 0.0f;
    objects[ObjDaylight].emissive = 1.6f;
    objects[ObjDaylight].visible = false;

    // Двери. Локальные координаты створки: ширина по X, низ на полу.
    const Vec3 slabMin{-0.5f, 0, -0.06f}, slabMax{0.5f, kDoorH, 0.06f};
    objects[ObjDoorCell].mesh = boxMesh(slabMin, slabMax);
    objects[ObjDoorCell].tex = gfx::makeTexture(texgen::cellDoor(), true, false);
    objects[ObjDoorBad].mesh = boxMesh(slabMin, slabMax);
    objects[ObjDoorBad].tex = gfx::makeTexture(texgen::heavyDoor(), true, false);
    objects[ObjDoorGate].mesh = grateMesh();
    objects[ObjDoorGate].tex = tex[MSteel];
    objects[ObjDoorExit].mesh = grateMesh();
    objects[ObjDoorExit].tex = tex[MSteel];

    doors[DoorCell] = Door{{6.5f, 0, 10.5f}, {0.98f, 0, 0}, true, 0, 0, 1.1f, false};
    doors[DoorGate] = Door{{12.5f, 0, 6.5f}, {0.98f, 0, 0}, true, 0, 0, 1.0f, false};
    doors[DoorBad] = Door{{12.5f, 0, 10.5f}, {0.98f, 0, 0}, true, 0, 0, 0.45f, true};
    doors[DoorExit] = Door{{4.5f, 0, 9.5f}, {0, 0, -0.98f}, false, 0, 0, 1.0f, false};

    // Конвоир. Локально смотрит на -Z, начало координат — у пола.
    {
        gfx::MeshBuilder body, dark, eye;
        dark.box({-0.32f, 0, -0.40f}, {0.32f, 0.28f, 0.40f}, 2);       // гусеничная база
        body.box({-0.27f, 0.28f, -0.20f}, {0.27f, 1.25f, 0.20f});      // корпус
        dark.box({-0.42f, 1.05f, -0.14f}, {0.42f, 1.25f, 0.14f}, 2);   // плечи
        dark.box({-0.42f, 0.45f, -0.08f}, {-0.30f, 1.05f, 0.08f}, 2);  // руки
        dark.box({0.30f, 0.45f, -0.08f}, {0.42f, 1.05f, 0.08f}, 2);
        dark.box({-0.07f, 1.25f, -0.07f}, {0.07f, 1.38f, 0.07f}, 2);   // шея
        body.box({-0.20f, 1.38f, -0.18f}, {0.20f, 1.68f, 0.18f});      // голова
        eye.box({-0.14f, 1.49f, -0.195f}, {0.14f, 1.56f, -0.18f});     // глаз-щель
        objects[ObjRobotBody].mesh = gfx::uploadMesh(body);
        objects[ObjRobotBody].tex = gfx::makeTexture(texgen::robotBody(), true, false);
        objects[ObjRobotDark].mesh = gfx::uploadMesh(dark);
        objects[ObjRobotDark].tex = gfx::makeTexture(texgen::robotDark(), true, true);
        objects[ObjRobotEye].mesh = gfx::uploadMesh(eye);
        objects[ObjRobotEye].tex = gfx::makeTexture(texgen::robotEye(), true, false);
        objects[ObjRobotEye].tint = {1.0f, 0.12f, 0.08f};
        objects[ObjRobotEye].lit = 0.0f;
    }

    // Человек снаружи (хорошая концовка).
    {
        gfx::MeshBuilder jacket, pants, skin;
        pants.box({-0.17f, 0, -0.09f}, {-0.03f, 0.85f, 0.09f});
        pants.box({0.03f, 0, -0.09f}, {0.17f, 0.85f, 0.09f});
        jacket.box({-0.22f, 0.85f, -0.13f}, {0.22f, 1.45f, 0.13f});
        jacket.box({-0.32f, 0.90f, -0.08f}, {-0.22f, 1.42f, 0.08f});
        jacket.box({0.22f, 0.90f, -0.08f}, {0.32f, 1.42f, 0.08f});
        skin.box({-0.05f, 1.45f, -0.05f}, {0.05f, 1.52f, 0.05f});
        skin.box({-0.11f, 1.50f, -0.11f}, {0.11f, 1.74f, 0.11f});
        jacket.box({-0.125f, 1.70f, -0.125f}, {0.125f, 1.80f, 0.125f}); // каска
        objects[ObjHumanJacket].mesh = gfx::uploadMesh(jacket);
        objects[ObjHumanJacket].tex =
            gfx::makeTexture(texgen::cloth(Color{196, 104, 36, 255}, Color{220, 220, 200, 255}), true, false);
        objects[ObjHumanPants].mesh = gfx::uploadMesh(pants);
        objects[ObjHumanPants].tex =
            gfx::makeTexture(texgen::cloth(Color{52, 58, 72, 255}, Color{44, 50, 62, 255}), true, false);
        objects[ObjHumanSkin].mesh = gfx::uploadMesh(skin);
        objects[ObjHumanSkin].tex = gfx::makeTexture(texgen::flat(Color{196, 152, 120, 255}), true, false);
        placeHuman({0, 0, 0}, 0, false);
    }

    // ---------- источники света ----------
    // Коробка источника — помещение, которое он освещает (с небольшим запасом).
    auto light = [&](LightId id, Vec3 pos, float radius, Vec3 mn, Vec3 mx) {
        lights[id].pos = pos;
        lights[id].radius = radius;
        lights[id].boxMin = mn - Vec3{0.05f, 0.05f, 0.05f};
        lights[id].boxMax = mx + Vec3{0.05f, 0.05f, 0.05f};
    };
    light(LightCell, {6.5f, 2.75f, 12.8f}, 6.5f, {5, 0, 10.5f}, {8, 3, 15});
    light(LightCellDoor, {6.5f, 2.25f, 9.6f}, 4.5f, {5, 0, 8}, {10, 3, 10.5f});
    light(LightFork, {12.5f, 2.25f, 8.4f}, 5.0f, {9, 0, 6.5f}, {14, 3, 10.5f});
    light(LightBadDoor, {12.5f, 2.25f, 9.6f}, 4.5f, {9.5f, 0, 8}, {14, 3, 10.5f});
    light(LightTestMain, {11.5f, 2.8f, 3.2f}, 9.5f, {7, 0, 1}, {16, 3, 6.5f});
    light(LightMonitor, {12.5f, 1.0f, 1.95f}, 2.4f, {7, 0, 1}, {16, 3, 6.5f});
    light(LightSeal, {15.2f, 2.75f, 2.8f}, 5.0f, {7, 0, 1}, {16, 3, 6.5f});
    light(LightWing, {7.4f, 2.3f, 3.0f}, 3.2f, {7, 0, 1}, {16, 3, 6.5f});
    light(LightBadRoom, {12.5f, 2.75f, 13.5f}, 5.5f, {11, 0, 10.5f}, {14, 3, 15});
    light(LightDay, {1.4f, 1.4f, 9.5f}, 13.0f, {1, 0, 8}, {14, 3, 10});
    light(LightPower, {8.9f, 2.0f, 1.5f}, 3.2f, {7, 0, 1}, {16, 3, 6.5f});
    lights[LightRobot].radius = 1.7f;

    // ---------- предметы для взаимодействия ----------
    interactables.push_back({UseTerminal, {12.2f, 0.72f, 1.05f}, {12.8f, 1.2f, 1.8f}});
    interactables.push_back({UseBunk, {5.0f, 0.2f, 12.0f}, {5.95f, 0.6f, 13.95f}});
    interactables.push_back({UsePlate, {7.1f, 1.40f, 10.98f}, {7.7f, 1.75f, 11.05f}});
    interactables.push_back({UseSealReader, {15.0f, 0.98f, 2.8f}, {15.4f, 1.12f, 3.2f}});
}

void Level::setPlate(int prisonerNumber) {
    gfx::updateTexture(texPlate_, texgen::numberPlate(12, prisonerNumber));
}

void Level::setShiftTextures(const core::Shift& shift) {
    if (shift.kind != core::ShiftKind::Seal) return;
    gfx::updateTexture(texInvoice_, texgen::invoice(shift.crateNo, shift.invoiceSeal));
    gfx::updateTexture(texSealTag_[0], texgen::sealTag(shift.crateSeal));
    gfx::updateTexture(texSealTag_[1], texgen::sealTag(shift.neighborSeal));
    gfx::updateTexture(texCrateLabel_[0], texgen::crateLabel(shift.crateNo));
    gfx::updateTexture(texCrateLabel_[1], texgen::crateLabel(shift.neighborNo));
}

void Level::setPowerPanel(const core::Shift& shift, bool active) {
    const int key = shift.kind == core::ShiftKind::Power ? (shift.powerSector * 10 + shift.powerOccupied) * 2 + (active ? 1 : 0) : 0;
    if (key == powerShown_) return;
    powerShown_ = key;
    if (shift.kind == core::ShiftKind::Power)
        gfx::updateTexture(texPower_, texgen::powerPanel(shift.powerSector, shift.powerOccupied, active));
    else
        gfx::updateTexture(texPower_, texgen::powerPanel(3, 0, false));
}

void Level::setLever(float down) {
    const float angle = 2.5f * clamp01(down); // от «вверх» до «вниз, к себе»
    const Mat4 m = Mat4::translate({9.70f, 1.30f, 1.12f}) * Mat4::rotateX(angle);
    objects[ObjPowerLever].model = m;
    objects[ObjPowerGrip].model = m;
}

bool Level::solidCell(int cx, int cz) const { return isSolid(cx, cz); }

bool Level::blocked(float x, float z, float r) const {
    const int x0 = static_cast<int>(std::floor(x - r)), x1 = static_cast<int>(std::floor(x + r));
    const int z0 = static_cast<int>(std::floor(z - r)), z1 = static_cast<int>(std::floor(z + r));
    for (int cz = z0; cz <= z1; ++cz)
        for (int cx = x0; cx <= x1; ++cx)
            if (isSolid(cx, cz) &&
                circleHitsBox(x, z, r, static_cast<float>(cx), static_cast<float>(cz), cx + 1.0f, cz + 1.0f))
                return true;
    for (const Box2& b : colliders_)
        if (circleHitsBox(x, z, r, b.x0, b.z0, b.x1, b.z1)) return true;
    for (const Door& d : doors) {
        if (d.open > 0.8f) continue; // открытая дверь не мешает
        const float hw = d.alongX ? 0.5f : 0.08f;
        const float hd = d.alongX ? 0.08f : 0.5f;
        if (circleHitsBox(x, z, r, d.base.x - hw, d.base.z - hd, d.base.x + hw, d.base.z + hd)) return true;
    }
    return false;
}

void Level::moveWithCollision(float* x, float* z, float dx, float dz, float r) const {
    // Оси проверяются по отдельности: упёршись в стену под углом, игрок скользит вдоль неё.
    if (!blocked(*x + dx, *z, r)) *x += dx;
    if (!blocked(*x, *z + dz, r)) *z += dz;
}

bool Level::lineClear(const Vec3& from, const Vec3& to) const {
    const Vec3 d = to - from;
    const float len = length(d);
    const int steps = std::max(1, static_cast<int>(len / 0.05f));
    for (int i = 0; i <= steps; ++i) {
        const Vec3 p = from + d * (static_cast<float>(i) / steps);
        if (isSolid(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.z)))) return false;
        for (const Door& door : doors) {
            if (door.open > 0.8f) continue;
            const float hw = door.alongX ? 0.5f : 0.08f;
            const float hd = door.alongX ? 0.08f : 0.5f;
            if (p.x > door.base.x - hw && p.x < door.base.x + hw && p.z > door.base.z - hd &&
                p.z < door.base.z + hd && p.y < kDoorH)
                return false;
        }
    }
    return true;
}

InteractId Level::pick(const Vec3& eye, const Vec3& dir, float reach) const {
    float best = reach;
    InteractId found = UseNone;
    for (const Interactable& it : interactables) {
        const float t = rayBox(eye, dir, it.mn, it.mx);
        if (t < 0 || t > best) continue;
        best = t;
        found = it.id;
    }
    if (found == UseNone) return UseNone;
    // Чуть не доходя до предмета, чтобы стена за ним не считалась преградой.
    if (!lineClear(eye, eye + dir * std::max(0.0f, best - 0.12f))) return UseNone;
    return found;
}

void Level::updateDoors(float dt) {
    for (Door& d : doors) {
        if (d.open < d.target) d.open = std::min(d.target, d.open + d.speed * dt);
        if (d.open > d.target) d.open = std::max(d.target, d.open - d.speed * dt);
    }
    const ObjectId ids[kDoorCount] = {ObjDoorCell, ObjDoorGate, ObjDoorBad, ObjDoorExit};
    for (int i = 0; i < kDoorCount; ++i) {
        const Mat4 turn = doors[i].alongX ? Mat4::identity() : Mat4::rotateY(kPi * 0.5f);
        objects[ids[i]].model = Mat4::translate(doorPos(i)) * turn;
    }
}

void Level::placeRobot(const Vec3& pos, float yaw, float bob, float eyeGlow, float eyeOpen) {
    const Mat4 m = Mat4::translate({pos.x, bob, pos.z}) * Mat4::rotateY(-yaw) *
                   Mat4::scale({1.18f, 1.18f, 1.18f});
    objects[ObjRobotBody].model = m;
    objects[ObjRobotDark].model = m;
    const Vec3 eyeCenter{0, 1.525f, -0.19f};
    objects[ObjRobotEye].model = m * Mat4::translate(eyeCenter) *
                                 Mat4::scale({1.0f + 0.25f * (eyeOpen - 1.0f), eyeOpen, 1.0f}) *
                                 Mat4::translate(eyeCenter * -1.0f);
    objects[ObjRobotEye].emissive = eyeGlow;
    const Vec3 forward{std::sin(yaw), 0, -std::cos(yaw)};
    lights[LightRobot].pos = Vec3{pos.x, 1.8f, pos.z} + forward * 0.4f;
    lights[LightRobot].color = Vec3{0.9f, 0.10f, 0.06f} * eyeGlow;
}

void Level::placeHuman(const Vec3& pos, float yaw, bool visible) {
    const Mat4 m = Mat4::translate(pos) * Mat4::rotateY(-yaw);
    for (ObjectId id : {ObjHumanJacket, ObjHumanPants, ObjHumanSkin}) {
        objects[id].model = m;
        objects[id].visible = visible;
    }
}

void Level::draw(gfx::Renderer& r) const {
    const Mat4 identity;
    for (const StaticBatch& s : statics_) r.drawMesh(s.mesh, s.tex, identity);
    for (const Object& o : objects) {
        if (!o.visible || o.mesh.count == 0) continue;
        r.drawMesh(o.mesh, o.tex, o.model, o.tint, o.emissive, o.lit);
    }
}

} // namespace game
