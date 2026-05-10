// Компиляция и запуск:
//   g++ -std=c++17 -o test_gfsk test_geoscanGFSK.cpp geoscanGFSK.cpp -lm && ./test_gfsk

#include "geoscanGFSK.h"
#include <cmath>
#include <cstdio>
#include <cassert>
#include <cstdlib>

static const float SAMPLE_RATE  = 96000.0f;
static const float SYMBOL_RATE  =  9600.0f;

static const float SENSITIVITY  = M_PI * SYMBOL_RATE / SAMPLE_RATE;

static int failures = 0;

static void check(const char* name, int got, int expected, int tolerance = 1)
{
    if (abs(got - expected) <= tolerance) {
        printf("  PASS  %s: got %d (expected %d)\n", name, got, expected);
    } else {
        printf("  FAIL  %s: got %d (expected %d, tolerance %d)\n",
               name, got, expected, tolerance);
        failures++;
    }
}

// Тест 1: положительное отклонение частоты — бит «1», ожидаем 255.
//
// prevAngle = 0 (начальное состояние).
// Следующий сэмпл имеет фазу +sensitivity.
// diff = sensitivity → soft = (1 + 1) * 127.5 = 255.
static void test_positive_deviation()
{
    puts("test_positive_deviation");
    GFSK gfsk(SAMPLE_RATE, SYMBOL_RATE);

    float angle = SENSITIVITY;
    uint8_t out = gfsk.processSample(cosf(angle), sinf(angle));

    check("output == 255", out, 255);
}

// Тест 2: отрицательное отклонение частоты — бит «0», ожидаем 0.
//
// prevAngle = 0. Следующий сэмпл имеет фазу -sensitivity.
// diff = -sensitivity → soft = (-1 + 1) * 127.5 = 0.
static void test_negative_deviation()
{
    puts("test_negative_deviation");
    GFSK gfsk(SAMPLE_RATE, SYMBOL_RATE);

    float angle = -SENSITIVITY;
    uint8_t out = gfsk.processSample(cosf(angle), sinf(angle));

    check("output == 0", out, 0);
}

// Тест 3: нулевое отклонение — несущая без модуляции, ожидаем 127.
//
// prevAngle = 0. Следующий сэмпл тоже имеет фазу 0.
// diff = 0 → soft = (0 + 1) * 127.5 = 127.5 → uint8 = 127.
static void test_zero_deviation()
{
    puts("test_zero_deviation");
    GFSK gfsk(SAMPLE_RATE, SYMBOL_RATE);

    uint8_t out = gfsk.processSample(1.0f, 0.0f);

    check("output == 127", out, 127);
}

// Тест 4: корректность wrap-around на границе ±π.
//
// Если сигнал переходит через границу ±π (atan2 «перескакивает»),
// без коррекции diff получился бы ≈ -2π, что даёт 0 вместо ~135.
//
// prevAngle = π - 0.01  (чуть ниже π)
// nextAngle = -(π - 0.01) (чуть выше -π, т.е. тот же физический угол плюс ε)
// diff_raw  = -(π-0.01) - (π-0.01) = -2π + 0.02
// после коррекции: diff = +0.02
// soft = (0.02/sensitivity + 1) * 127.5 = (0.2/π + 1) * 127.5 ≈ 135
static void test_phase_wraparound()
{
    puts("test_phase_wraparound");
    GFSK gfsk(SAMPLE_RATE, SYMBOL_RATE);

    // Устанавливаем prevAngle = π - 0.01
    float prev = M_PI - 0.01f;
    gfsk.processSample(cosf(prev), sinf(prev));

    // Теперь подаём сэмпл с фазой -(π - 0.01)
    float next = -(M_PI - 0.01f);
    uint8_t out = gfsk.processSample(cosf(next), sinf(next));

    // Ожидаемое: diff = 0.02, soft ≈ 135
    float expected_soft = (0.02f / SENSITIVITY + 1.0f) * 127.5f;
    int expected = (int)expected_soft;

    check("wrap-around output", out, expected, 2);

    // Без коррекции был бы 0 — убеждаемся, что это не так
    if (out > 64)
        printf("  PASS  wrap-around не обнулился (out=%d)\n", out);
    else {
        printf("  FAIL  wrap-around дал 0 — коррекция не работает\n");
        failures++;
    }
}

int main()
{
    test_positive_deviation();
    test_negative_deviation();
    test_zero_deviation();
    test_phase_wraparound();

    if (failures == 0)
        puts("\nВсе тесты прошли успешно.");
    else
        printf("\nПровалено тестов: %d\n", failures);

    return failures ? 1 : 0;
}
