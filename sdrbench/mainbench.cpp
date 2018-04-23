///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#include <QDebug>
#include "mainbench.h"

MainBench *MainBench::m_instance = 0;

MainBench::MainBench(qtwebapp::LoggerWithFile *logger, const ParserBench& parser, QObject *parent) :
    QObject(parent),
    m_logger(logger),
    m_parser(parser)
{
    qDebug() << "MainBench::MainBench: start";
    m_instance = this;
    qDebug() << "MainBench::MainBench: end";
}

void MainBench::run()
{
    qDebug() << "MainBench::run: work in progress";
    qDebug() << "MainBench::run: parameters:" 
        << " test: " << m_parser.getTest()
        << " nsamples: " << m_parser.getNbSamples()
        << " repet: " << m_parser.getRepetition()
        << " log2f: " << m_parser.getLog2Factor();
    emit finished();
}

MainBench::~MainBench()
{}