///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDial>
#include <QSlider>
#include <QLabel>
#include <QDebug>

#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "dialpopup.h"

DialPopup::DialPopup(QDial *parent) :
    QDialog(parent),
    m_dial(parent),
    m_originalValue(m_dial->value())
{
    m_value = new QSlider(Qt::Horizontal);
    m_valueText = new QLabel(QString::number(m_dial->value()));
    m_label = new QLabel(m_dial->toolTip());
    QVBoxLayout *v = new QVBoxLayout(this);
    QHBoxLayout *h = new QHBoxLayout();
    h->addWidget(m_label);
    h->addWidget(m_value);
    h->addWidget(m_valueText);
    v->addLayout(h);
    h = new QHBoxLayout();
    m_cancelButton = new QPushButton("Cancel");
    m_okButton = new QPushButton("OK");
    h->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
    h->addWidget(m_cancelButton);
    h->addWidget(m_okButton);
    v->addLayout(h);

    connect(m_value, &QSlider::valueChanged, this, &DialPopup::on_value_valueChanged);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);

    // Add right click / tap and hold on parent to open dialog
    CRightClickEnabler *rclick = new CRightClickEnabler(m_dial);
    connect(rclick, &CRightClickEnabler::rightClick, this, &DialPopup::display);

    m_positioner = new DialogPositioner(this);
}

void DialPopup::reject()
{
    m_dial->setValue(m_originalValue);
    QDialog::reject();
}

void DialPopup::accept()
{
    m_dial->setValue(m_value->value());
    QDialog::accept();
}

void DialPopup::display(const QPoint& p)
{
    if (m_dial->isEnabled())
    {
        m_value->setMaximum(m_dial->maximum());
        m_value->setMinimum(m_dial->minimum());
        m_value->setPageStep(m_dial->pageStep());
        m_value->setSingleStep(m_dial->singleStep());
        m_value->setValue(m_dial->value());
        m_originalValue = m_dial->value();
        move(p);
        show();
    }
}

void DialPopup::on_value_valueChanged(int value)
{
    m_valueText->setText(QString::number(value));
    m_dial->setValue(value);
}

void DialPopup::addPopupsToChildDials(QWidget *parent)
{
    // Get list of child dials that already have a popup
    QList<DialPopup *> popups = parent->findChildren<DialPopup *>();
    QList<QDial *> dialsWithPopups;
    for (auto popup : popups) {
        dialsWithPopups.append(popup->dial());
    }

    // Add dial popups to all child dials without one
    QList<QDial *> dials = parent->findChildren<QDial *>();
    for (auto dial : dials)
    {
        if (!dialsWithPopups.contains(dial)) {
            new DialPopup(dial);
        }
    }
}
