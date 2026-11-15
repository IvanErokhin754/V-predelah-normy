// Детерминированный генератор случайных чисел (splitmix64).
// Свой, а не std::mt19937 + distribution: распределения из <random> дают
// разные числа на разных компиляторах, а нам нужны одинаковые задания
// при загрузке сохранения на любой платформе.
#pragma once

#include <cstdint>

namespace core {

struct Rng {
    uint64_t state;

    explicit Rng(uint64_t seed = 1) : state(seed) {}

    uint64_t next() {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Целое из [lo, hi] включительно.
    int range(int lo, int hi) {
        const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(next() % span);
    }

    // Вещественное из [0, 1).
    double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
};

} // namespace core
