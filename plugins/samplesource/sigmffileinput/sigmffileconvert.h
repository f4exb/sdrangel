///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SIGMFFILECONVERT_H
#define INCLUDE_SIGMFFILECONVERT_H

#include "dsp/dsptypes.h"

// Convert from Little Endian

template<typename T>
T sigMFFromLE(const T in) {
    return in; // default assumes LE -> LE and is unused anyway
}

template<>
float sigMFFromLE<float>(const float in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    return in;
#else
    float retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#endif
}

template<>
int16_t sigMFFromLE<int16_t>(const int16_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    return in;
#else
    int16_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[1];
    converted[1] = toConvert[0];

    return retVal;
#endif
}

template<>
uint16_t sigMFFromLE<uint16_t>(const uint16_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    return in;
#else
    uint16_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[1];
    converted[1] = toConvert[0];

    return retVal;
#endif
}

template<>
int32_t sigMFFromLE<int32_t>(const int32_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    return in;
#else
    int32_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#endif
}

template<>
uint32_t sigMFFromLE<uint32_t>(const uint32_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    return in;
#else
    uint32_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#endif
}

// Convert from Big Endian

template<typename T>
T sigMFFromBE(const T in) {
    return in; // default assumes BE -> BE and is unused anyway
}

template<>
float sigMFFromBE<float>(const float in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    float retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#else
    return in;
#endif
}

template<>
int16_t sigMFFromBE<int16_t>(const int16_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    int16_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[1];
    converted[1] = toConvert[0];

    return retVal;
#else
    return in;
#endif
}

template<>
uint16_t sigMFFromBE<uint16_t>(const uint16_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    uint16_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[1];
    converted[1] = toConvert[0];

    return retVal;
#else
    return in;
#endif
}

template<>
int32_t sigMFFromBE<int32_t>(const int32_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    int32_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#else
    return in;
#endif
}

template<>
uint32_t sigMFFromBE<uint32_t>(const uint32_t in)
{
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    uint32_t retVal;
    char *toConvert = ( char* ) & in;
    char *converted = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    converted[0] = toConvert[3];
    converted[1] = toConvert[2];
    converted[2] = toConvert[1];
    converted[3] = toConvert[0];

    return retVal;
#else
    return in;
#endif
}

// Sample conversions

class SigMFConverterInterface
{
public:
    virtual int convert(FixReal *convertBuffer, const quint8* buf, int nbBytes) = 0;
};

template<typename SigMFT, unsigned int SdrBits, unsigned int InputBits, bool IsComplex, bool IsBigEndian, bool SwapIQ>
class SigMFConverter : public SigMFConverterInterface
{
public:
    virtual int convert(FixReal *convertBuffer, const quint8* buf, int nbBytes);
};

template<typename SigMFT, unsigned int SdrBits, unsigned int InputBits, bool IsComplex, bool IsBigEndian, bool SwapIQ>
int SigMFConverter<SigMFT, SdrBits, InputBits, IsComplex, IsBigEndian, SwapIQ>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const SigMFT *sigMFBuf = (SigMFT *) buf;
    int nbSamples = nbBytes / ((IsComplex ? 2 : 1) * sizeof(SigMFT));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFBuf[2*is];
        convertBuffer[2*is+1] = sigMFBuf[2*is+1];
    }

    return nbSamples;
}

// Specialized templates

// =================
// float input type
// =================

// float complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is]) * 32768.0f;
        convertBuffer[2*is+1] = sigMFFromLE<float>(sigMFBuf[2*is+1]) * 32768.0f;
    }

    return nbSamples;
}

// float complex LE IQ => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is]) * 8388608.0f;
        convertBuffer[2*is+1] = sigMFFromLE<float>(sigMFBuf[2*is+1]) * 8388608.0f;
    }

    return nbSamples;
}

// float complex LE QI => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is+1]) * 32768.0f;
        convertBuffer[2*is+1] = sigMFFromLE<float>(sigMFBuf[2*is]) * 32768.0f;
    }

    return nbSamples;
}

