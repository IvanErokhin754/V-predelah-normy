// Процедурные текстуры. Всё рисуется кодом при запуске — файлов с картинками нет.
// Плотность пикселей намеренно разная (раздел 3 ТЗ):
//   бетон, краска, пол      — 32 px на метр, крупные квадратные пиксели;
//   дверь, койка, конвоир   — 64–128 px на предмет;
//   табличка, панели, рамка — 256–512 px, их рассматривают вблизи.
#pragma once

#include <string>
#include <vector>

#include "font.h"

namespace texgen {

// --- грубые, тайлятся ---
Image concrete();     // 64x64 на 2 м
Image paint();        // 64x64, полоса краски на нижней части стен
Image floorTiles();   // 64x64 на 2 м
Image ceiling();      // 64x64 на 2 м

// --- предметы ---
Image cellDoor();     // 64x128
Image heavyDoor();    // 64x128, дверь комнаты плохой концовки
Image wingDoor();     // 64x128, дверь пустого крыла
Image steel();        // 32x32, решётки и рамы
Image blanket();      // 64x64
Image pillow();       // 32x32
Image robotBody();    // 128x128
Image robotDark();    // 64x64
Image robotEye();     // 64x16, глаз-щель: строки развёртки и зрачок-объектив
Image crate();        // 128x128
Image tableTop();     // 64x64
Image monitorCase();  // 64x64
Image keyboard();     // 128x48
Image speaker();      // 64x64
Image cloth(Color base, Color stripe); // 32x32, одежда человека
Image flat(Color c);  // 8x8, однотонная
// Свет из-под двери на полу: 32x16, верхняя строка — у самой двери.
// Яркость спадает с расстоянием, край рассыпается на пиксели (дизеринг).
Image lightSpill();

// --- детали, которые рассматривают вблизи ---
Image monitorBezel();                                 // 512x448, рамка монитора
Image numberPlate(int cell, int prisoner);            // 256x128
Image buttonLabel(const std::string& text, Color bg); // 256x96
Image controlPanel(const std::string& title);         // 256x256
Image invoice(int crateNo, int sealNo);               // 256x320
Image sealTag(int sealNo);                            // 128x64
Image crateLabel(int crateNo);                        // 128x64
Image powerPanel(int sector, int occupied, bool active); // 256x256, смена «рубильник»

} // namespace texgen
