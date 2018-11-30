///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBENCH_PARSERBENCH_H_
#define SDRBENCH_PARSERBENCH_H_

#include <QCommandLineParser>
#include <stdint.h>

class ParserBench
{
public:
    typedef enum
    {
        TestDecimatorsII,
        TestDecimatorsIF,
        TestDecimatorsFI,
        TestDecimatorsFF,
        TestDecimatorsInfII,
        TestDecimatorsSupII
    } TestType;

    ParserBench();
    ~ParserBench();

    void parse(const QCoreApplication& app);

    const QString& getTestStr() const { return m_testStr; }
    TestType getTestType() const;
    uint32_t getNbSamples() const { return m_nbSamples; }
    uint32_t getRepetition() const { return m_repetition; }
    uint32_t getLog2Factor() const { return m_log2Factor; }

private:
    QString  m_testStr;
    uint32_t m_nbSamples;
    uint32_t m_repetition;
    uint32_t m_log2Factor;

    QCommandLineParser m_parser;
    QCommandLineOption m_testOption;
    QCommandLineOption m_nbSamplesOption;
    QCommandLineOption m_repetitionOption;
    QCommandLineOption m_log2FactorOption;
};



#endif /* SDRBENCH_PARSERBENCH_H_ */
