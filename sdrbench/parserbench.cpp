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

#include <QCommandLineOption>
#include <QRegExpValidator>
#include <QDebug>

#include "parserbench.h"

ParserBench::ParserBench() :
    m_testOption(QStringList() << "t" << "test",
        "Test type.",
        "test",
        "decimateii"),
    m_nbSamplesOption(QStringList() << "n" << "nb-samples",
        "Number of sample to deal with.",
        "samples",
        "1048576"),
    m_repetitionOption(QStringList() << "r" << "repeat",
        "Number of repetitions.",
        "repetition",
        "1"),
    m_log2FactorOption(QStringList() << "l" << "log2-factor",
        "Log2 factor for rate conversion.",
        "log2",
        "2")
{
    m_testStr = "decimateii";
    m_nbSamples = 1048576;
    m_repetition = 1;
    m_log2Factor = 4;

    m_parser.setApplicationDescription("Software Defined Radio application benchmarks");
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(m_testOption);
    m_parser.addOption(m_nbSamplesOption);
    m_parser.addOption(m_repetitionOption);
    m_parser.addOption(m_log2FactorOption);
}

ParserBench::~ParserBench()
{ }

void ParserBench::parse(const QCoreApplication& app)
{
    m_parser.process(app);

    int pos;
    bool ok;

    // test switch

    QString test = m_parser.value(m_testOption);

    QString testStr = "([a-z]+)";
    QRegExp ipRegex ("^" + testStr + "$");
    QRegExpValidator ipValidator(ipRegex);

    if (ipValidator.validate(test, pos) == QValidator::Acceptable) {
        m_testStr = test;
    } else {
        qWarning() << "ParserBench::parse: test string invalid. Defaulting to " << m_testStr;
    }

    // number of samples

    QString nbSamplesStr = m_parser.value(m_nbSamplesOption);
    int nbSamples = nbSamplesStr.toInt(&ok);

    if (ok && (nbSamples > 1024) && (nbSamples < 1073741824)) {
        m_nbSamples = nbSamples;
    } else {
        qWarning() << "ParserBench::parse: number of samples invalid. Defaulting to " << m_nbSamples;
    }

    // repetition

    QString repetitionStr = m_parser.value(m_repetitionOption);
    int repetition = repetitionStr.toInt(&ok);

    if (ok && (repetition >= 0)) {
        m_repetition = repetition;
    } else {
        qWarning() << "ParserBench::parse: repetition invalid. Defaulting to " << m_repetition;
    }

    // log2 factor

    QString log2FactorStr = m_parser.value(m_log2FactorOption);
    int log2Factor = log2FactorStr.toInt(&ok);

    if (ok && (log2Factor >= 0) && (log2Factor <= 6)) {
        m_log2Factor = log2Factor;
    } else {
        qWarning() << "ParserBench::parse: repetilog2 factortion invalid. Defaulting to " << m_log2Factor;
    }
}

ParserBench::TestType ParserBench::getTestType() const
{
    if (m_testStr == "decimatefi") {
        return TestDecimatorsFI;
    } else if (m_testStr == "decimateff") {
        return TestDecimatorsFF;
    } else if (m_testStr == "decimateif") {
        return TestDecimatorsIF;
    } else if (m_testStr == "decimateinfii") {
        return TestDecimatorsInfII;
    } else if (m_testStr == "decimatesupii") {
        return TestDecimatorsSupII;
    } else {
        return TestDecimatorsII;
    }
}
