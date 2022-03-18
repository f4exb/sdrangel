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

#include <QPainter>
#include <QResizeEvent>

#include "featurewindow.h"
#include "rollupwidget.h"

FeatureWindow::FeatureWindow(QWidget* parent) :
    QScrollArea(parent)
{
    m_container = new QWidget(this);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_splitter = new QSplitter();
    m_layout = new FeatureLayout(m_container, 3, 3, 3);
    setWidget(m_container);
    setWidgetResizable(true);
    setBackgroundRole(QPalette::Base);
    m_layout->addWidget(m_splitter); // Splitter must be added first
}

void FeatureWindow::addRollupWidget(QWidget* rollupWidget)
{
    if (rollupWidget->sizePolicy().verticalPolicy() == QSizePolicy::Expanding)
    {
        rollupWidget->setParent(m_splitter);
        m_splitter->addWidget(rollupWidget);
    }
    else
    {
        rollupWidget->setParent(m_container);
        m_layout->addWidget(rollupWidget);
    }
}

void FeatureWindow::resizeEvent(QResizeEvent* event)
{
    if (event->size().height() > event->size().width())
    {
        m_layout->setOrientation(Qt::Vertical);
        m_splitter->setOrientation(Qt::Vertical);
    }
    else
    {
        m_layout->setOrientation(Qt::Horizontal);
        m_splitter->setOrientation(Qt::Horizontal);
    }
    QScrollArea::resizeEvent(event);
}
