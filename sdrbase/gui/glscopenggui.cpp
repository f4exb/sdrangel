///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#include "glscopenggui.h"
#include "glscopeng.h"
#include "ui_glscopenggui.h"
#include "util/simpleserializer.h"

GLScopeNGGUI::GLScopeNGGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::GLScopeNGGUI),
    m_messageQueue(0),
    m_glScope(0),
    m_scopeVis(0),
    m_sampleRate(0)
{
    ui->setupUi(this);
}

GLScopeNGGUI::~GLScopeNGGUI()
{
    delete ui;
}

void GLScopeNGGUI::setBuddies(MessageQueue* messageQueue, ScopeVisNG* scopeVis, GLScopeNG* glScope)
{
    m_messageQueue = messageQueue;
    m_scopeVis = scopeVis;
    m_glScope = glScope;
    connect(m_glScope, SIGNAL(sampleRateChanged(int)), this, SLOT(on_scope_sampleRateChanged(int)));
    applySettings();
}

void GLScopeNGGUI::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
}

void GLScopeNGGUI::on_scope_sampleRateChanged(int sampleRate)
{
    //m_sampleRate = m_glScope->getSampleRate();
    m_sampleRate = sampleRate;
    ui->sampleRateText->setText(tr("%1\nkS/s").arg(m_sampleRate / 1000.0f, 0, 'f', 2));
}

void GLScopeNGGUI::resetToDefaults()
{
}


QByteArray GLScopeNGGUI::serialize() const
{
    // TODO
    SimpleSerializer s(1);
    return s.final();
}

bool GLScopeNGGUI::deserialize(const QByteArray& data)
{
    // TODO
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1) {
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void GLScopeNGGUI::on_onlyX_toggled(bool checked)
{
    if (checked)
    {
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_glScope->setDisplayMode(GLScopeNG::DisplayX);
    }
}

void GLScopeNGGUI::on_onlyY_toggled(bool checked)
{
    if (checked)
    {
        ui->onlyX->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_glScope->setDisplayMode(GLScopeNG::DisplayY);
    }
}

void GLScopeNGGUI::on_horizontalXY_toggled(bool checked)
{
    if (checked)
    {
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_glScope->setDisplayMode(GLScopeNG::DisplayXYH);
    }
}

void GLScopeNGGUI::on_verticalXY_toggled(bool checked)
{
    ui->onlyX->setChecked(false);
    ui->onlyY->setChecked(false);
    ui->horizontalXY->setChecked(false);
    ui->polar->setChecked(false);
    m_glScope->setDisplayMode(GLScopeNG::DisplayXYV);
}

void GLScopeNGGUI::on_polar_toggled(bool checked)
{
    ui->onlyX->setChecked(false);
    ui->onlyY->setChecked(false);
    ui->horizontalXY->setChecked(false);
    ui->verticalXY->setChecked(false);
    m_glScope->setDisplayMode(GLScopeNG::DisplayPol);
}

void GLScopeNGGUI::on_traceIntensity_valueChanged(int value)
{

}

void GLScopeNGGUI::on_gridIntensity_valueChanged(int value)
{

}

void GLScopeNGGUI::on_time_valueChanged(int value)
{

}

void GLScopeNGGUI::on_timeOfs_valueChanged(int value)
{

}

void GLScopeNGGUI::on_traceLen_valueChanged(int value)
{

}


void GLScopeNGGUI::applySettings()
{
}

bool GLScopeNGGUI::handleMessage(Message* message)
{
    return false;
}
