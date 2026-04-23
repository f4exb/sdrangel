#ifndef GEOSCAN_GFSK_H
#define GEOSCAN_GFSK_H
#include <cmath>
#include <cstdint>

class GFSK {
public:
    GFSK(float sampleRate, float symbolRate);
    ~GFSK();

    uint8_t processSample(float i, float q);

private:
    float m_prevI;
    float m_prevQ;
    float m_sensitivity;
};

#endif // GEOSCAN_GFSK_H