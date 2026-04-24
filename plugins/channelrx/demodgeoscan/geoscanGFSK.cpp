#include "geoscanGFSK.h"

GFSK::GFSK(float sampleRate, float symbolRate)
{
    m_sensitivity = M_PI * symbolRate / sampleRate;
    m_prevAngle = 0.0f;
}

GFSK::~GFSK(){}

uint8_t GFSK::processSample(float i, float q)
{
    const float prevAngle = m_prevAngle;
    float angle = atan2(q, i);

    float diff = angle - prevAngle;
    if (diff > M_PI) diff -= 2.0f * M_PI;
    if (diff < -M_PI) diff += 2.0f * M_PI;

    float soft = ((diff / m_sensitivity) + 1.0f) * 127.5f;
    if (soft > 255.0f) soft = 255.0f;
    if (soft < 0.0f)   soft = 0.0f;


    m_prevAngle = angle;
    return (uint8_t)soft;
}