// float complex LE QI => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is+1]) * 8388608.0f;
        convertBuffer[2*is+1] = sigMFFromLE<float>(sigMFBuf[2*is]) * 8388608.0f;
    }

    return nbSamples;
}

// float real LE => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / sizeof(float);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is]) * 32768.0f;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// float real LE => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / sizeof(float);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<float>(sigMFBuf[2*is]) * 8388608.0f;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// float complex BE IQ => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is]) * 32768.0f;
        convertBuffer[2*is+1] = sigMFFromBE<float>(sigMFBuf[2*is+1]) * 32768.0f;
    }

    return nbSamples;
}

// float complex BE IQ => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is]) * 8388608.0f;
        convertBuffer[2*is+1] = sigMFFromBE<float>(sigMFBuf[2*is+1]) * 8388608.0f;
    }

    return nbSamples;
}

// float complex BE QI => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is+1]) * 32768.0f;
        convertBuffer[2*is+1] = sigMFFromBE<float>(sigMFBuf[2*is]) * 32768.0f;
    }

    return nbSamples;
}

// float complex BE QI => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / (2*sizeof(float));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is+1]) * 8388608.0f;
        convertBuffer[2*is+1] = sigMFFromBE<float>(sigMFBuf[2*is]) * 8388608.0f;
    }

    return nbSamples;
}

// float real BE => FixReal 16 bits

template<>
int SigMFConverter<float, 16, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / sizeof(float);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is]) * 32768.0f;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// float real BE => FixReal 24 bits

template<>
int SigMFConverter<float, 24, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const float *sigMFBuf = (float *) buf;
    int nbSamples = nbBytes / sizeof(float);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<float>(sigMFBuf[2*is]) * 8388608.0f;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// ================================
// 8 bit signed integer input type
// ================================

// s8 complex IQ => FixReal 16 bits

template<>
int SigMFConverter<int8_t, 16, 8, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFBuf[2*is] << 8;
        convertBuffer[2*is+1] = sigMFBuf[2*is+1] << 8;
    }

    return nbSamples;
}

// s8 complex IQ => FixReal 24 bits

template<>
int SigMFConverter<int8_t, 24, 8, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFBuf[2*is] << 16;
        convertBuffer[2*is+1] = sigMFBuf[2*is+1] << 16;
    }

    return nbSamples;
}

// s8 complex QI => FixReal 16 bits

template<>
int SigMFConverter<int8_t, 16, 8, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFBuf[2*is+1] << 8;
        convertBuffer[2*is+1] = sigMFBuf[2*is] << 8;
    }

    return nbSamples;
}

// s8 complex QI => FixReal 24 bits

template<>
int SigMFConverter<int8_t, 24, 8, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFBuf[2*is+1] << 16;
        convertBuffer[2*is+1] = sigMFBuf[2*is] << 16;
    }

    return nbSamples;
}

// s8 real => FixReal 16 bits

template<>
int SigMFConverter<int8_t, 16, 8, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / sizeof(int8_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is] = sigMFBuf[2*is] << 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// s8 real => FixReal 24 bits

template<>
int SigMFConverter<int8_t, 24, 8, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int8_t *sigMFBuf = (int8_t *) buf;
    int nbSamples = nbBytes / sizeof(int8_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is] = sigMFBuf[2*is] << 16;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// ==================================
// 8 bit unsigned integer input type
// ==================================

// u8 complex IQ => FixReal 16 bits

template<>
int SigMFConverter<uint8_t, 16, 8, true, false, false>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / (2*sizeof(uint8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFBuf[2*is] - 128) << 8;
        convertBuffer[2*is+1] = (sigMFBuf[2*is+1] - 128) << 8;
    }

    return nbSamples;
}

// u8 complex IQ => FixReal 24 bits

template<>
int SigMFConverter<uint8_t, 24, 8, true, false, false>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / (2*sizeof(uint8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFBuf[2*is] - 128) << 16;
        convertBuffer[2*is+1] = (sigMFBuf[2*is+1] - 128) << 16;
    }

    return nbSamples;
}

