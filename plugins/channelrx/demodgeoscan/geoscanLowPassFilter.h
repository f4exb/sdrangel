#ifndef GEOSCAN_LOWPASS_FILTER_H
#define GEOSCAN_LOWPASS_FILTER_H
#include <cmath>
#include <cstdint>

class LowPassFilter {
public:
    LowPassFilter(float bw, float sampleRate) : m_alpha(1.0f - (2.0f * M_PI * bw / sampleRate)), m_avgSoft(0.0f) {}
    
    uint8_t process(uint8_t soft) {
        m_avgSoft = m_alpha * m_avgSoft + (1 - m_alpha) * soft;
        return static_cast<uint8_t>(std::roundf(m_avgSoft));
    }

    void setBw(float bw, float sampleRate) {
        m_alpha = 1.0f - (2.0f * M_PI * bw / sampleRate);
        m_avgSoft = 0.0f;
    }

private:
    float m_alpha;
    float m_avgSoft;
};

#endif // GEOSCAN_LOWPASS_FILTER_H