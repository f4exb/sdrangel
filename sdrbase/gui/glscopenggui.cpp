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

#include <QColorDialog>

#include "glscopenggui.h"
#include "glscopeng.h"
#include "ui_glscopenggui.h"
#include "util/simpleserializer.h"

const double GLScopeNGGUI::amps[11] = { 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002, 0.0001 };

GLScopeNGGUI::GLScopeNGGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::GLScopeNGGUI),
    m_messageQueue(0),
    m_glScope(0),
    m_scopeVis(0),
    m_sampleRate(0),
    m_traceLenMult(1),
    m_timeBase(1),
    m_timeOffset(0)
{
    qDebug("GLScopeNGGUI::GLScopeNGGUI");
    setEnabled(false);
    ui->setupUi(this);
    ui->trigDelayFine->setMaximum(ScopeVisNG::m_traceChunkSize / 10.0);
    ui->traceColor->setStyleSheet("QLabel { background-color : rgb(255,255,64); }");
    m_focusedTraceColor.setRgb(255,255,64);
    ui->trigColor->setStyleSheet("QLabel { background-color : rgb(0,255,0); }");
    m_focusedTriggerColor.setRgb(0,255,0);
    ui->traceText->setText("X"); // TODO: remove when more than 2 traces are supported
}

GLScopeNGGUI::~GLScopeNGGUI()
{
    delete ui;
}

void GLScopeNGGUI::setBuddies(MessageQueue* messageQueue, ScopeVisNG* scopeVis, GLScopeNG* glScope)
{
    qDebug("GLScopeNGGUI::setBuddies");

    m_messageQueue = messageQueue;
    m_scopeVis = scopeVis;
    m_glScope = glScope;

    // initialize display combo
    ui->onlyX->setChecked(true);
    ui->onlyY->setChecked(false);
    ui->horizontalXY->setChecked(false);
    ui->verticalXY->setChecked(false);
    ui->polar->setChecked(false);
    ui->onlyY->setEnabled(false);
    ui->horizontalXY->setEnabled(false);
    ui->verticalXY->setEnabled(false);
    ui->polar->setEnabled(false);
    m_glScope->setDisplayMode(GLScopeNG::DisplayX);

    // initialize trigger combo
    ui->trigPos->setChecked(true);
    ui->trigNeg->setChecked(false);
    ui->trigBoth->setChecked(false);
    ui->trigOneShot->setChecked(false);
    ui->trigOneShot->setEnabled(false);
    ui->freerun->setChecked(true);

    // Add a trigger
    ScopeVisNG::TriggerData triggerData;
    fillTriggerData(triggerData);
    m_scopeVis->addTrigger(triggerData);

    // Add a trace
    ScopeVisNG::TraceData traceData;
    fillTraceData(traceData);
    m_scopeVis->addTrace(traceData);

    setEnabled(true);
    connect(m_glScope, SIGNAL(sampleRateChanged(int)), this, SLOT(on_scope_sampleRateChanged(int)));

    ui->traceMode->clear();
    fillProjectionCombo(ui->traceMode);

    ui->trigMode->clear();
    fillProjectionCombo(ui->trigMode);

    m_scopeVis->configure(2*m_traceLenMult*ScopeVisNG::m_traceChunkSize,
    		m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked());

    m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
    		m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked());

    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setAmpScaleDisplay();
    setAmpOfsDisplay();
    setTraceDelayDisplay();
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
    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setTraceDelayDisplay();
}

void GLScopeNGGUI::resetToDefaults()
{
}


