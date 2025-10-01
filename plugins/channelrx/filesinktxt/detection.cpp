#include "detection.h"
#include <algorithm>
#include <cmath>


// Function to find minimum value in a range of the vector
float getMinValueInRange(const QVector<float>& vec, int startIdx, int endIdx) {
    if (startIdx < 0 || endIdx > vec.size() || startIdx >= endIdx) {
        return -1e10f; // Error value
    }
    
    float minVal = vec[startIdx];
    for (int idx = startIdx + 1; idx < endIdx; ++idx) {
        if (vec[idx] < minVal) {
            minVal = vec[idx];
        }
    }
    return minVal;
}


QVector<DetectedWindowStruct> detection::Detect( QVector<float> data, int Packet_Size, float Adaptive_Thershold_level,   //110-130 dB
               bool effect_status, int Bandwidth_magnification /*  1-200   */)
{

    //QVector<float> data = QVector<float>(117*Packet_Size);
    QVector<float> normalizedData_dB = QVector<float>(data.length());



    // normalization
    float minValue = *std::min_element(data.begin(), data.end());
    float maxValue = *std::max_element(data.begin(), data.end());
    for(int i=0; i< data.length(); i++)
    {
        float normalizedData = (data[i] - minValue) / (maxValue - minValue);
        normalizedData = normalizedData*normalizedData;
        normalizedData_dB[i] = 20*log10(normalizedData);
    }

    //QVector<float> min_of_each_packets = QVector<float>(data.length());
    //QVector<float> thershold_of_each_packets = QVector<float>(data.length());
    QVector<int> findIndexes = QVector<int>();
    for(int i=0; i<data.length();i+=Packet_Size)
    {
        float min_of_each_packets = getMinValueInRange(normalizedData_dB, i, i+Packet_Size);
        //if(min_of_each_packets == -INFINITY)
        //    min_of_each_packets = min_of_each_packets[i-1];

        float thershold_of_each_packets = min_of_each_packets + Adaptive_Thershold_level;

        for(int j=0; j<Packet_Size; j++)
            if(normalizedData_dB[i+j] > thershold_of_each_packets)
                findIndexes.append(i+j);
    }

    QVector<DetectedWindowStruct> detectedWindos = QVector<DetectedWindowStruct>();
    int startIndex = -1;
    int lastIndex = -1;
    for(int i=0; i< findIndexes.length(); i++)
    {
        if(startIndex == -1)
        {
            startIndex = findIndexes[i];
            lastIndex = startIndex;
            continue;
        }

        if(findIndexes[i] - lastIndex <=100)
            lastIndex = findIndexes[i];
        else
        {
            int s = effect_status ? qMax(0,startIndex-Bandwidth_magnification) : startIndex;
            int e = effect_status ? qMin(data.length()-1,lastIndex+Bandwidth_magnification) : lastIndex;
            detectedWindos.append(DetectedWindowStruct(s,e));
            startIndex = findIndexes[i];
            lastIndex = startIndex;
        }
    }
    if(startIndex != -1)
    {
        int s = effect_status ? qMax(0,startIndex-Bandwidth_magnification) : startIndex;
        int e = effect_status ? qMin(data.length()-1,lastIndex+Bandwidth_magnification) : lastIndex;
        detectedWindos.append(DetectedWindowStruct(s,e));
    }


    return detectedWindos;
}