// u8 complex QI => FixReal 16 bits

template<>
int SigMFConverter<uint8_t, 16, 8, true, false, true>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / (2*sizeof(uint8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFBuf[2*is+1] - 128) << 8;
        convertBuffer[2*is+1] = (sigMFBuf[2*is] - 128) << 8;
    }

    return nbSamples;
}

// u8 complex QI => FixReal 24 bits

template<>
int SigMFConverter<uint8_t, 24, 8, true, false, true>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / (2*sizeof(uint8_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFBuf[2*is+1] - 128) << 16;
        convertBuffer[2*is+1] = (sigMFBuf[2*is] - 128) << 16;
    }

    return nbSamples;
}

// u8 real => FixReal 16 bits

template<>
int SigMFConverter<uint8_t, 16, 8, false, false, false>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / sizeof(uint8_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is] = (sigMFBuf[2*is] - 128) << 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u8 real => FixReal 24 bits

template<>
int SigMFConverter<uint8_t, 24, 8, false, false, false>::convert(FixReal *convertBuffer, const quint8* sigMFBuf, int nbBytes)
{
    int nbSamples = nbBytes / sizeof(uint8_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is] = (sigMFBuf[2*is] - 128) << 16;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// =================================
// 16 bit signed integer input type
// =================================

// i16 complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    std::copy(sigMFBuf, sigMFBuf + 2*nbSamples, convertBuffer);
    return nbSamples;
#else
    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is]);
        convertBuffer[2*is+1] = sigMFFromLE<int16_t>(sigMFBuf[2*is+1]);
    }

    return nbSamples;
#endif
}

// i16 complex LE IQ => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is]) << 8;
        convertBuffer[2*is+1] = sigMFFromLE<int16_t>(sigMFBuf[2*is+1]) << 8;
    }

    return nbSamples;
}

// i16 complex LE QI => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is+1]);
        convertBuffer[2*is+1] = sigMFFromLE<int16_t>(sigMFBuf[2*is]);
    }

    return nbSamples;
}

// i16 complex LE QI => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is+1] << 8);
        convertBuffer[2*is+1] = sigMFFromLE<int16_t>(sigMFBuf[2*is] << 8);
    }

    return nbSamples;
}

// i16 real LE => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / sizeof(int16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is]);
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// i16 real LE => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / sizeof(int16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int16_t>(sigMFBuf[2*is] << 8);
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// i16 complex BE IQ => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is]);
        convertBuffer[2*is+1] = sigMFFromBE<int16_t>(sigMFBuf[2*is+1]);
    }

    return nbSamples;
}

// i16 complex BE IQ => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is]) << 8;
        convertBuffer[2*is+1] = sigMFFromBE<int16_t>(sigMFBuf[2*is+1]) << 8;
    }

    return nbSamples;
}

// i16 complex BE QI => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is+1]);
        convertBuffer[2*is+1] = sigMFFromBE<int16_t>(sigMFBuf[2*is]);
    }

    return nbSamples;
}

// i16 complex BE QI => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is+1]) << 8;
        convertBuffer[2*is+1] = sigMFFromBE<int16_t>(sigMFBuf[2*is]) << 8;
    }

    return nbSamples;
}

// i16 real BE => FixReal 16 bits

template<>
int SigMFConverter<int16_t, 16, 16, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / sizeof(int16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is]);
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// i16 real BE => FixReal 24 bits

template<>
int SigMFConverter<int16_t, 24, 16, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int16_t *sigMFBuf = (int16_t *) buf;
    int nbSamples = nbBytes / sizeof(int16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int16_t>(sigMFBuf[2*is]) << 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// ===================================
// 16 bit unsigned integer input type
// ===================================

// u16 complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<uint16_t>(sigMFBuf[2*is]) - 32768;
        convertBuffer[2*is+1] = sigMFFromLE<uint16_t>(sigMFBuf[2*is+1]) -32768;
    }

    return nbSamples;
}

