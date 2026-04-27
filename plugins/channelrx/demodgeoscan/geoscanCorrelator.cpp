#include "geoscanCorrelator.h"

Correlator::Correlator(int sampPerBit, int threshold, CallbackFunc callback)
    : m_sampPerBit(sampPerBit)
    , m_threshold(threshold)
    , m_callback(callback)
    , m_state(State::SEARCHING)
    , m_bufferLen(SYNC_WORD_LEN * sampPerBit)
    , m_buffer(new uint8_t[SYNC_WORD_LEN * sampPerBit]{})
{}

Correlator::~Correlator() {
    delete[] m_buffer;
}

// Returns average soft value over circular buffer range [from, to]
int Correlator::contribution(const uint8_t* buffer, int from, int to) const
{
    float sum = 0.0f;
    int count = 0;

    if (from <= to) {
        for (int i = from; i <= to; i++) {
            sum += buffer[i];
            count++;
        }
    } else {
        // range wraps around end of circular buffer
        for (int i = from; i < m_bufferLen; i++) { sum += buffer[i]; count++; }
        for (int i = 0;    i <= to;         i++) { sum += buffer[i]; count++; }
    }

    return (int)(sum / count);
}

void Correlator::process(uint8_t soft)
{
    if (m_state == State::SEARCHING) {
        // push new sample into circular buffer
        m_buffer[m_head] = soft;
        m_head = (m_head + 1) % m_bufferLen;

        // correlate buffer against each bit of SYNC_WORD
        int sumCont = 0;
        for (int i = 0; i < SYNC_WORD_LEN; i++) {
            int symbolStart = (m_head + m_sampPerBit * i) % m_bufferLen;
            int symbolEnd   = (m_head + m_sampPerBit * (i + 1) - 1) % m_bufferLen;
            int avg = contribution(m_buffer, symbolStart, symbolEnd);

            // high avg → '1', invert for '0'
            sumCont += (SYNC_WORD[i] == 1) ? avg : (255 - avg);
        }

        if (sumCont > m_threshold)
            m_state = State::COLLECTING;
    }
    else if (m_state == State::COLLECTING) {
        // accumulate samples for current bit
        m_softSum += soft;
        m_samplesCount++;

        if (m_samplesCount == m_sampPerBit) {
            // decide bit: avg above midpoint = 1, below = 0
            uint8_t bit = ((float)m_softSum / m_sampPerBit > 127.5f) ? 1 : 0;
            m_samplesCount = 0;
            m_softSum = 0;

            // shift bit into current byte (MSB first)
            m_currentByte = (m_currentByte << 1) | bit;
            m_bitCount++;

            if (m_bitCount == 8) {
                m_packet[m_packetIndex++] = m_currentByte;
                m_bitCount = 0;
                m_currentByte = 0;
            }

            if (m_packetIndex == PACKET_SIZE) {
                m_callback(std::vector<uint8_t>(m_packet, m_packet + PACKET_SIZE));
                m_packetIndex = 0;
                m_state = State::SEARCHING;
            }
        }
    }
}