QByteArray GLScopeNGGUI::serialize() const
{
    SimpleSerializer s(1);

    // first row
    s.writeS32(1, (int) m_glScope->getDisplayMode());
    s.writeS32(2, ui->traceIntensity->value());
    s.writeS32(3, ui->gridIntensity->value());
    s.writeS32(4, ui->time->value());
    s.writeS32(5, ui->timeOfs->value());
    s.writeS32(6, ui->traceLen->value());

    // second row - by trace
    const std::vector<ScopeVisNG::TraceData>& tracesData = m_scopeVis->getTracesData();
    std::vector<ScopeVisNG::TraceData>::const_iterator traceDataIt = tracesData.begin();
    s.writeU32(10, (uint32_t) tracesData.size());
    int i = 0;

    for (; traceDataIt != tracesData.end(); ++traceDataIt, i++)
    {
        s.writeS32(20 + 16*i, (int) traceDataIt->m_projectionType);
        s.writeU32(21 + 16*i, traceDataIt->m_ampIndex);
        s.writeS32(22 + 16*i, traceDataIt->m_ofsCoarse);
        s.writeS32(23 + 16*i, traceDataIt->m_ofsFine);
        s.writeS32(24 + 16*i, traceDataIt->m_traceDelayCoarse);
        s.writeS32(25 + 16*i, traceDataIt->m_traceDelayFine);
        s.writeFloat(26 + 16*i, traceDataIt->m_traceColorR);
        s.writeFloat(27 + 16*i, traceDataIt->m_traceColorG);
        s.writeFloat(28 + 16*i, traceDataIt->m_traceColorB);
    }

    // third row - by trigger
    s.writeU32(200, (uint32_t) m_scopeVis->getNbTriggers());
    s.writeS32(201, ui->trigPre->value());

    for (int i = 0; i < m_scopeVis->getNbTriggers(); i++)
    {
        const ScopeVisNG::TriggerData& triggerData = m_scopeVis->getTriggerData(i);
        s.writeS32(210 + 16*i, (int) triggerData.m_projectionType);
        s.writeBool(211 + 16*i, triggerData.m_triggerPositiveEdge);
        s.writeBool(212 + 16*i, triggerData.m_triggerBothEdges);
        s.writeBool(213 + 16*i, triggerData.m_triggerLevelCoarse);
        s.writeBool(214 + 16*i, triggerData.m_triggerLevelFine);
        s.writeBool(215 + 16*i, triggerData.m_triggerDelayCoarse);
        s.writeBool(216 + 16*i, triggerData.m_triggerDelayFine);
    }

    return s.final();
}

bool GLScopeNGGUI::deserialize(const QByteArray& data)
{
    qDebug("GLScopeNGGUI::deserialize");
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        MainUIBlocker mainUIBlocker(ui);
        int intValue;

        d.readS32(1, &intValue, (int) GLScopeNG::DisplayX);
        m_glScope->setDisplayMode((GLScopeNG::DisplayMode) intValue);

        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);

        switch (m_glScope->getDisplayMode())
        {
        case GLScopeNG::DisplayY:
            ui->onlyY->setChecked(true);
            break;
        case GLScopeNG::DisplayXYH:
            ui->horizontalXY->setChecked(true);
            break;
        case GLScopeNG::DisplayXYV:
            ui->verticalXY->setChecked(true);
            break;
        case GLScopeNG::DisplayPol:
            ui->polar->setChecked(true);
            break;
        case GLScopeNG::DisplayX:
        default:
            ui->onlyX->setChecked(true);
            break;
        }

        d.readS32(2, &intValue, 50);
        ui->traceIntensity->setValue(intValue);
        d.readS32(3, &intValue, 10);
        ui->gridIntensity->setValue(intValue);
        d.readS32(4, &intValue, 1);
        ui->time->setValue(intValue);
        d.readS32(5, &intValue, 0);
        ui->timeOfs->setValue(intValue);
        d.readS32(6, &intValue, 1);
        ui->traceLen->setValue(intValue);

        return true;
    }
    else
    {
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
    if (checked)
    {
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_glScope->setDisplayMode(GLScopeNG::DisplayXYV);
    }
}

void GLScopeNGGUI::on_polar_toggled(bool checked)
{
    if (checked)
    {
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        m_glScope->setDisplayMode(GLScopeNG::DisplayPol);
    }
}

void GLScopeNGGUI::on_traceIntensity_valueChanged(int value)
{
    ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(value));
    m_glScope->setDisplayTraceIntensity(value);
}

void GLScopeNGGUI::on_gridIntensity_valueChanged(int value)
{
    ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(value));
    m_glScope->setDisplayGridIntensity(value);
}

void GLScopeNGGUI::on_time_valueChanged(int value)
{
    m_timeBase = value;
    setTimeScaleDisplay();
    setTraceDelayDisplay();
    m_glScope->setTimeBase(m_timeBase);
}

void GLScopeNGGUI::on_timeOfs_valueChanged(int value)
{
    if ((value < 0) || (value > 100)) {
        return;
    }

    m_timeOffset = value;
    setTimeOfsDisplay();
    m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
    		m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked());
}

void GLScopeNGGUI::on_traceLen_valueChanged(int value)
{
    if ((value < 1) || (value > 100)) {
        return;
    }

    m_traceLenMult = value;
    m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
    		m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked());
    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setTrigDelayDisplay();
    setTrigPreDisplay();
}

void GLScopeNGGUI::on_trace_valueChanged(int value)
{
    //ui->traceText->setText(tr("%1").arg(value)); TODO: restore when more than 2 traces are supported
    ui->traceText->setText(value == 0 ? "X" : "Y");

    ScopeVisNG::TraceData traceData;
    m_scopeVis->getTraceData(traceData, value);

    qDebug() << "GLScopeNGGUI::on_trace_valueChanged:"
            << " m_projectionType: " << (int) traceData.m_projectionType
            << " m_amp" << traceData.m_amp
            << " m_ofs" << traceData.m_ofs
            << " m_traceDelay" << traceData.m_traceDelay;

    setTraceUI(traceData);

    m_scopeVis->focusOnTrace(value);
}