// u16 complex LE IQ => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint16_t>(sigMFBuf[2*is]) - 32768) << 8;
        convertBuffer[2*is+1] = (sigMFFromLE<uint16_t>(sigMFBuf[2*is+1]) - 32768) << 8;
        convertBuffer[2*is+1] <<= 8;
    }

    return nbSamples;
}

// u16 complex LE QI => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<uint16_t>(sigMFBuf[2*is+1]) - 32768;
        convertBuffer[2*is+1] = sigMFFromLE<uint16_t>(sigMFBuf[2*is]) -32768;
    }

    return nbSamples;
}

// u16 complex LE QI => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint16_t>(sigMFBuf[2*is+1]) - 32768) << 8;
        convertBuffer[2*is+1] = (sigMFFromLE<uint16_t>(sigMFBuf[2*is]) - 32768) << 8;
    }

    return nbSamples;
}

// u16 real LE => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / sizeof(uint16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<uint16_t>(sigMFBuf[2*is]) - 32768;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u16 real LE  => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / sizeof(uint16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint16_t>(sigMFBuf[2*is]) - 32768) << 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u16 complex BE IQ => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<uint16_t>(sigMFBuf[2*is]) - 32768;
        convertBuffer[2*is+1] = sigMFFromBE<uint16_t>(sigMFBuf[2*is+1]) -32768;
    }

    return nbSamples;
}

// u16 complex BE IQ => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint16_t>(sigMFBuf[2*is]) - 32768) << 8;
        convertBuffer[2*is+1] = (sigMFFromBE<uint16_t>(sigMFBuf[2*is+1]) -32768) << 8;
    }

    return nbSamples;
}

// u16 complex BE QI => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<uint16_t>(sigMFBuf[2*is+1]) - 32768;
        convertBuffer[2*is+1] = sigMFFromBE<uint16_t>(sigMFBuf[2*is]) -32768;
    }

    return nbSamples;
}

// u16 complex BE QI => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint16_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint16_t>(sigMFBuf[2*is+1]) - 32768) << 8;
        convertBuffer[2*is+1] = (sigMFFromBE<uint16_t>(sigMFBuf[2*is]) -32768) << 8;
    }

    return nbSamples;
}

// u16 real BE => FixReal 16 bits

template<>
int SigMFConverter<uint16_t, 16, 16, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / sizeof(uint16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<uint16_t>(sigMFBuf[2*is]) - 32768;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u16 real BE  => FixReal 24 bits

template<>
int SigMFConverter<uint16_t, 24, 16, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint16_t *sigMFBuf = (uint16_t *) buf;
    int nbSamples = nbBytes / sizeof(uint16_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint16_t>(sigMFBuf[2*is]) - 32768) << 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// ======================================================
// 24 bit signed integer input type - SDRangel exclusive
// ======================================================

// s24 complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 24, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 8;
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 8;
    }

    return nbSamples;
}

// s24 complex LE IQ => FixReal 24 bits - SDRangel only

template<>
int SigMFConverter<int32_t, 24, 24, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));
#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    std::copy(sigMFBuf, sigMFBuf + 2*nbSamples, convertBuffer);
    return nbSamples;
#else
    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]);
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]);
    }

    return nbSamples;
#endif
}

// =================================
// 32 bit signed integer input type
// =================================

// s32 complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 16;
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 16;
    }

    return nbSamples;
}

// s32 complex LE IQ => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 8;
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 8;
    }

    return nbSamples;
}

// s32 complex LE QI => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 16;
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 16;
    }

    return nbSamples;
}

// s32 complex LE QI => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 8;
        convertBuffer[2*is+1] = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 8;
    }

    return nbSamples;
}

// s32 real LE => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / sizeof(int32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 16;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// s32 real LE => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / sizeof(int32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// s32 complex BE IQ => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 16;
        convertBuffer[2*is+1] = sigMFFromBE<int32_t>(sigMFBuf[2*is+1]) >> 16;
    }

    return nbSamples;
}

