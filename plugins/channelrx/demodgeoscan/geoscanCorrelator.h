#ifndef GEOSCAN_CORRELATOR_H
#define GEOSCAN_CORRELATOR_H
#include <cstdint>
#include <vector>
#include <functional>
#include <memory>

constexpr int SYNC_WORD_LEN = 32;
constexpr uint8_t SYNC_WORD[SYNC_WORD_LEN] = {
    1,0,0,1, 0,0,1,1, 0,0,0,0, 1,0,1,1,
    0,1,0,1, 0,0,0,1, 1,1,0,1, 1,1,1,0
};

constexpr int PACKET_DATA_SIZE = 72;
constexpr int CRC_SIZE = 2;
constexpr int PACKET_SIZE = PACKET_DATA_SIZE + CRC_SIZE;

enum class State { SEARCHING, COLLECTING };

typedef std::function<void(const std::vector<uint8_t>&)> CallbackFunc;

class Correlator {
public:
    Correlator(int sampPerBit, int threshold, CallbackFunc callback);
    ~Correlator();

    Correlator(const Correlator&) = delete;
    Correlator& operator=(const Correlator&) = delete;
    Correlator(Correlator&&) = default;
    Correlator& operator=(Correlator&&) = default;

    void process(uint8_t soft);

private:
    // Returns average soft value over circular buffer range [from, to]
    int contribution(const uint8_t* buffer, int from, int to) const;

    // --- config ---
    int m_sampPerBit;
    int m_threshold;
    CallbackFunc m_callback;

    // --- search state (circular buffer) ---
    State m_state;
    int m_bufferLen;
    std::unique_ptr<uint8_t[]> m_buffer; 
    int m_head = 0;
    bool m_isInverted = false;

    // --- collect state ---
    uint8_t m_packet[PACKET_SIZE];
    int m_packetIndex = 0;
    uint8_t m_currentByte = 0;
    int m_bitCount = 0;
    int m_samplesCount = 0;
    int m_softSum = 0;
};

#endif // GEOSCAN_CORRELATOR_H
