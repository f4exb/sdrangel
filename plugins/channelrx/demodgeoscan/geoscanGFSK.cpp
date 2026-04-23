#include "geoscanGFSK.h"

GFSK::GFSK(float sampleRate, float symbolRate)
{
    m_sensitivity = M_PI * symbolRate / sampleRate;
    m_prevI = 0.0f;
    m_prevQ = 0.0f;
}

GFSK::~GFSK()
{
}

uint8_t GFSK::processSample(float i, float q)
{
}