void GLScopeNGGUI::on_traceAdd_clicked(bool checked)
{
    if (ui->trace->maximum() < 1) // TODO: extend to more traces when 2 traces are fully stable
    {
        if (ui->trace->value() == 0)
        {
            ui->onlyY->setEnabled(true);
            ui->horizontalXY->setEnabled(true);
            ui->verticalXY->setEnabled(true);
            ui->polar->setEnabled(true);
        }

        ScopeVisNG::TraceData traceData;
        fillTraceData(traceData);
        m_scopeVis->addTrace(traceData);
        ui->trace->setMaximum(ui->trace->maximum() + 1);
    }
}

void GLScopeNGGUI::on_traceDel_clicked(bool checked)
{
    if (ui->trace->value() > 0)
    {
        ui->trace->setMaximum(ui->trace->maximum() - 1);

        if (ui->trace->value() == 0)
        {
            ui->onlyX->setChecked(true);
            ui->onlyY->setEnabled(false);
            ui->horizontalXY->setEnabled(false);
            ui->verticalXY->setEnabled(false);
            ui->polar->setEnabled(false);
            m_glScope->setDisplayMode(GLScopeNG::DisplayX);
        }

        m_scopeVis->removeTrace(ui->trace->value());
        changeCurrentTrace();
    }
}


void GLScopeNGGUI::on_trig_valueChanged(int value)
{
    ui->trigText->setText(tr("%1").arg(value));

    ScopeVisNG::TriggerData triggerData;
    m_scopeVis->getTriggerData(triggerData, value);

    qDebug() << "GLScopeNGGUI::on_trig_valueChanged:"
            << " m_projectionType: " << (int) triggerData.m_projectionType
            << " m_triggerRepeat" << triggerData.m_triggerRepeat
            << " m_triggerPositiveEdge" << triggerData.m_triggerPositiveEdge
            << " m_triggerBothEdges" << triggerData.m_triggerBothEdges
            << " m_triggerLevel" << triggerData.m_triggerLevel;

    setTriggerUI(triggerData);

    m_scopeVis->focusOnTrigger(value);
}

void GLScopeNGGUI::on_trigAdd_clicked(bool checked)
{
    if (ui->trig->maximum() < 9)
    {
        ScopeVisNG::TriggerData triggerData;
        fillTriggerData(triggerData);
        m_scopeVis->addTrigger(triggerData);
        ui->trig->setMaximum(ui->trig->maximum() + 1);
    }
}

void GLScopeNGGUI::on_trigDel_clicked(bool checked)
{
    if (ui->trig->value() > 0)
    {
        m_scopeVis->removeTrigger(ui->trig->value());
        ui->trig->setMaximum(ui->trig->maximum() - 1);
    }
}

