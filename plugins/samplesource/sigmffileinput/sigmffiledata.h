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

#ifndef INCLUDE_SIGMFFILEDATA_H
#define INCLUDE_SIGMFFILEDATA_H

#include <QString>

struct SigMFFileDataType
{
    bool m_complex;
    bool m_floatingPoint;
    bool m_signed;
    bool m_bigEndian;
    bool m_swapIQ;
    int  m_sampleBits;

    SigMFFileDataType() :
        m_complex(true),
        m_floatingPoint(false),
        m_signed(true),
        m_bigEndian(false),
        m_swapIQ(false),
        m_sampleBits(32)
    {}

    SigMFFileDataType(const SigMFFileDataType& other) :
        m_complex(other.m_complex),
        m_floatingPoint(other.m_floatingPoint),
        m_signed(other.m_signed),
        m_bigEndian(other.m_bigEndian),
        m_swapIQ(other.m_swapIQ),
        m_sampleBits(other.m_sampleBits)
    {}

    SigMFFileDataType& operator=(const SigMFFileDataType& t)
    {
        // Check for self assignment
        if (this != &t)
        {
            m_complex = t.m_complex;
            m_floatingPoint = t.m_floatingPoint;
            m_signed = t.m_signed;
            m_bigEndian = t.m_bigEndian;
            m_swapIQ = t.m_swapIQ;
            m_sampleBits = t.m_sampleBits;
        }

        return *this;
    }
};

struct SigMFFileMetaInfo
{
    // core
    QString m_dataTypeStr;
    SigMFFileDataType m_dataType;
    uint64_t m_totalSamples;
    uint64_t m_totalTimeMs;
    double m_coreSampleRate;
    QString m_sigMFVersion;
    QString m_sha512;
    unsigned int m_offset;
    QString m_description;
    QString m_author;
    QString m_metaDOI;
    QString m_dataDOI;
    QString m_recorder;
    QString m_license;
    QString m_hw;
    // sdrangel
    QString m_sdrAngelVersion;
    QString m_qtVersion;
    int m_rxBits;
    QString m_arch;
    QString m_os;
    // lists
    unsigned int m_nbCaptures;
    unsigned int m_nbAnnotations;

    SigMFFileMetaInfo()
    {}

    SigMFFileMetaInfo(const SigMFFileMetaInfo& other) :
        m_dataTypeStr(other.m_dataTypeStr),
        m_dataType(other.m_dataType),
        m_totalSamples(other.m_totalSamples),
        m_totalTimeMs(other.m_totalTimeMs),
        m_coreSampleRate(other.m_coreSampleRate),
        m_sigMFVersion(other.m_sigMFVersion),
        m_sha512(other.m_sha512),
        m_offset(other.m_offset),
        m_description(other.m_description),
        m_author(other.m_author),
        m_metaDOI(other.m_metaDOI),
        m_dataDOI(other.m_dataDOI),
        m_recorder(other.m_recorder),
        m_license(other.m_license),
        m_hw(other.m_hw),
        m_sdrAngelVersion(other.m_sdrAngelVersion),
        m_qtVersion(other.m_qtVersion),
        m_rxBits(other.m_rxBits),
        m_arch(other.m_arch),
        m_os(other.m_os),
        m_nbCaptures(other.m_nbCaptures),
        m_nbAnnotations(other.m_nbAnnotations)
    {}

    SigMFFileMetaInfo& operator=(const SigMFFileMetaInfo& t)
    {
        // Check for self assignment
        if (this != &t)
        {
            m_dataTypeStr = t.m_dataTypeStr;
            m_dataType = t.m_dataType;
            m_totalSamples = t.m_totalSamples;
            m_totalTimeMs = t.m_totalTimeMs;
            m_coreSampleRate = t.m_coreSampleRate;
            m_sigMFVersion = t.m_sigMFVersion;
            m_sha512 = t.m_sha512;
            m_offset = t.m_offset;
            m_description = t.m_description;
            m_author = t.m_author;
            m_metaDOI = t.m_metaDOI;
            m_dataDOI = t.m_dataDOI;
            m_recorder = t.m_recorder;
            m_license = t.m_license;
            m_hw = t.m_hw;
            m_sdrAngelVersion = t.m_sdrAngelVersion;
            m_qtVersion = t.m_qtVersion;
            m_rxBits = t.m_rxBits;
            m_arch = t.m_arch;
            m_os = t.m_os;
            m_nbCaptures = t.m_nbCaptures;
            m_nbAnnotations = t.m_nbAnnotations;
        }

        return *this;
    }
};

struct SigMFFileCapture
{
    uint64_t m_tsms;            //!< Unix timestamp in milliseconds
    uint64_t m_centerFrequency; //!< Center frequency in Hz
    uint64_t m_sampleStart;     //!< Sample index at which capture start
    uint64_t m_length;          //!< Length of capture in samples
    uint64_t m_cumulativeTime;  //!< Time since beginning of record (millisecond timestamp) at start
    unsigned int m_sampleRate;  //!< sdrangel extension - sample rate for this capture

    SigMFFileCapture() :
        m_tsms(0),
        m_centerFrequency(0),
        m_sampleStart(0),
        m_length(0),
        m_cumulativeTime(0),
        m_sampleRate(1)
    {}

    SigMFFileCapture(const SigMFFileCapture& other) :
        m_tsms(other.m_tsms),
        m_centerFrequency(other.m_centerFrequency),
        m_sampleStart(other.m_sampleStart),
        m_length(other.m_length),
        m_cumulativeTime(other.m_cumulativeTime),
        m_sampleRate(other.m_sampleRate)
    {}

    SigMFFileCapture& operator=(const SigMFFileCapture& t)
    {
        // Check for self assignment
        if (this != &t)
        {
            m_tsms = t.m_tsms;
            m_centerFrequency = t.m_centerFrequency;
            m_sampleStart = t.m_sampleStart;
            m_length = t.m_length;
            m_cumulativeTime = t.m_cumulativeTime;
            m_sampleRate = t.m_sampleRate;
        }

        return *this;
    }
};

#endif // INCLUDE_SIGMFFILEDATA_H