// s32 complex BE IQ => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 8;
        convertBuffer[2*is+1] = sigMFFromBE<int32_t>(sigMFBuf[2*is+1]) >> 8;
    }

    return nbSamples;
}

// s32 complex BE QI => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is+1]) >> 16;
        convertBuffer[2*is+1] = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 16;
    }

    return nbSamples;
}

// s32 complex BE QI => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(int32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is+1]) >> 8;
        convertBuffer[2*is+1] = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 8;
    }

    return nbSamples;
}

// s32 real BE => FixReal 16 bits

template<>
int SigMFConverter<int32_t, 16, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / sizeof(int32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 16;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// s32 real BE => FixReal 24 bits

template<>
int SigMFConverter<int32_t, 24, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const int32_t *sigMFBuf = (int32_t *) buf;
    int nbSamples = nbBytes / sizeof(int32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = sigMFFromBE<int32_t>(sigMFBuf[2*is]) >> 8;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// ===================================
// 32 bit unsigned integer input type
// ===================================

// u32 complex LE IQ => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<int32_t>(sigMFBuf[2*is]) >> 16) - 32768;
        convertBuffer[2*is+1] = (sigMFFromLE<int32_t>(sigMFBuf[2*is+1]) >> 16) - 32768;
    }

    return nbSamples;
}

// u32 complex LE IQ => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, true, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
        convertBuffer[2*is+1] = (sigMFFromLE<uint32_t>(sigMFBuf[2*is+1]) >> 8) - 8388608;
    }

    return nbSamples;
}

// u32 complex LE QI => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint32_t>(sigMFBuf[2*is+1]) >> 16) - 32768;
        convertBuffer[2*is+1] = (sigMFFromLE<uint32_t>(sigMFBuf[2*is]) >> 16) - 32768;
    }

    return nbSamples;
}

// u32 complex LE QI => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, true, false, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint32_t>(sigMFBuf[2*is+1]) >> 8) - 8388608;
        convertBuffer[2*is+1] = (sigMFFromLE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
    }

    return nbSamples;
}

// u32 real LE => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / sizeof(uint32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint32_t>(sigMFBuf[2*is]) >> 16) - 32768;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u32 real LE => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, false, false, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / sizeof(uint32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromLE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u32 complex BE IQ => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 16) - 32768;
        convertBuffer[2*is+1] = (sigMFFromBE<uint32_t>(sigMFBuf[2*is+1]) >> 16) - 32768;
    }

    return nbSamples;
}

// u32 complex BE IQ => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, true, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
        convertBuffer[2*is+1] = (sigMFFromBE<uint32_t>(sigMFBuf[2*is+1]) >> 8) - 8388608;
    }

    return nbSamples;
}

// u32 complex BE QI => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is+1]) >> 16) - 32768;
        convertBuffer[2*is+1] = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 16) - 32768;
    }

    return nbSamples;
}

// u32 complex BE QI => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, true, true, true>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / (2*sizeof(uint32_t));

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is+1]) >> 8) - 8388608;
        convertBuffer[2*is+1] = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
    }

    return nbSamples;
}

// u32 real BE => FixReal 16 bits

template<>
int SigMFConverter<uint32_t, 16, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / sizeof(uint32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 16) - 32768;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

// u32 real BE => FixReal 24 bits

template<>
int SigMFConverter<uint32_t, 24, 32, false, true, false>::convert(FixReal *convertBuffer, const quint8* buf, int nbBytes)
{
    const uint32_t *sigMFBuf = (uint32_t *) buf;
    int nbSamples = nbBytes / sizeof(uint32_t);

    for (int is = 0; is < nbSamples; is++)
    {
        convertBuffer[2*is]   = (sigMFFromBE<uint32_t>(sigMFBuf[2*is]) >> 8) - 8388608;
        convertBuffer[2*is+1] = 0;
    }

    return nbSamples;
}

#endif // INCLUDE_SIGMFFILEDATA_H