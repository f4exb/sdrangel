#ifndef DETECTION_H
#define DETECTION_H
#include <QVector>


struct DetectedWindowStruct
{
    int startIndex;
    int endIndex;
    DetectedWindowStruct(int s, int e) : startIndex(s), endIndex(e) {}
};
class detection
{
public:
    detection();
    ~detection();
    static QVector<DetectedWindowStruct> Detect( QVector<float> data, int Packet_Size,float Adaptive_Thershold_level,   //110-130 dB
                   bool effect_status,int Bandwidth_magnification /*  1-200   */);
};

// Function declarations
float getMinValueInRange(const QVector<float>& vec, int startIdx, int endIdx);




#endif // DETECTION_H
