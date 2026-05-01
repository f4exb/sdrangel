#ifndef GEOSCAN_DCBLOCKER_H
#define GEOSCAN_DCBLOCKER_H
#include <cmath>
#include <utility>

class DCBlocker {
public:
    DCBlocker(float dev, float sampleRate)
        : m_avgI(0.0f)
        , m_avgQ(0.0f)
    {
        m_alpha = 1.0f - (2.0f * M_PI * dev / sampleRate);
    }

    void setDev(float dev, float sampleRate) {
        m_alpha = 1.0f - (2.0f * M_PI * dev / sampleRate);
        m_avgI = 0.0f;
        m_avgQ = 0.0f;
    }

    std::pair<float, float> process(float i, float q) {
        m_avgI = m_alpha * m_avgI + (1.0f - m_alpha) * i;
        m_avgQ = m_alpha * m_avgQ + (1.0f - m_alpha) * q;
        return { i - m_avgI, q - m_avgQ };
    }

private:
    float m_alpha;
    float m_avgI;
    float m_avgQ;
};

#endif // GEOSCAN_DCBLOCKER_H