void GLScopeNGGUI::on_traceMode_currentIndexChanged(int index)
{
    setAmpScaleDisplay();
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeNGGUI::on_amp_valueChanged(int value)
{
    setAmpScaleDisplay();
    changeCurrentTrace();
}

void GLScopeNGGUI::on_ofsCoarse_valueChanged(int value)
{
	setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeNGGUI::on_ofsFine_valueChanged(int value)
{
	setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeNGGUI::on_traceDelayCoarse_valueChanged(int value)
{
	setTraceDelayDisplay();
	changeCurrentTrace();
}

void GLScopeNGGUI::on_traceDelayFine_valueChanged(int value)
{
    setTraceDelayDisplay();
    changeCurrentTrace();
}

void GLScopeNGGUI::on_traceColor_clicked()
{
    QColor newColor = QColorDialog::getColor(m_focusedTraceColor);

    if (newColor.isValid()) // user clicked OK and selected a color
    {
        m_focusedTraceColor = newColor;
        int r,g,b,a;
        m_focusedTraceColor.getRgb(&r, &g, &b, &a);
        ui->traceColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        changeCurrentTrace();
    }
}

void GLScopeNGGUI::on_trigMode_currentIndexChanged(int index)
{
	setTrigLevelDisplay();
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigCount_valueChanged(int value)
{
    setTrigCountDisplay();
    changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigPos_toggled(bool checked)
{
    if (checked)
    {
    	ui->trigNeg->setChecked(false);
    	ui->trigBoth->setChecked(false);
    }
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigNeg_toggled(bool checked)
{
    if (checked)
    {
    	ui->trigPos->setChecked(false);
    	ui->trigBoth->setChecked(false);
    }
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigBoth_toggled(bool checked)
{
    if (checked)
    {
    	ui->trigNeg->setChecked(false);
    	ui->trigPos->setChecked(false);
    }
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigLevelCoarse_valueChanged(int value)
{
	setTrigLevelDisplay();
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigLevelFine_valueChanged(int value)
{
	setTrigLevelDisplay();
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigDelayCoarse_valueChanged(int value)
{
	setTrigDelayDisplay();
	changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigDelayFine_valueChanged(int value)
{
    setTrigDelayDisplay();
    changeCurrentTrigger();
}

void GLScopeNGGUI::on_trigPre_valueChanged(int value)
{
	setTrigPreDisplay();
    m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
            m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
            ui->freerun->isChecked()); // TODO: implement one shot feature
}

void GLScopeNGGUI::on_trigColor_clicked()
{
    QColor newColor = QColorDialog::getColor(m_focusedTriggerColor);

    if (newColor.isValid()) // user clicked "OK"
    {
        m_focusedTriggerColor = newColor;
        int r,g,b,a;
        m_focusedTriggerColor.getRgb(&r, &g, &b, &a);
        ui->trigColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        changeCurrentTrigger();
    }
}

void GLScopeNGGUI::on_trigOneShot_toggled(bool checked)
{
	m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
			m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked()); // TODO: implement one shot feature
}

void GLScopeNGGUI::on_freerun_toggled(bool checked)
{
	if (checked)
	{
		ui->trigOneShot->setChecked(false);
        ui->trigOneShot->setEnabled(false);
	}
	else
	{
        ui->trigOneShot->setEnabled(true);
	}

    m_scopeVis->configure(m_traceLenMult*ScopeVisNG::m_traceChunkSize,
    		m_timeOffset*10,
            (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
			ui->freerun->isChecked()); // TODO: implement one shot feature
}

void GLScopeNGGUI::setTraceIndexDisplay()
{
	ui->traceText->setText(tr("%1").arg(ui->trace->value()));
}

void GLScopeNGGUI::setTrigCountDisplay()
{
    QString text;
    text.sprintf("%02d", ui->trigCount->value());
    ui->trigCountText->setText(text);
}

void GLScopeNGGUI::setTimeScaleDisplay()
{
    m_sampleRate = m_glScope->getSampleRate();
    double t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate) / (double) m_timeBase;

    if(t < 0.000001)
    {
        t = round(t * 100000000000.0) / 100.0;
        ui->timeText->setText(tr("%1\nns").arg(t));
    }
    else if(t < 0.001)
    {
        t = round(t * 100000000.0) / 100.0;
        ui->timeText->setText(tr("%1\nµs").arg(t));
    }
    else if(t < 1.0)
    {
        t = round(t * 100000.0) / 100.0;
        ui->timeText->setText(tr("%1\nms").arg(t));
    }
    else
    {
        t = round(t * 100.0) / 100.0;
        ui->timeText->setText(tr("%1\ns").arg(t));
    }
}

void GLScopeNGGUI::setTraceLenDisplay()
{
    unsigned int n_samples = m_traceLenMult * ScopeVisNG::m_traceChunkSize;

    if (n_samples < 1000) {
        ui->traceLenText->setToolTip(tr("%1S").arg(n_samples));
    } else if (n_samples < 1000000) {
        ui->traceLenText->setToolTip(tr("%1kS").arg(n_samples/1000.0));
    } else {
        ui->traceLenText->setToolTip(tr("%1MS").arg(n_samples/1000000.0));
    }

    m_sampleRate = m_glScope->getSampleRate();
    double t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate);

    if(t < 0.000001)
        ui->traceLenText->setText(tr("%1\nns").arg(t * 1000000000.0));
    else if(t < 0.001)
        ui->traceLenText->setText(tr("%1\nµs").arg(t * 1000000.0));
    else if(t < 1.0)
        ui->traceLenText->setText(tr("%1\nms").arg(t * 1000.0));
    else
        ui->traceLenText->setText(tr("%1\ns").arg(t * 1.0));
}

void GLScopeNGGUI::setTimeOfsDisplay()
{
    double dt = m_glScope->getTraceSize() * (m_timeOffset/100.0) / m_sampleRate;

    if(dt < 0.000001)
        ui->timeOfsText->setText(tr("%1\nns").arg(dt * 1000000000.0));
    else if(dt < 0.001)
        ui->timeOfsText->setText(tr("%1\nµs").arg(dt * 1000000.0));
    else if(dt < 1.0)
        ui->timeOfsText->setText(tr("%1\nms").arg(dt * 1000.0));
    else
        ui->timeOfsText->setText(tr("%1\ns").arg(dt * 1.0));
}

void GLScopeNGGUI::setAmpScaleDisplay()
{
    ScopeVisNG::ProjectionType projectionType = (ScopeVisNG::ProjectionType) ui->traceMode->currentIndex();
    double ampValue = amps[ui->amp->value()];

    if (projectionType == ScopeVisNG::ProjectionMagDB)
    {
        double displayValue = ampValue*500.0f;

        if (displayValue < 10.0f) {
            ui->ampText->setText(tr("%1\ndB").arg(displayValue, 0, 'f', 2));
        }
        else {
            ui->ampText->setText(tr("%1\ndB").arg(displayValue, 0, 'f', 1));
        }
    }
    else
    {
        double a = ampValue*10.0f;

        if(a < 0.000001)
            ui->ampText->setText(tr("%1\nn").arg(a * 1000000000.0));
        else if(a < 0.001)
            ui->ampText->setText(tr("%1\nµ").arg(a * 1000000.0));
        else if(a < 1.0)
            ui->ampText->setText(tr("%1\nm").arg(a * 1000.0));
        else
            ui->ampText->setText(tr("%1").arg(a * 1.0));
    }
}

void GLScopeNGGUI::setAmpOfsDisplay()
{
	ScopeVisNG::ProjectionType projectionType = (ScopeVisNG::ProjectionType) ui->traceMode->currentIndex();
	double o = (ui->ofsCoarse->value() * 10.0f) + (ui->ofsFine->value() / 20.0f);

    if (projectionType == ScopeVisNG::ProjectionMagDB)
    {
    	ui->ofsText->setText(tr("%1\ndB").arg(o/10.0f - 100.0f, 0, 'f', 1));
    }
    else
    {
        double a;

        if (projectionType == ScopeVisNG::ProjectionMagLin)
        {
            a = o/2000.0f;
        }
        else
        {
            a = o/1000.0f;
        }

		if(fabs(a) < 0.000001f)
			ui->ofsText->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(fabs(a) < 0.001f)
			ui->ofsText->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(fabs(a) < 1.0f)
			ui->ofsText->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->ofsText->setText(tr("%1").arg(a * 1.0));
    }
}

void GLScopeNGGUI::setTraceDelayDisplay()
{
    int n_samples = ui->traceDelayCoarse->value()*100 + ui->traceDelayFine->value();
    double t = ((double) n_samples) / m_sampleRate;

    if (n_samples < 1000) {
        ui->traceDelayText->setToolTip(tr("%1 S").arg(n_samples));
    } else if (n_samples < 1000000) {
        ui->traceDelayText->setToolTip(tr("%1 kS").arg(n_samples/1000.0));
    } else {
        ui->traceDelayText->setToolTip(tr("%1 MS").arg(n_samples/1000000.0));
    }

    if(t < 0.000001)
        ui->traceDelayText->setText(tr("%1\nns").arg(t * 1000000000.0, 0, 'f', 2));
    else if(t < 0.001)
        ui->traceDelayText->setText(tr("%1\nµs").arg(t * 1000000.0, 0, 'f', 2));
    else if(t < 1.0)
        ui->traceDelayText->setText(tr("%1\nms").arg(t * 1000.0, 0, 'f', 2));
    else
        ui->traceDelayText->setText(tr("%1\ns").arg(t * 1.0, 0, 'f', 2));
}

void GLScopeNGGUI::setTrigIndexDisplay()
{
	ui->trigText->setText(tr("%1").arg(ui->trig->value()));
}

void GLScopeNGGUI::setTrigLevelDisplay()
{
    double t = (ui->trigLevelCoarse->value() / 100.0f) + (ui->trigLevelFine->value() / 20000.0f);
	ScopeVisNG::ProjectionType projectionType = (ScopeVisNG::ProjectionType) ui->trigMode->currentIndex();

    ui->trigLevelCoarse->setToolTip(QString("Trigger level coarse: %1 %").arg(ui->trigLevelCoarse->value() / 100.0f));
    ui->trigLevelFine->setToolTip(QString("Trigger level fine: %1 ppm").arg(ui->trigLevelFine->value() * 50));

	if (projectionType == ScopeVisNG::ProjectionMagDB) {
		ui->trigLevelText->setText(tr("%1\ndB").arg(100.0 * (t - 1.0), 0, 'f', 1));
	}
	else
	{
	    double a;

		if (projectionType == ScopeVisNG::ProjectionMagLin) {
			a = 1.0 + t;
		} else {
			a = t;
		}

		if(fabs(a) < 0.000001)
			ui->trigLevelText->setText(tr("%1\nn").arg(a * 1000000000.0f, 0, 'f', 2));
		else if(fabs(a) < 0.001)
			ui->trigLevelText->setText(tr("%1\nµ").arg(a * 1000000.0f, 0, 'f', 2));
		else if(fabs(a) < 1.0)
			ui->trigLevelText->setText(tr("%1\nm").arg(a * 1000.0f, 0, 'f', 2));
		else
			ui->trigLevelText->setText(tr("%1").arg(a * 1.0f, 0, 'f', 2));
	}
}

void GLScopeNGGUI::setTrigDelayDisplay()
{
    double delayMult = ui->trigDelayCoarse->value() + ui->trigDelayFine->value() / (ScopeVisNG::m_traceChunkSize / 10.0);
    unsigned int n_samples_delay = m_traceLenMult * ScopeVisNG::m_traceChunkSize * delayMult;

	if (n_samples_delay < 1000) {
		ui->trigDelayText->setToolTip(tr("%1S").arg(n_samples_delay));
	} else if (n_samples_delay < 1000000) {
		ui->trigDelayText->setToolTip(tr("%1kS").arg(n_samples_delay/1000.0));
	} else if (n_samples_delay < 1000000000) {
		ui->trigDelayText->setToolTip(tr("%1MS").arg(n_samples_delay/1000000.0));
	} else {
		ui->trigDelayText->setToolTip(tr("%1GS").arg(n_samples_delay/1000000000.0));
	}

	m_sampleRate = m_glScope->getSampleRate();
	double t = (n_samples_delay * 1.0f / m_sampleRate);

	if(t < 0.000001)
		ui->trigDelayText->setText(tr("%1\nns").arg(t * 1000000000.0, 0, 'f', 1));
	else if(t < 0.001)
		ui->trigDelayText->setText(tr("%1\nµs").arg(t * 1000000.0, 0, 'f', 1));
	else if(t < 1.0)
		ui->trigDelayText->setText(tr("%1\nms").arg(t * 1000.0, 0, 'f', 1));
	else
		ui->trigDelayText->setText(tr("%1\ns").arg(t * 1.0, 0, 'f', 1));
}

void GLScopeNGGUI::setTrigPreDisplay()
{
    double dt = m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f) / m_sampleRate;

	if(dt < 0.000001)
		ui->trigPreText->setText(tr("%1\nns").arg(dt * 1000000000.0f));
	else if(dt < 0.001)
		ui->trigPreText->setText(tr("%1\nµs").arg(dt * 1000000.0f));
	else if(dt < 1.0)
		ui->trigPreText->setText(tr("%1\nms").arg(dt * 1000.0f));
	else
		ui->trigPreText->setText(tr("%1\ns").arg(dt * 1.0f));
}

void GLScopeNGGUI::changeCurrentTrace()
{
    ScopeVisNG::TraceData traceData;
    fillTraceData(traceData);
    uint32_t currentTraceIndex = ui->trace->value();
    m_scopeVis->changeTrace(traceData, currentTraceIndex);
}

void GLScopeNGGUI::changeCurrentTrigger()
{
    ScopeVisNG::TriggerData triggerData;
    fillTriggerData(triggerData);
    uint32_t currentTriggerIndex = ui->trig->value();
    m_scopeVis->changeTrigger(triggerData, currentTriggerIndex);
}

void GLScopeNGGUI::fillProjectionCombo(QComboBox* comboBox)
{
    comboBox->addItem("Real", ScopeVisNG::ProjectionReal);
    comboBox->addItem("Imag", ScopeVisNG::ProjectionImag);
    comboBox->addItem("Mag", ScopeVisNG::ProjectionMagLin);
    comboBox->addItem("MagdB", ScopeVisNG::ProjectionMagDB);
    comboBox->addItem("Phi", ScopeVisNG::ProjectionPhase);
    comboBox->addItem("dPhi", ScopeVisNG::ProjectionDPhase);
}

void GLScopeNGGUI::fillTraceData(ScopeVisNG::TraceData& traceData)
{
    traceData.m_projectionType = (ScopeVisNG::ProjectionType) ui->traceMode->currentIndex();
    traceData.m_inputIndex = 0;
    traceData.m_amp = 0.2 / amps[ui->amp->value()];
    traceData.m_ampIndex = ui->amp->value();

    traceData.m_ofsCoarse = ui->ofsCoarse->value();
    traceData.m_ofsFine = ui->ofsFine->value();

    if (traceData.m_projectionType == ScopeVisNG::ProjectionMagLin) {
        traceData.m_ofs = ((10.0 * ui->ofsCoarse->value()) + (ui->ofsFine->value() / 20.0)) / 2000.0f;
    } else {
        traceData.m_ofs = ((10.0 * ui->ofsCoarse->value()) + (ui->ofsFine->value() / 20.0)) / 1000.0f;
    }

    traceData.m_traceDelayCoarse = ui->traceDelayCoarse->value();
    traceData.m_traceDelayFine = ui->traceDelayFine->value();
    traceData.m_traceDelay = traceData.m_traceDelayCoarse * 100 + traceData.m_traceDelayFine;

    traceData.setColor(m_focusedTraceColor);
}

void GLScopeNGGUI::fillTriggerData(ScopeVisNG::TriggerData& triggerData)
{
    triggerData.m_projectionType = (ScopeVisNG::ProjectionType) ui->trigMode->currentIndex();
    triggerData.m_inputIndex = 0;
    triggerData.m_triggerLevel = (ui->trigLevelCoarse->value() / 100.0) + (ui->trigLevelFine->value() / 20000.0);
    triggerData.m_triggerLevelCoarse = ui->trigLevelCoarse->value();
    triggerData.m_triggerLevelFine = ui->trigLevelFine->value();
    triggerData.m_triggerPositiveEdge = ui->trigPos->isChecked();
    triggerData.m_triggerBothEdges = ui->trigBoth->isChecked();
    triggerData.m_triggerRepeat = ui->trigCount->value();
    triggerData.m_triggerDelayMult = ui->trigDelayCoarse->value() + ui->trigDelayFine->value() / (ScopeVisNG::m_traceChunkSize / 10.0);
    triggerData.m_triggerDelay = (int) (m_traceLenMult * ScopeVisNG::m_traceChunkSize * triggerData.m_triggerDelayMult);
    triggerData.m_triggerDelayCoarse = ui->trigDelayCoarse->value();
    triggerData.m_triggerDelayFine = ui->trigDelayFine->value();
    triggerData.setColor(m_focusedTriggerColor);
}

void GLScopeNGGUI::setTraceUI(ScopeVisNG::TraceData& traceData)
{
    TraceUIBlocker traceUIBlocker(ui);

    ui->traceMode->setCurrentIndex((int) traceData.m_projectionType);
    ui->amp->setValue(traceData.m_ampIndex);
    setAmpScaleDisplay();

    ui->ofsCoarse->setValue(traceData.m_ofsCoarse);
    ui->ofsFine->setValue(traceData.m_ofsFine);
    setAmpOfsDisplay();

    ui->traceDelayCoarse->setValue(traceData.m_traceDelayCoarse);
    ui->traceDelayFine->setValue(traceData.m_traceDelayFine);
    setTraceDelayDisplay();

    m_focusedTraceColor = traceData.m_traceColor;
    int r, g, b, a;
    m_focusedTraceColor.getRgb(&r, &g, &b, &a);
    ui->traceColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
}

void GLScopeNGGUI::setTriggerUI(ScopeVisNG::TriggerData& triggerData)
{
    TrigUIBlocker trigUIBlocker(ui);

    ui->trigMode->setCurrentIndex((int) triggerData.m_projectionType);
    ui->trigCount->setValue(triggerData.m_triggerRepeat);
    setTrigCountDisplay();

    ui->trigPos->setChecked(false);
    ui->trigNeg->setChecked(false);
    ui->trigPos->doToggle(false);
    ui->trigNeg->doToggle(false);

    if (triggerData.m_triggerBothEdges)
    {
        ui->trigBoth->setChecked(true);
        ui->trigBoth->doToggle(true);
    }
    else
    {
        ui->trigBoth->setChecked(false);
        ui->trigBoth->doToggle(false);

        if (triggerData.m_triggerPositiveEdge)
        {
            ui->trigPos->setChecked(true);
            ui->trigPos->doToggle(true);
        }
        else
        {
            ui->trigNeg->setChecked(true);
            ui->trigNeg->doToggle(true);
        }
    }

    ui->trigLevelCoarse->setValue(triggerData.m_triggerLevelCoarse);
    ui->trigLevelFine->setValue(triggerData.m_triggerLevelFine);
    setTrigLevelDisplay();

    ui->trigDelayCoarse->setValue(triggerData.m_triggerDelayCoarse);
    ui->trigDelayFine->setValue(triggerData.m_triggerDelayFine);
    setTrigDelayDisplay();

    m_focusedTriggerColor = triggerData.m_triggerColor;
    int r, g, b, a;
    m_focusedTriggerColor.getRgb(&r, &g, &b, &a);
    ui->trigColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
}

void GLScopeNGGUI::applySettings()
{
}

bool GLScopeNGGUI::handleMessage(Message* message)
{
    return false;
}

GLScopeNGGUI::TrigUIBlocker::TrigUIBlocker(Ui::GLScopeNGGUI *ui) :
        m_ui(ui)
{
    m_oldStateTrigMode        = ui->trigMode->blockSignals(true);
    m_oldStateTrigCount       = ui->trigCount->blockSignals(true);
    m_oldStateTrigPos         = ui->trigPos->blockSignals(true);
    m_oldStateTrigNeg         = ui->trigNeg->blockSignals(true);
    m_oldStateTrigBoth        = ui->trigBoth->blockSignals(true);
    m_oldStateTrigLevelCoarse = ui->trigLevelCoarse->blockSignals(true);
    m_oldStateTrigLevelFine   = ui->trigLevelFine->blockSignals(true);
    m_oldStateTrigDelayCoarse = ui->trigDelayCoarse->blockSignals(true);
    m_oldStateTrigDelayFine   = ui->trigDelayFine->blockSignals(true);
}

GLScopeNGGUI::TrigUIBlocker::~TrigUIBlocker()
{
    unBlock();
}

void GLScopeNGGUI::TrigUIBlocker::unBlock()
{
    m_ui->trigMode->blockSignals(m_oldStateTrigMode);
    m_ui->trigCount->blockSignals(m_oldStateTrigCount);
    m_ui->trigPos->blockSignals(m_oldStateTrigPos);
    m_ui->trigNeg->blockSignals(m_oldStateTrigNeg);
    m_ui->trigBoth->blockSignals(m_oldStateTrigBoth);
    m_ui->trigLevelCoarse->blockSignals(m_oldStateTrigLevelCoarse);
    m_ui->trigLevelFine->blockSignals(m_oldStateTrigLevelFine);
    m_ui->trigDelayCoarse->blockSignals(m_oldStateTrigDelayCoarse);
    m_ui->trigDelayFine->blockSignals(m_oldStateTrigDelayFine);
}

GLScopeNGGUI::TraceUIBlocker::TraceUIBlocker(Ui::GLScopeNGGUI* ui) :
        m_ui(ui)
{
    m_oldStateTraceMode        = m_ui->traceMode->blockSignals(true);
    m_oldStateAmp              = m_ui->amp->blockSignals(true);
    m_oldStateOfsCoarse        = m_ui->ofsCoarse->blockSignals(true);
    m_oldStateOfsFine          = m_ui->ofsFine->blockSignals(true);
    m_oldStateTraceDelayCoarse = m_ui->traceDelayCoarse->blockSignals(true);
    m_oldStateTraceDelayFine   = m_ui->traceDelayFine->blockSignals(true);
    m_oldStateZSelect          = m_ui->zSelect->blockSignals(true);
    m_oldStateZTraceMode       = m_ui->zTraceMode->blockSignals(true);
    m_oldStateTraceColor       = m_ui->traceColor->blockSignals(true);
}

GLScopeNGGUI::TraceUIBlocker::~TraceUIBlocker()
{
    unBlock();
}

void GLScopeNGGUI::TraceUIBlocker::unBlock()
{
    m_ui->traceMode->blockSignals(m_oldStateTraceMode);
    m_ui->amp->blockSignals(m_oldStateAmp);
    m_ui->ofsCoarse->blockSignals(m_oldStateOfsCoarse);
    m_ui->ofsFine->blockSignals(m_oldStateOfsFine);
    m_ui->traceDelayCoarse->blockSignals(m_oldStateTraceDelayCoarse);
    m_ui->traceDelayFine->blockSignals(m_oldStateTraceDelayFine);
    m_ui->zSelect->blockSignals(m_oldStateZSelect);
    m_ui->zTraceMode->blockSignals(m_oldStateZTraceMode);
    m_ui->traceColor->blockSignals(m_oldStateTraceColor);
}

GLScopeNGGUI::MainUIBlocker::MainUIBlocker(Ui::GLScopeNGGUI* ui) :
        m_ui(ui)
{
    m_oldStateOnlyX        = m_ui->onlyX->blockSignals(true);
    m_oldStateOnlyY        = m_ui->onlyY->blockSignals(true);
    m_oldStateHorizontalXY = m_ui->horizontalXY->blockSignals(true);
    m_oldStateVerticalXY   = m_ui->verticalXY->blockSignals(true);
    m_oldStatePolar        = m_ui->polar->blockSignals(true);
//    m_oldStateTime         = m_ui->time->blockSignals(true);
//    m_oldStateTimeOfs      = m_ui->timeOfs->blockSignals(true);
//    m_oldStateTraceLen     = m_ui->traceLen->blockSignals(true);
}

GLScopeNGGUI::MainUIBlocker::~MainUIBlocker()
{
    unBlock();
}

void GLScopeNGGUI::MainUIBlocker::unBlock()
{
    m_ui->onlyX->blockSignals(m_oldStateOnlyX);
    m_ui->onlyY->blockSignals(m_oldStateOnlyY);
    m_ui->horizontalXY->blockSignals(m_oldStateHorizontalXY);
    m_ui->verticalXY->blockSignals(m_oldStateVerticalXY);
    m_ui->polar->blockSignals(m_oldStatePolar);
//    m_ui->time->blockSignals(m_oldStateTime);
//    m_ui->timeOfs->blockSignals(m_oldStateTimeOfs);
//    m_ui->traceLen->blockSignals(m_oldStateTraceLen);
}

