///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>

#include "glscopegui.h"
#include "glscope.h"
#include "ui_glscopegui.h"
#include "gui/dialpopup.h"
#include "util/simpleserializer.h"
#include "util/db.h"

GLScopeGUI::GLScopeGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::GLScopeGUI),
    m_messageQueue(nullptr),
    m_scopeVis(nullptr),
    m_glScope(nullptr),
    m_sampleRate(0),
    m_timeBase(1),
    m_timeOffset(0),
    m_ctlTraceIndex(0),
    m_ctlTriggerIndex(0)
{
    qDebug("GLScopeGUI::GLScopeGUI");
    setEnabled(false);
    ui->setupUi(this);
    ui->trigDelayFine->setMaximum(GLScopeSettings::m_traceChunkDefaultSize / 10.0);
    ui->traceColor->setStyleSheet("QLabel { background-color : rgb(255,255,64); }");
    m_focusedTraceColor.setRgb(255,255,64);
    ui->trigColor->setStyleSheet("QLabel { background-color : rgb(0,255,0); }");
    m_focusedTriggerColor.setRgb(0,255,0);
    ui->traceText->setText("X");
    ui->mem->setMaximum(GLScopeSettings::m_nbTraceMemories - 1);
    DialPopup::addPopupsToChildDials(this);
}

GLScopeGUI::~GLScopeGUI()
{
    delete ui;
}

void GLScopeGUI::setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope)
{
    qDebug("GLScopeGUI::setBuddies");

    m_messageQueue = messageQueue;
    m_scopeVis = scopeVis;
    m_glScope = glScope;

    // initialize display combo
    ui->onlyY->setEnabled(false);
    ui->horizontalXY->setEnabled(false);
    ui->verticalXY->setEnabled(false);
    ui->polar->setEnabled(false);
    m_glScope->setDisplayMode(GLScope::DisplayX);

    // initialize trigger combo
    ui->trigPos->setChecked(true);
    ui->trigNeg->setChecked(false);
    ui->trigBoth->setChecked(false);
    ui->trigOneShot->setChecked(false);
    ui->trigOneShot->setEnabled(false);
    ui->freerun->setChecked(true);

    // Add a trigger
    GLScopeSettings::TriggerData triggerData;
    fillTriggerData(triggerData);
    ScopeVis::MsgScopeVisAddTrigger *msgAddTrigger = ScopeVis::MsgScopeVisAddTrigger::create(triggerData);
    m_scopeVis->getInputMessageQueue()->push(msgAddTrigger);
    settingsTriggerAdd(triggerData);

    // Add a trace
    GLScopeSettings::TraceData traceData;
    fillTraceData(traceData);
    ScopeVis::MsgScopeVisAddTrace *msgAddTrace = ScopeVis::MsgScopeVisAddTrace::create(traceData);
    m_scopeVis->getInputMessageQueue()->push(msgAddTrace);
    settingsTraceAdd(traceData);

    setEnabled(true);
    connect(m_glScope, SIGNAL(sampleRateChanged(int)), this, SLOT(onScopeSampleRateChanged(int)));
    connect(m_glScope, SIGNAL(traceSizeChanged(uint32_t)), this, SLOT(onScopeTraceSizeChanged(uint32_t)));
    connect(m_glScope, SIGNAL(preTriggerChanged(uint32_t)), this, SLOT(onScopePreTriggerChanged(uint32_t)));

    ui->traceMode->clear();
    fillProjectionCombo(ui->traceMode);

    ui->trigMode->clear();
    fillProjectionCombo(ui->trigMode);

    m_scopeVis->configure(
        2*m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );

    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );

    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setAmpScaleDisplay();
    setAmpOfsDisplay();
    setTraceDelayDisplay();
}

void GLScopeGUI::setSampleRate(int sampleRate)
{
    onScopeSampleRateChanged(sampleRate);
}

void GLScopeGUI::onScopeSampleRateChanged(int sampleRate)
{
    //m_sampleRate = m_glScope->getSampleRate();
    m_sampleRate = sampleRate;
    ui->sampleRateText->setText(tr("%1\nkS/s").arg(m_sampleRate / 1000.0f, 0, 'f', 2));
    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setTraceDelayDisplay();
    setTrigPreDisplay();
    setTrigDelayDisplay();
}

void GLScopeGUI::onScopeTraceSizeChanged(uint32_t traceNbSamples)
{
    qDebug("GLScopeGUI::onScopeTraceSizeChanged: %u", traceNbSamples);
    m_settings.m_traceLenMult = traceNbSamples / m_scopeVis->getTraceChunkSize();
    ui->traceLen->setValue(m_settings.m_traceLenMult);
    setTraceLenDisplay();
}

void GLScopeGUI::onScopePreTriggerChanged(uint32_t preTriggerNbSamples)
{
    qDebug("GLScopeGUI::onScopePreTriggerChanged: %u", preTriggerNbSamples);
    ui->trigPre->setValue(preTriggerNbSamples*100 / m_glScope->getTraceSize()); // slider position is a percentage value of the trace size
    setTrigPreDisplay();
}

void GLScopeGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray GLScopeGUI::serialize() const
{
    return m_settings.serialize();
}

bool GLScopeGUI::deserialize(const QByteArray& data)
{
    bool ret;

    if (m_settings.deserialize(data))
    {
        displaySettings();
        ret = true;
    }
    else
    {
        resetToDefaults();
        ret = false;
    }

    // trace stuff

    const std::vector<GLScopeSettings::TraceData>& tracesData = m_scopeVis->getTracesData();
    uint32_t iTrace = tracesData.size();
    uint32_t nbTracesSaved = m_settings.m_tracesData.size();
    ui->trace->setMaximum(nbTracesSaved-1);

    while (iTrace > nbTracesSaved) // remove possible traces in excess
    {
        ScopeVis::MsgScopeVisRemoveTrace *msg = ScopeVis::MsgScopeVisRemoveTrace::create(iTrace - 1);
        m_scopeVis->getInputMessageQueue()->push(msg);
        iTrace--;
    }

    for (iTrace = 0; iTrace < nbTracesSaved; iTrace++)
    {
        GLScopeSettings::TraceData& traceData = m_settings.m_tracesData[iTrace];

        if (iTrace < tracesData.size()) // change existing traces
        {
            ScopeVis::MsgScopeVisChangeTrace *msg = ScopeVis::MsgScopeVisChangeTrace::create(traceData, iTrace);
            m_scopeVis->getInputMessageQueue()->push(msg);
        }
        else // add new traces
        {
            ScopeVis::MsgScopeVisAddTrace *msg = ScopeVis::MsgScopeVisAddTrace::create(traceData);
            m_scopeVis->getInputMessageQueue()->push(msg);
        }
    }

    setTraceIndexDisplay();
    setAmpScaleDisplay();
    setAmpOfsDisplay();
    setTraceDelayDisplay();
    setDisplayMode(m_settings.m_displayMode);

    // trigger stuff

    uint32_t nbTriggersSaved = m_settings.m_triggersData.size();
    uint32_t nbTriggers = m_scopeVis->getNbTriggers();
    uint32_t iTrigger = nbTriggers;
    ui->trig->setMaximum(nbTriggersSaved-1);

    while (iTrigger > nbTriggersSaved) // remove possible triggers in excess
    {
        ScopeVis::MsgScopeVisRemoveTrigger *msg = ScopeVis::MsgScopeVisRemoveTrigger::create(iTrigger - 1);
        m_scopeVis->getInputMessageQueue()->push(msg);
        iTrigger--;
    }

    for (iTrigger = 0; iTrigger < nbTriggersSaved; iTrigger++)
    {
        GLScopeSettings::TriggerData& triggerData = m_settings.m_triggersData[iTrigger];

        if (iTrigger < nbTriggers) // change existing triggers
        {
            ScopeVis::MsgScopeVisChangeTrigger *msg = ScopeVis::MsgScopeVisChangeTrigger::create(triggerData, iTrigger);
            m_scopeVis->getInputMessageQueue()->push(msg);
        }
        else // add new trigers
        {
            ScopeVis::MsgScopeVisAddTrigger *msg = ScopeVis::MsgScopeVisAddTrigger::create(triggerData);
            m_scopeVis->getInputMessageQueue()->push(msg);
        }
    }

    setTrigCountDisplay();
    setTrigDelayDisplay();
    setTrigIndexDisplay();
    setTrigLevelDisplay();
    setTrigPreDisplay();

    displaySettings();

    return ret;
}

void GLScopeGUI::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
    m_settings.formatTo(swgObject);
}

void GLScopeGUI::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
    m_settings.updateFrom(keys, swgObject);
}

void GLScopeGUI::updateSettings()
{
    displaySettings();
    applySettings(m_settings); /// FIXME
}

void GLScopeGUI::setNbStreams(unsigned int nbStreams)
{
    QStringList streamNames;

    for (unsigned int s = 0; s < nbStreams; s++) {
        streamNames.append(tr("%1").arg(s));
    }

    setStreams(streamNames);
}

void GLScopeGUI::setStreams(const QStringList& streamNames)
{
    int traceStreamIndex = ui->traceStream->currentIndex();
    int triggerStreamIndex = ui->trigStream->currentIndex();
    ui->traceStream->blockSignals(true);
    ui->trigStream->blockSignals(true);
    ui->traceStream->clear();
    ui->trigStream->clear();

    for (QString s : streamNames)
    {
        ui->traceStream->addItem(s);
        ui->trigStream->addItem(s);
    }

    int newTraceStreamIndex = traceStreamIndex < streamNames.size() ? traceStreamIndex : streamNames.size() - 1;
    int newTriggerStreamIndex = triggerStreamIndex < streamNames.size() ? triggerStreamIndex: streamNames.size() - 1;

    ui->traceStream->setCurrentIndex(newTraceStreamIndex);

    if (newTraceStreamIndex != traceStreamIndex) {
        changeCurrentTrace();
    }

    ui->trigStream->setCurrentIndex(newTriggerStreamIndex);

    if (newTriggerStreamIndex != triggerStreamIndex) {
        changeCurrentTrigger();
    }

    ui->traceStream->blockSignals(false);
    ui->trigStream->blockSignals(false);
}

void GLScopeGUI::on_onlyX_toggled(bool checked)
{
    if (checked)
    {
        m_glScope->setDisplayMode(GLScope::DisplayX);
        ui->onlyX->setEnabled(false);
        ui->onlyY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->horizontalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->verticalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->polar->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_settings.m_displayMode = GLScopeSettings::DisplayX;
        m_scopeVis->configure(
            m_settings.m_displayMode,
            m_settings.m_traceIntensity,
            m_settings.m_gridIntensity
        );
    }
}

void GLScopeGUI::on_onlyY_toggled(bool checked)
{
    if (checked)
    {
        m_glScope->setDisplayMode(GLScope::DisplayY);
        ui->onlyX->setEnabled(true);
        ui->onlyY->setEnabled(false);
        ui->horizontalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->verticalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->polar->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->onlyX->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_settings.m_displayMode = GLScopeSettings::DisplayY;
        m_scopeVis->configure(
            m_settings.m_displayMode,
            m_settings.m_traceIntensity,
            m_settings.m_gridIntensity
        );
    }
}

void GLScopeGUI::on_horizontalXY_toggled(bool checked)
{
    if (checked)
    {
        m_glScope->setDisplayMode(GLScope::DisplayXYH);
        ui->onlyX->setEnabled(true);
        ui->onlyY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->horizontalXY->setEnabled(false);
        ui->verticalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->polar->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->verticalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_settings.m_displayMode = GLScopeSettings::DisplayXYH;
        m_scopeVis->configure(
            m_settings.m_displayMode,
            m_settings.m_traceIntensity,
            m_settings.m_gridIntensity
        );
    }
}

void GLScopeGUI::on_verticalXY_toggled(bool checked)
{
    if (checked)
    {
        m_glScope->setDisplayMode(GLScope::DisplayXYV);
        ui->onlyX->setEnabled(true);
        ui->onlyY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->horizontalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->verticalXY->setEnabled(false);
        ui->polar->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->polar->setChecked(false);
        m_settings.m_displayMode = GLScopeSettings::DisplayXYV;
        m_scopeVis->configure(
            m_settings.m_displayMode,
            m_settings.m_traceIntensity,
            m_settings.m_gridIntensity
        );
    }
}

void GLScopeGUI::on_polar_toggled(bool checked)
{
    if (checked)
    {
        m_glScope->setDisplayMode(GLScope::DisplayPol);
        ui->onlyX->setEnabled(true);
        ui->onlyY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->horizontalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->verticalXY->setEnabled(m_scopeVis->getNbTraces() > 1);
        ui->polar->setEnabled(false);
        ui->onlyX->setChecked(false);
        ui->onlyY->setChecked(false);
        ui->horizontalXY->setChecked(false);
        ui->verticalXY->setChecked(false);
        m_settings.m_displayMode = GLScopeSettings::DisplayPol;
        m_scopeVis->configure(
            m_settings.m_displayMode,
            m_settings.m_traceIntensity,
            m_settings.m_gridIntensity
        );
    }
}

void GLScopeGUI::on_polarPoints_toggled(bool checked)
{
    m_glScope->setDisplayXYPoints(checked);
}

void GLScopeGUI::on_polarGrid_toggled(bool checked)
{
    m_glScope->setDisplayXYPolarGrid(checked);
}

void GLScopeGUI::on_traceIntensity_valueChanged(int value)
{
    ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(value));
    m_glScope->setDisplayTraceIntensity(value);
    m_settings.m_traceIntensity = value;
    m_scopeVis->configure(
        m_settings.m_displayMode,
        m_settings.m_traceIntensity,
        m_settings.m_gridIntensity
    );
}

void GLScopeGUI::on_gridIntensity_valueChanged(int value)
{
    ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(value));
    m_glScope->setDisplayGridIntensity(value);
    m_settings.m_gridIntensity = value;
    m_scopeVis->configure(
        m_settings.m_displayMode,
        m_settings.m_traceIntensity,
        m_settings.m_gridIntensity
    );
}

void GLScopeGUI::on_time_valueChanged(int value)
{
    m_timeBase = value;
    m_settings.m_time = value;
    setTimeScaleDisplay();
    setTraceDelayDisplay();
    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );
}

void GLScopeGUI::on_timeOfs_valueChanged(int value)
{
    if ((value < 0) || (value > 100)) {
        return;
    }

    m_timeOffset = value;
    m_settings.m_timeOfs = value;
    setTimeOfsDisplay();
    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );

    if ((value > 0) && (ui->mem->value() == 0)) // switch from live to memory trace
    {
        ui->mem->setValue(1);
        ui->memText->setText("01");
    }
}

void GLScopeGUI::on_traceLen_valueChanged(int value)
{
    if ((value < 1) || (value > 100)) {
        return;
    }

    m_settings.m_traceLenMult = value;
    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );
    setTraceLenDisplay();
    setTimeScaleDisplay();
    setTimeOfsDisplay();
    setTrigDelayDisplay();
    setTrigPreDisplay();
}

void GLScopeGUI::on_trace_valueChanged(int value)
{
    ui->traceText->setText(value == 0 ? "X" : QString("Y%1").arg(ui->trace->value()));

    m_ctlTraceIndex = value;
    GLScopeSettings::TraceData traceData;
    m_scopeVis->getTraceData(traceData, value);

    qDebug() << "GLScopeGUI::on_trace_valueChanged:"
            << " m_projectionType: " << (int) traceData.m_projectionType
            << " m_amp" << traceData.m_amp
            << " m_ofs" << traceData.m_ofs
            << " m_traceDelay" << traceData.m_traceDelay;

    setTraceUI(traceData);

    ScopeVis::MsgScopeVisFocusOnTrace *msg = ScopeVis::MsgScopeVisFocusOnTrace::create(value);
    m_scopeVis->getInputMessageQueue()->push(msg);
}

void GLScopeGUI::on_traceAdd_clicked(bool checked)
{
    (void) checked;
    GLScopeSettings::TraceData traceData;
    fillTraceData(traceData);
    addTrace(traceData);
}

void GLScopeGUI::on_traceDel_clicked(bool checked)
{
    (void) checked;
    if (ui->trace->value() > 0) // not the X trace
    {
        ui->trace->setMaximum(ui->trace->maximum() - 1);

        if (ui->trace->maximum() == 0)
        {
            setDisplayMode(GLScopeSettings::DisplayX);
            m_glScope->setDisplayMode(GLScope::DisplayX);
        }

        ScopeVis::MsgScopeVisRemoveTrace *msg = ScopeVis::MsgScopeVisRemoveTrace::create(ui->trace->value());
        m_scopeVis->getInputMessageQueue()->push(msg);
        settingsTraceDel(ui->trace->value());

        changeCurrentTrace();
    }
}

void GLScopeGUI::on_traceUp_clicked(bool checked)
{
    (void) checked;
    if (ui->trace->maximum() > 0) // more than one trace
    {
        int newTraceIndex = (ui->trace->value() + 1) % (ui->trace->maximum()+1);
        ScopeVis::MsgScopeVisMoveTrace *msgMoveTrace = ScopeVis::MsgScopeVisMoveTrace::create(ui->trace->value(), true);
        m_scopeVis->getInputMessageQueue()->push(msgMoveTrace);
        settingsTraceMove(ui->trace->value(), true);
        ui->trace->setValue(newTraceIndex); // follow trace
        GLScopeSettings::TraceData traceData;
        m_scopeVis->getTraceData(traceData, ui->trace->value());
        setTraceUI(traceData);
        ScopeVis::MsgScopeVisFocusOnTrace *msgFocusOnTrace = ScopeVis::MsgScopeVisFocusOnTrace::create(ui->trace->value());
        m_scopeVis->getInputMessageQueue()->push(msgFocusOnTrace);
    }
}

void GLScopeGUI::on_traceDown_clicked(bool checked)
{
    (void) checked;
    if (ui->trace->value() > 0) // not the X (lowest) trace
    {
        int newTraceIndex = (ui->trace->value() - 1) % (ui->trace->maximum()+1);
        ScopeVis::MsgScopeVisMoveTrace *msgMoveTrace = ScopeVis::MsgScopeVisMoveTrace::create(ui->trace->value(), false);
        m_scopeVis->getInputMessageQueue()->push(msgMoveTrace);
        settingsTraceMove(ui->trace->value(), false);
        ui->trace->setValue(newTraceIndex); // follow trace
        GLScopeSettings::TraceData traceData;
        m_scopeVis->getTraceData(traceData, ui->trace->value());
        setTraceUI(traceData);
        ScopeVis::MsgScopeVisFocusOnTrace *msgFocusOnTrace = ScopeVis::MsgScopeVisFocusOnTrace::create(ui->trace->value());
        m_scopeVis->getInputMessageQueue()->push(msgFocusOnTrace);
    }
}

void GLScopeGUI::on_trig_valueChanged(int value)
{
    ui->trigText->setText(tr("%1").arg(value));

    m_ctlTriggerIndex = value;
    GLScopeSettings::TriggerData triggerData;
    m_scopeVis->getTriggerData(triggerData, value);

    qDebug() << "GLScopeGUI::on_trig_valueChanged:"
            << " m_projectionType: " << (int) triggerData.m_projectionType
            << " m_triggerRepeat" << triggerData.m_triggerRepeat
            << " m_triggerPositiveEdge" << triggerData.m_triggerPositiveEdge
            << " m_triggerBothEdges" << triggerData.m_triggerBothEdges
            << " m_triggerLevel" << triggerData.m_triggerLevel;

    setTriggerUI(triggerData);
    ScopeVis::MsgScopeVisFocusOnTrigger *msg = ScopeVis::MsgScopeVisFocusOnTrigger::create(value);
    m_scopeVis->getInputMessageQueue()->push(msg);
}

void GLScopeGUI::on_trigAdd_clicked(bool checked)
{
    (void) checked;
    GLScopeSettings::TriggerData triggerData;
    fillTriggerData(triggerData);
    addTrigger(triggerData);
}

void GLScopeGUI::on_trigDel_clicked(bool checked)
{
    (void) checked;
    if (ui->trig->value() > 0)
    {
        ScopeVis::MsgScopeVisRemoveTrigger *msg = ScopeVis::MsgScopeVisRemoveTrigger::create(ui->trig->value());
        m_scopeVis->getInputMessageQueue()->push(msg);
        settingsTriggerDel(ui->trig->value());
        ui->trig->setMaximum(ui->trig->maximum() - 1);
    }
}

void GLScopeGUI::on_trigUp_clicked(bool checked)
{
    (void) checked;
    if (ui->trig->maximum() > 0) // more than one trigger
    {
        int newTriggerIndex = (ui->trig->value() + 1) % (ui->trig->maximum()+1);
        ScopeVis::MsgScopeVisMoveTrigger *msgMoveTrigger = ScopeVis::MsgScopeVisMoveTrigger::create(ui->trace->value(), true);
        m_scopeVis->getInputMessageQueue()->push(msgMoveTrigger);
        settingsTriggerMove(ui->trace->value(), true);
        ui->trig->setValue(newTriggerIndex); // follow trigger
        GLScopeSettings::TriggerData triggerData;
        m_scopeVis->getTriggerData(triggerData, ui->trig->value());
        setTriggerUI(triggerData);
        ScopeVis::MsgScopeVisFocusOnTrigger *msgFocusOnTrigger = ScopeVis::MsgScopeVisFocusOnTrigger::create(ui->trig->value());
        m_scopeVis->getInputMessageQueue()->push(msgFocusOnTrigger);
    }
}

void GLScopeGUI::on_trigDown_clicked(bool checked)
{
    (void) checked;
    if (ui->trig->value() > 0) // not the 0 (lowest) trigger
    {
        int newTriggerIndex = (ui->trig->value() - 1) % (ui->trig->maximum()+1);
        ScopeVis::MsgScopeVisMoveTrigger *msgMoveTrigger = ScopeVis::MsgScopeVisMoveTrigger::create(ui->trace->value(), false);
        m_scopeVis->getInputMessageQueue()->push(msgMoveTrigger);
        settingsTriggerMove(ui->trace->value(), false);
        ui->trig->setValue(newTriggerIndex); // follow trigger
        GLScopeSettings::TriggerData triggerData;
        m_scopeVis->getTriggerData(triggerData, ui->trig->value());
        setTriggerUI(triggerData);
        ScopeVis::MsgScopeVisFocusOnTrigger *msgFocusOnTrigger = ScopeVis::MsgScopeVisFocusOnTrigger::create(ui->trig->value());
        m_scopeVis->getInputMessageQueue()->push(msgFocusOnTrigger);
    }
}

void GLScopeGUI::on_traceStream_currentIndexChanged(int index)
{
    (void) index;
    changeCurrentTrace();
}

void GLScopeGUI::on_traceMode_currentIndexChanged(int index)
{
    (void) index;
    setAmpScaleDisplay();
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ampReset_clicked(bool checked)
{
    (void) checked;
    ui->amp->setValue(0);
    ui->ampCoarse->setValue(1);
    ui->ampExp->setValue(0);
    setAmpScaleDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_amp_valueChanged(int value)
{
    (void) value;
    setAmpScaleDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ampCoarse_valueChanged(int value)
{
    (void) value;
    setAmpScaleDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ampExp_valueChanged(int value)
{
    (void) value;
    setAmpScaleDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ofsReset_clicked(bool checked)
{
    (void) checked;
    ui->ofsFine->setValue(0);
    ui->ofsCoarse->setValue(0);
    ui->ofsExp->setValue(0);
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ofsCoarse_valueChanged(int value)
{
    (void) value;
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ofsFine_valueChanged(int value)
{
    (void) value;
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_ofsExp_valueChanged(int value)
{
    (void) value;
    setAmpOfsDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_traceDelayCoarse_valueChanged(int value)
{
    (void) value;
    setTraceDelayDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_traceDelayFine_valueChanged(int value)
{
    (void) value;
    setTraceDelayDisplay();
    changeCurrentTrace();
}

void GLScopeGUI::on_traceView_toggled(bool checked)
{
    (void) checked;
    changeCurrentTrace();
}

void GLScopeGUI::on_traceColor_clicked()
{
    QColor newColor = QColorDialog::getColor(m_focusedTraceColor, this, tr("Select Color for trace"), QColorDialog::DontUseNativeDialog);

    if (newColor.isValid()) // user clicked OK and selected a color
    {
        m_focusedTraceColor = newColor;
        int r,g,b,a;
        m_focusedTraceColor.getRgb(&r, &g, &b, &a);
        ui->traceColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        changeCurrentTrace();
    }
}

void GLScopeGUI::on_memorySave_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Create trace memory file"), ".", tr("Trace memory files (*.trcm)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        QFileInfo fileInfo(fileName);

        if (fileInfo.suffix() != "trcm") {
            fileName += ".trcm";
        }

        QFile exportFile(fileName);

        if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QString base64Str = m_scopeVis->serializeMemory().toBase64();
            QTextStream outstream(&exportFile);
            outstream << base64Str;
            exportFile.close();
            qDebug("GLScopeGUI::on_memorySave_clicked: saved to %s", qPrintable(fileName));
        }
        else
        {
            QMessageBox::information(this, tr("Message"), tr("Cannot open %1 file for writing").arg(fileName));
        }
    }
}

void GLScopeGUI::on_memoryLoad_clicked(bool checked)
{
    (void) checked;
    qDebug("GLScopeGUI::on_memoryLoad_clicked");

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open trace memory file"), ".", tr("Trace memory files (*.trcm)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        QFile exportFile(fileName);

        if (exportFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray base64Str;
            QTextStream instream(&exportFile);
            instream >> base64Str;
            exportFile.close();
            m_scopeVis->deserializeMemory(QByteArray::fromBase64(base64Str));
            qDebug("GLScopeGUI::on_memoryLoad_clicked: loaded from %s", qPrintable(fileName));
        }
        else
        {
            QMessageBox::information(this, tr("Message"), tr("Cannot open file %1 for reading").arg(fileName));
        }
    }
}

void GLScopeGUI::on_mem_valueChanged(int value)
{
    QString text = QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
    ui->memText->setText(text);
   	disableLiveMode(value > 0); // live / memory mode toggle
   	m_scopeVis->setMemoryIndex(value);
}

void GLScopeGUI::on_trigStream_currentIndexChanged(int index)
{
    (void) index;
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigMode_currentIndexChanged(int index)
{
    (void) index;
    setTrigLevelDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigCount_valueChanged(int value)
{
    (void) value;
    setTrigCountDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigPos_toggled(bool checked)
{
    if (checked)
    {
        ui->trigNeg->setChecked(false);
        ui->trigBoth->setChecked(false);
    }
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigNeg_toggled(bool checked)
{
    if (checked)
    {
        ui->trigPos->setChecked(false);
        ui->trigBoth->setChecked(false);
    }
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigBoth_toggled(bool checked)
{
    if (checked)
    {
        ui->trigNeg->setChecked(false);
        ui->trigPos->setChecked(false);
    }
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigHoldoff_valueChanged(int value)
{
    ui->trigHoldoffText->setText(tr("%1").arg(value));
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigLevelCoarse_valueChanged(int value)
{
    (void) value;
    setTrigLevelDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigLevelFine_valueChanged(int value)
{
    (void) value;
    setTrigLevelDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigDelayCoarse_valueChanged(int value)
{
    (void) value;
    setTrigDelayDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigDelayFine_valueChanged(int value)
{
    (void) value;
    setTrigDelayDisplay();
    changeCurrentTrigger();
}

void GLScopeGUI::on_trigPre_valueChanged(int value)
{
    (void) value;
    setTrigPreDisplay();
    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );
    m_settings.m_trigPre = (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f));
}

void GLScopeGUI::on_trigColor_clicked()
{
    QColor newColor = QColorDialog::getColor(m_focusedTriggerColor, this, tr("Select Color for trigger line"), QColorDialog::DontUseNativeDialog);

    if (newColor.isValid()) // user clicked "OK"
    {
        m_focusedTriggerColor = newColor;
        int r,g,b,a;
        m_focusedTriggerColor.getRgb(&r, &g, &b, &a);
        ui->trigColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));
        changeCurrentTrigger();
    }
}

void GLScopeGUI::on_trigOneShot_toggled(bool checked)
{
    m_scopeVis->setOneShot(checked);
}

void GLScopeGUI::on_freerun_toggled(bool checked)
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

    m_scopeVis->configure(
        m_settings.m_traceLenMult*m_scopeVis->getTraceChunkSize(),
        m_timeBase,
        m_timeOffset*10,
        (uint32_t) (m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f)),
        ui->freerun->isChecked()
    );

    m_settings.m_freerun = checked;
}

void GLScopeGUI::setTraceIndexDisplay()
{
    ui->traceText->setText(ui->trace->value() == 0 ? "X" : QString("Y%1").arg(ui->trace->value()));
}

void GLScopeGUI::setTrigCountDisplay()
{
    QString text = QStringLiteral("%1").arg(ui->trigCount->value(), 2, 10, QLatin1Char('0'));
    ui->trigCountText->setText(text);
}

void GLScopeGUI::setTimeScaleDisplay()
{
    m_sampleRate = m_glScope->getSampleRate();
    unsigned int n_samples = (m_glScope->getTraceSize() * 1.0) / (double) m_timeBase;
    double t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate) / (double) m_timeBase;

    if (n_samples < 1000) {
        ui->timeText->setToolTip(tr("%1 S").arg(n_samples));
    } else if (n_samples < 1000000) {
        ui->timeText->setToolTip(tr("%1 kS").arg(n_samples/1000.0));
    } else {
        ui->timeText->setToolTip(tr("%1 MS").arg(n_samples/1000000.0));
    }

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

void GLScopeGUI::setTraceLenDisplay()
{
    unsigned int n_samples = m_settings.m_traceLenMult * m_scopeVis->getTraceChunkSize();

    if (n_samples < 1000) {
        ui->traceLenText->setToolTip(tr("%1 S").arg(n_samples));
    } else if (n_samples < 1000000) {
        ui->traceLenText->setToolTip(tr("%1 kS").arg(n_samples/1000.0));
    } else {
        ui->traceLenText->setToolTip(tr("%1 MS").arg(n_samples/1000000.0));
    }

    m_sampleRate = m_glScope->getSampleRate();
    double t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate);

    if(t < 0.000001)
        ui->traceLenText->setText(tr("%1\nns").arg(t * 1000000000.0, 0, 'f', 2));
    else if(t < 0.001)
        ui->traceLenText->setText(tr("%1\nµs").arg(t * 1000000.0, 0, 'f', 2));
    else if(t < 1.0)
        ui->traceLenText->setText(tr("%1\nms").arg(t * 1000.0, 0, 'f', 2));
    else
        ui->traceLenText->setText(tr("%1\ns").arg(t * 1.0, 0, 'f', 2));
}

void GLScopeGUI::setTimeOfsDisplay()
{
    unsigned int n_samples = m_glScope->getTraceSize() * (m_timeOffset/100.0);
    double dt = m_glScope->getTraceSize() * (m_timeOffset/100.0) / m_sampleRate;

    if (n_samples < 1000) {
        ui->timeOfsText->setToolTip(tr("%1 S").arg(n_samples));
    } else if (n_samples < 1000000) {
        ui->timeOfsText->setToolTip(tr("%1 kS").arg(n_samples/1000.0));
    } else {
        ui->timeOfsText->setToolTip(tr("%1 MS").arg(n_samples/1000000.0));
    }

    if(dt < 0.000001)
        ui->timeOfsText->setText(tr("%1\nns").arg(dt * 1000000000.0, 0, 'f', 2));
    else if(dt < 0.001)
        ui->timeOfsText->setText(tr("%1\nµs").arg(dt * 1000000.0, 0, 'f', 2));
    else if(dt < 1.0)
        ui->timeOfsText->setText(tr("%1\nms").arg(dt * 1000.0, 0, 'f', 2));
    else
        ui->timeOfsText->setText(tr("%1\ns").arg(dt * 1.0, 0, 'f', 2));
}

void GLScopeGUI::setAmpScaleDisplay()
{
    Projector::ProjectionType projectionType = (Projector::ProjectionType) ui->traceMode->currentIndex();
    double amp  = (ui->amp->value() / 1000.0) + ui->ampCoarse->value();
    int ampExp = ui->ampExp->value();
    double ampValue = amp * pow(10.0, ampExp);
    ui->ampText->setText(tr("%1").arg(amp, 0, 'f', 3));

    if (projectionType == Projector::ProjectionMagDB)
    {
        ui->ampExpText->setText(tr("e%1%2").arg(ampExp+2 < 0 ? "" : "+").arg(ampExp+2));
        ui->ampMultiplierText->setText("-");
    }
    else
    {
        ui->ampExpText->setText(tr("e%1%2").arg(ampExp < 0 ? "" : "+").arg(ampExp));
        double ampValue2 = 2.0 * ampValue;

        if (ampValue2 < 1e-9) {
            ui->ampMultiplierText->setText("p");
        } else if (ampValue2 < 1e-6) {
            ui->ampMultiplierText->setText("n");
        } else if (ampValue2 < 1e-3) {
            ui->ampMultiplierText->setText("µ");
        } else if (ampValue2 < 1e0) {
            ui->ampMultiplierText->setText("m");
        } else if (ampValue2 <= 1e3) {
            ui->ampMultiplierText->setText("-");
        } else if (ampValue2 <= 1e6) {
            ui->ampMultiplierText->setText("k");
        } else if (ampValue2 <= 1e9) {
            ui->ampMultiplierText->setText("M");
        } else {
            ui->ampMultiplierText->setText("G");
        }
    }
}

void GLScopeGUI::setAmpOfsDisplay()
{
    Projector::ProjectionType projectionType = (Projector::ProjectionType) ui->traceMode->currentIndex();
    double ofs = (ui->ofsFine->value() / 1000.0) + ui->ofsCoarse->value();
    int ofsExp = ui->ofsExp->value();
    ui->ofsText->setText(tr("%1").arg(ofs, 0, 'f', 3));

    if (projectionType == Projector::ProjectionMagDB) {
        ui->ofsExpText->setText(tr("e%1%2").arg(ofsExp+2 < 0 ? "" : "+").arg(ofsExp+2));
    } else {
        ui->ofsExpText->setText(tr("e%1%2").arg(ofsExp < 0 ? "" : "+").arg(ofsExp));
    }
}

void GLScopeGUI::setTraceDelayDisplay()
{
    if (m_sampleRate > 0)
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
}

void GLScopeGUI::setTrigIndexDisplay()
{
    ui->trigText->setText(tr("%1").arg(ui->trig->value()));
}

void GLScopeGUI::setTrigLevelDisplay()
{
    double t = (ui->trigLevelCoarse->value() / 100.0f) + (ui->trigLevelFine->value() / 50000.0f);
    Projector::ProjectionType projectionType = (Projector::ProjectionType) ui->trigMode->currentIndex();

    ui->trigLevelCoarse->setToolTip(QString("Trigger level coarse: %1 %").arg(ui->trigLevelCoarse->value() / 100.0f));
    ui->trigLevelFine->setToolTip(QString("Trigger level fine: %1 ppm").arg(ui->trigLevelFine->value() * 20));

    if (projectionType == Projector::ProjectionMagDB) {
        ui->trigLevelText->setText(tr("%1\ndB").arg(100.0 * (t - 1.0), 0, 'f', 1));
    }
    else
    {
        double a;

        if ((projectionType == Projector::ProjectionMagLin) || (projectionType == Projector::ProjectionMagSq)) {
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

void GLScopeGUI::setTrigDelayDisplay()
{
    if (m_sampleRate > 0)
    {
        double delayMult = ui->trigDelayCoarse->value() + ui->trigDelayFine->value() / (m_scopeVis->getTraceChunkSize() / 10.0);
        unsigned int n_samples_delay = m_settings.m_traceLenMult * m_scopeVis->getTraceChunkSize() * delayMult;

        if (n_samples_delay < 1000) {
            ui->trigDelayText->setToolTip(tr("%1 S").arg(n_samples_delay));
        } else if (n_samples_delay < 1000000) {
            ui->trigDelayText->setToolTip(tr("%1 kS").arg(n_samples_delay/1000.0));
        } else if (n_samples_delay < 1000000000) {
            ui->trigDelayText->setToolTip(tr("%1 MS").arg(n_samples_delay/1000000.0));
        } else {
            ui->trigDelayText->setToolTip(tr("%1 GS").arg(n_samples_delay/1000000000.0));
        }

        m_sampleRate = m_glScope->getSampleRate();
        double t = (n_samples_delay * 1.0f / m_sampleRate);

        if(t < 0.000001)
            ui->trigDelayText->setText(tr("%1\nns").arg(t * 1000000000.0, 0, 'f', 2));
        else if(t < 0.001)
            ui->trigDelayText->setText(tr("%1\nµs").arg(t * 1000000.0, 0, 'f', 2));
        else if(t < 1.0)
            ui->trigDelayText->setText(tr("%1\nms").arg(t * 1000.0, 0, 'f', 2));
        else
            ui->trigDelayText->setText(tr("%1\ns").arg(t * 1.0, 0, 'f', 2));
    }
}

void GLScopeGUI::setTrigPreDisplay()
{
    if (m_sampleRate > 0)
    {
        unsigned int n_samples_delay = m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f);
        double dt = m_glScope->getTraceSize() * (ui->trigPre->value()/100.0f) / m_sampleRate;

        if (n_samples_delay < 1000) {
            ui->trigPreText->setToolTip(tr("%1 S").arg(n_samples_delay));
        } else if (n_samples_delay < 1000000) {
            ui->trigPreText->setToolTip(tr("%1 kS").arg(n_samples_delay/1000.0));
        } else if (n_samples_delay < 1000000000) {
            ui->trigPreText->setToolTip(tr("%1 MS").arg(n_samples_delay/1000000.0));
        } else {
            ui->trigPreText->setToolTip(tr("%1 GS").arg(n_samples_delay/1000000000.0));
        }

        if(dt < 0.000001)
            ui->trigPreText->setText(tr("%1\nns").arg(dt * 1000000000.0f, 0, 'f', 2));
        else if(dt < 0.001)
            ui->trigPreText->setText(tr("%1\nµs").arg(dt * 1000000.0f, 0, 'f', 2));
        else if(dt < 1.0)
            ui->trigPreText->setText(tr("%1\nms").arg(dt * 1000.0f, 0, 'f', 2));
        else
            ui->trigPreText->setText(tr("%1\ns").arg(dt * 1.0f, 0, 'f', 2));
    }
}

void GLScopeGUI::changeCurrentTrace()
{
    GLScopeSettings::TraceData traceData;
    fillTraceData(traceData);
    uint32_t currentTraceIndex = ui->trace->value();
    ScopeVis::MsgScopeVisChangeTrace *msg = ScopeVis::MsgScopeVisChangeTrace::create(traceData, currentTraceIndex);
    m_scopeVis->getInputMessageQueue()->push(msg);

    if (currentTraceIndex < m_settings.m_tracesData.size()) {
        m_settings.m_tracesData[currentTraceIndex] = traceData;
    }
}

void GLScopeGUI::changeCurrentTrigger()
{
    GLScopeSettings::TriggerData triggerData;
    fillTriggerData(triggerData);
    uint32_t currentTriggerIndex = ui->trig->value();
    ScopeVis::MsgScopeVisChangeTrigger *msg = ScopeVis::MsgScopeVisChangeTrigger::create(triggerData, currentTriggerIndex);
    m_scopeVis->getInputMessageQueue()->push(msg);

    if (currentTriggerIndex < m_settings.m_triggersData.size()) {
        m_settings.m_triggersData[currentTriggerIndex] = triggerData;
    }
}

void GLScopeGUI::fillProjectionCombo(QComboBox* comboBox)
{
    comboBox->addItem("Real", Projector::ProjectionReal);
    comboBox->addItem("Imag", Projector::ProjectionImag);
    comboBox->addItem("Mag", Projector::ProjectionMagLin);
    comboBox->addItem("MagSq", Projector::ProjectionMagSq);
    comboBox->addItem("MagdB", Projector::ProjectionMagDB);
    comboBox->addItem("Phi", Projector::ProjectionPhase);
    comboBox->addItem("DOAP", Projector::ProjectionDOAP);
    comboBox->addItem("DOAN", Projector::ProjectionDOAN);
    comboBox->addItem("dPhi", Projector::ProjectionDPhase);
    comboBox->addItem("BPSK", Projector::ProjectionBPSK);
    comboBox->addItem("QPSK", Projector::ProjectionQPSK);
    comboBox->addItem("8PSK", Projector::Projection8PSK);
    comboBox->addItem("16PSK", Projector::Projection16PSK);
}

void GLScopeGUI::disableLiveMode(bool disable)
{
    ui->traceLen->setEnabled(!disable);
    ui->trig->setEnabled(!disable);
    ui->trigAdd->setEnabled(!disable);
    ui->trigDel->setEnabled(!disable);
    ui->trigMode->setEnabled(!disable);
    ui->trigCount->setEnabled(!disable);
    ui->trigPos->setEnabled(!disable);
    ui->trigNeg->setEnabled(!disable);
    ui->trigBoth->setEnabled(!disable);
    ui->trigLevelCoarse->setEnabled(!disable);
    ui->trigLevelFine->setEnabled(!disable);
    ui->trigDelayCoarse->setEnabled(!disable);
    ui->trigDelayFine->setEnabled(!disable);
    ui->trigPre->setEnabled(!disable);
    ui->trigOneShot->setEnabled(!disable);
    ui->freerun->setEnabled(!disable);
    ui->memorySave->setEnabled(disable);
    ui->memoryLoad->setEnabled(disable);
}

void GLScopeGUI::fillTraceData(GLScopeSettings::TraceData& traceData)
{
    traceData.m_streamIndex = ui->traceStream->currentIndex();
    traceData.m_projectionType = (Projector::ProjectionType) ui->traceMode->currentIndex();
    traceData.m_hasTextOverlay = (traceData.m_projectionType == Projector::ProjectionMagDB)
        || (traceData.m_projectionType == Projector::ProjectionMagSq);
    traceData.m_textOverlay.clear();

    double ampValue = ((ui->amp->value() / 1000.0) + ui->ampCoarse->value()) * pow(10.0, ui->ampExp->value());
    traceData.m_amp = 1.0 / ampValue;

    double ofsValue = ((ui->ofsFine->value() / 1000.0) + ui->ofsCoarse->value()) * pow(10.0, ui->ofsExp->value());
    traceData.m_ofs = ofsValue;

    traceData.m_traceDelayCoarse = ui->traceDelayCoarse->value();
    traceData.m_traceDelayFine = ui->traceDelayFine->value();
    traceData.m_traceDelay = traceData.m_traceDelayCoarse * 100 + traceData.m_traceDelayFine;
    traceData.setColor(m_focusedTraceColor);
    traceData.m_viewTrace = ui->traceView->isChecked();
}

void GLScopeGUI::fillTriggerData(GLScopeSettings::TriggerData& triggerData)
{
    triggerData.m_streamIndex = ui->trigStream->currentIndex();
    triggerData.m_projectionType = (Projector::ProjectionType) ui->trigMode->currentIndex();
    triggerData.m_inputIndex = 0;
    triggerData.m_triggerLevel = (ui->trigLevelCoarse->value() / 100.0) + (ui->trigLevelFine->value() / 50000.0);
    triggerData.m_triggerLevelCoarse = ui->trigLevelCoarse->value();
    triggerData.m_triggerLevelFine = ui->trigLevelFine->value();
    triggerData.m_triggerPositiveEdge = ui->trigPos->isChecked();
    triggerData.m_triggerBothEdges = ui->trigBoth->isChecked();
    triggerData.m_triggerHoldoff = ui->trigHoldoff->value();
    triggerData.m_triggerRepeat = ui->trigCount->value();
    triggerData.m_triggerDelayMult = ui->trigDelayCoarse->value() + ui->trigDelayFine->value() / (m_scopeVis->getTraceChunkSize() / 10.0);
    triggerData.m_triggerDelay = (int) (m_settings.m_traceLenMult * m_scopeVis->getTraceChunkSize() * triggerData.m_triggerDelayMult);
    triggerData.m_triggerDelayCoarse = ui->trigDelayCoarse->value();
    triggerData.m_triggerDelayFine = ui->trigDelayFine->value();
    triggerData.setColor(m_focusedTriggerColor);
}

void GLScopeGUI::setTraceUI(const GLScopeSettings::TraceData& traceData)
{
    TraceUIBlocker traceUIBlocker(ui);

    ui->traceStream->setCurrentIndex(traceData.m_streamIndex);
    ui->traceMode->setCurrentIndex((int) traceData.m_projectionType);

    double ampValue = 1.0 / traceData.m_amp;
    int ampExp;
    double ampMant = CalcDb::frexp10(ampValue, &ampExp) * 10.0;
    int ampCoarse = (int) ampMant;
    int ampFine = round((ampMant - ampCoarse) * 1000.0);
    ampExp -= 1;
    ui->amp->setValue(ampFine);
    ui->ampCoarse->setValue(ampCoarse);
    ui->ampExp->setValue(ampExp);
    setAmpScaleDisplay();

    double ofsValue = traceData.m_ofs;
    int ofsExp;
    double ofsMant = CalcDb::frexp10(ofsValue, &ofsExp) * 10.0;
    int ofsCoarse = (int) ofsMant;
    int ofsFine = round((ofsMant - ofsCoarse) * 1000.0);
    ofsExp -= ofsMant == 0 ? 0 : 1;
    ui->ofsFine->setValue(ofsFine);
    ui->ofsCoarse->setValue(ofsCoarse);
    ui->ofsExp->setValue(ofsExp);
    setAmpOfsDisplay();

    ui->traceDelayCoarse->setValue(traceData.m_traceDelayCoarse);
    ui->traceDelayFine->setValue(traceData.m_traceDelayFine);
    setTraceDelayDisplay();

    m_focusedTraceColor = traceData.m_traceColor;
    int r, g, b, a;
    m_focusedTraceColor.getRgb(&r, &g, &b, &a);
    ui->traceColor->setStyleSheet(tr("QLabel { background-color : rgb(%1,%2,%3); }").arg(r).arg(g).arg(b));

    ui->traceView->setChecked(traceData.m_viewTrace);
}

void GLScopeGUI::setTriggerUI(const GLScopeSettings::TriggerData& triggerData)
{
    TrigUIBlocker trigUIBlocker(ui);

    ui->trigStream->setCurrentIndex(triggerData.m_streamIndex);
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

    ui->trigHoldoffText->setText(tr("%1").arg(triggerData.m_triggerHoldoff));

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

void GLScopeGUI::applySettings(const GLScopeSettings& settings, bool force)
{
    if (m_scopeVis)
    {
        ScopeVis::MsgConfigureScopeVis *msg = ScopeVis::MsgConfigureScopeVis::create(settings, force);
        m_scopeVis->getInputMessageQueue()->push(msg);
    }
}

void GLScopeGUI::displaySettings()
{
    MainUIBlocker mainUIBlocker(ui);

    ui->traceText->setText(m_ctlTraceIndex == 0 ? "X" : QString("Y%1").arg(m_ctlTraceIndex));
    ui->trace->setValue(m_ctlTraceIndex);
    const GLScopeSettings::TraceData& traceData = m_settings.m_tracesData[m_ctlTraceIndex];
    setTraceUI(traceData);
    ui->trigText->setText(tr("%1").arg(m_ctlTriggerIndex));
    ui->trig->setValue(m_ctlTriggerIndex);
    const GLScopeSettings::TriggerData& triggerData = m_settings.m_triggersData[m_ctlTriggerIndex];
    setTriggerUI(triggerData);
    setDisplayMode(m_settings.m_displayMode);
    ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_settings.m_traceIntensity));
    ui->traceIntensity->setValue(m_settings.m_traceIntensity);
    m_glScope->setDisplayTraceIntensity(m_settings.m_traceIntensity);
    ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_settings.m_gridIntensity));
    ui->gridIntensity->setValue(m_settings.m_gridIntensity);
    m_glScope->setDisplayGridIntensity(m_settings.m_gridIntensity);
    setTimeScaleDisplay();
    ui->timeOfs->setValue(m_settings.m_timeOfs);
    setTimeOfsDisplay();
    ui->traceLen->setValue(m_settings.m_traceLenMult);
    setPreTrigger(100.0f * m_settings.m_trigPre / m_glScope->getTraceSize());
    ui->freerun->setChecked(m_settings.m_freerun);
    changeCurrentTrigger(); // Ensure consistency with GUI
}

bool GLScopeGUI::handleMessage(Message* message)
{
    (void) message;
    return false;
}

GLScopeGUI::TrigUIBlocker::TrigUIBlocker(Ui::GLScopeGUI *ui) :
        m_ui(ui)
{
    m_oldStateTrigStream      = ui->trigStream->blockSignals(true);
    m_oldStateTrigMode        = ui->trigMode->blockSignals(true);
    m_oldStateTrigCount       = ui->trigCount->blockSignals(true);
    m_oldStateTrigPos         = ui->trigPos->blockSignals(true);
    m_oldStateTrigNeg         = ui->trigNeg->blockSignals(true);
    m_oldStateTrigBoth        = ui->trigBoth->blockSignals(true);
    m_oldStateTrigLevelCoarse = ui->trigLevelCoarse->blockSignals(true);
    m_oldStateTrigLevelFine   = ui->trigLevelFine->blockSignals(true);
    m_oldStateTrigDelayCoarse = ui->trigDelayCoarse->blockSignals(true);
    m_oldStateTrigDelayFine   = ui->trigDelayFine->blockSignals(true);
    m_oldStateTrigColor       = ui->trigColor->blockSignals(true);
}

GLScopeGUI::TrigUIBlocker::~TrigUIBlocker()
{
    unBlock();
}

void GLScopeGUI::TrigUIBlocker::unBlock()
{
    m_ui->trigStream->blockSignals(m_oldStateTrigStream);
    m_ui->trigMode->blockSignals(m_oldStateTrigMode);
    m_ui->trigCount->blockSignals(m_oldStateTrigCount);
    m_ui->trigPos->blockSignals(m_oldStateTrigPos);
    m_ui->trigNeg->blockSignals(m_oldStateTrigNeg);
    m_ui->trigBoth->blockSignals(m_oldStateTrigBoth);
    m_ui->trigLevelCoarse->blockSignals(m_oldStateTrigLevelCoarse);
    m_ui->trigLevelFine->blockSignals(m_oldStateTrigLevelFine);
    m_ui->trigDelayCoarse->blockSignals(m_oldStateTrigDelayCoarse);
    m_ui->trigDelayFine->blockSignals(m_oldStateTrigDelayFine);
    m_ui->trigColor->blockSignals(m_oldStateTrigColor);
}

GLScopeGUI::TraceUIBlocker::TraceUIBlocker(Ui::GLScopeGUI* ui) :
        m_ui(ui)
{
    m_oldStateTraceStream      = m_ui->traceStream->blockSignals(true);
    m_oldStateTraceMode        = m_ui->traceMode->blockSignals(true);
    m_oldStateAmp              = m_ui->amp->blockSignals(true);
    m_oldStateAmpCoarse        = m_ui->ampCoarse->blockSignals(true);
    m_oldStateAmpExp           = m_ui->ampExp->blockSignals(true);
    m_oldStateOfsCoarse        = m_ui->ofsCoarse->blockSignals(true);
    m_oldStateOfsFine          = m_ui->ofsFine->blockSignals(true);
    m_oldStateOfsExp           = m_ui->ofsExp->blockSignals(true);
    m_oldStateTraceDelayCoarse = m_ui->traceDelayCoarse->blockSignals(true);
    m_oldStateTraceDelayFine   = m_ui->traceDelayFine->blockSignals(true);
    m_oldStateTraceColor       = m_ui->traceColor->blockSignals(true);
    m_oldStateTraceView        = m_ui->traceView->blockSignals(true);
    m_oldStateTrace            = m_ui->trace->blockSignals(true);
    m_oldStateTraceAdd         = m_ui->traceAdd->blockSignals(true);
    m_oldStateTraceDel         = m_ui->traceDel->blockSignals(true);
}

GLScopeGUI::TraceUIBlocker::~TraceUIBlocker()
{
    unBlock();
}

void GLScopeGUI::TraceUIBlocker::unBlock()
{
    m_ui->traceStream->blockSignals(m_oldStateTraceStream);
    m_ui->trace->blockSignals(m_oldStateTrace);
    m_ui->traceAdd->blockSignals(m_oldStateTraceAdd);
    m_ui->traceDel->blockSignals(m_oldStateTraceDel);
    m_ui->traceMode->blockSignals(m_oldStateTraceMode);
    m_ui->amp->blockSignals(m_oldStateAmp);
    m_ui->ampCoarse->blockSignals(m_oldStateAmpCoarse);
    m_ui->ampExp->blockSignals(m_oldStateAmpExp);
    m_ui->ofsCoarse->blockSignals(m_oldStateOfsCoarse);
    m_ui->ofsFine->blockSignals(m_oldStateOfsFine);
    m_ui->ofsExp->blockSignals(m_oldStateOfsExp);
    m_ui->traceDelayCoarse->blockSignals(m_oldStateTraceDelayCoarse);
    m_ui->traceDelayFine->blockSignals(m_oldStateTraceDelayFine);
    m_ui->traceColor->blockSignals(m_oldStateTraceColor);
    m_ui->traceView->blockSignals(m_oldStateTraceView);
}

GLScopeGUI::MainUIBlocker::MainUIBlocker(Ui::GLScopeGUI* ui) :
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

GLScopeGUI::MainUIBlocker::~MainUIBlocker()
{
    unBlock();
}

void GLScopeGUI::MainUIBlocker::unBlock()
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

void GLScopeGUI::setDisplayMode(GLScopeSettings::DisplayMode displayMode)
{
    uint32_t nbTraces = m_scopeVis->getNbTraces();

    ui->onlyX->setChecked(false);
    ui->onlyY->setChecked(false);
    ui->horizontalXY->setChecked(false);
    ui->verticalXY->setChecked(false);
    ui->polar->setChecked(false);

    ui->onlyX->setEnabled(true);
    ui->onlyY->setEnabled(nbTraces > 1);
    ui->horizontalXY->setEnabled(nbTraces > 1);
    ui->verticalXY->setEnabled(nbTraces > 1);
    ui->polar->setEnabled(nbTraces > 1);

    if (ui->trace->maximum() == 1)
    {
        ui->onlyX->setChecked(true);
        ui->onlyX->setEnabled(false);
    }
    else
    {
        switch (displayMode)
        {
        case GLScopeSettings::DisplayX:
            ui->onlyX->setChecked(true);
            ui->onlyX->setEnabled(false);
            break;
        case GLScopeSettings::DisplayY:
            ui->onlyY->setChecked(true);
            ui->onlyY->setEnabled(false);
            break;
        case GLScopeSettings::DisplayXYH:
            ui->horizontalXY->setChecked(true);
            ui->horizontalXY->setEnabled(false);
            break;
        case GLScopeSettings::DisplayXYV:
            ui->verticalXY->setChecked(true);
            ui->verticalXY->setEnabled(false);
            break;
        case GLScopeSettings::DisplayPol:
            ui->polar->setChecked(true);
            ui->polar->setEnabled(false);
            break;
        default:
            ui->onlyX->setChecked(true);
            ui->onlyX->setEnabled(false);
            break;
        }
    }

    m_settings.m_displayMode = displayMode;
    m_scopeVis->configure(
        m_settings.m_displayMode,
        m_settings.m_traceIntensity,
        m_settings.m_gridIntensity
    );
}

void GLScopeGUI::setTraceIntensity(int value)
{
    if ((value < ui->traceIntensity->minimum()) || (value > ui->traceIntensity->maximum())) {
        return;
    }

    ui->traceIntensity->setValue(value);
}

void GLScopeGUI::setGridIntensity(int value)
{
    if ((value < ui->gridIntensity->minimum()) || (value > ui->gridIntensity->maximum())) {
        return;
    }

    ui->gridIntensity->setValue(value);
}

void GLScopeGUI::setTimeBase(int step)
{
    if ((step < ui->time->minimum()) || (step > ui->time->maximum())) {
        return;
    }

    ui->time->setValue(step);
}

void GLScopeGUI::setTimeOffset(int step)
{
    if ((step < ui->timeOfs->minimum()) || (step > ui->timeOfs->maximum())) {
        return;
    }

    ui->timeOfs->setValue(step);
}

void GLScopeGUI::setTraceLength(int step)
{
    if ((step < ui->traceLen->minimum()) || (step > ui->traceLen->maximum())) {
        return;
    }

    ui->traceLen->setValue(step);
}

void GLScopeGUI::setPreTrigger(int step)
{
    if ((step < ui->trigPre->minimum()) || (step > ui->trigPre->maximum())) {
        return;
    }

    ui->trigPre->setValue(step);
}

void GLScopeGUI::changeTrace(int traceIndex, const GLScopeSettings::TraceData& traceData)
{
    ScopeVis::MsgScopeVisChangeTrace *msg = ScopeVis::MsgScopeVisChangeTrace::create(traceData, traceIndex);
    m_scopeVis->getInputMessageQueue()->push(msg);
    settingsTraceChange(traceData, traceIndex);
}

void GLScopeGUI::addTrace(const GLScopeSettings::TraceData& traceData)
{
    if (ui->trace->maximum() < 7) // Limit number of channels to 8. Is it necessary?
    {
        if (ui->trace->value() == 0)
        {
            ui->onlyY->setEnabled(true);
            ui->horizontalXY->setEnabled(true);
            ui->verticalXY->setEnabled(true);
            ui->polar->setEnabled(true);
        }

        ScopeVis::MsgScopeVisAddTrace *msg = ScopeVis::MsgScopeVisAddTrace::create(traceData);
        m_scopeVis->getInputMessageQueue()->push(msg);
        settingsTraceAdd(traceData);
        ui->trace->setMaximum(ui->trace->maximum() + 1);
    }
}

void GLScopeGUI::focusOnTrace(int traceIndex)
{
    on_trace_valueChanged(traceIndex);
}

void GLScopeGUI::changeTrigger(int triggerIndex, const GLScopeSettings::TriggerData& triggerData)
{
    ScopeVis::MsgScopeVisChangeTrigger *msg = ScopeVis::MsgScopeVisChangeTrigger::create(triggerData, triggerIndex);
    m_scopeVis->getInputMessageQueue()->push(msg);
    settingsTriggerChange(triggerData, triggerIndex);
}

void GLScopeGUI::addTrigger(const GLScopeSettings::TriggerData& triggerData)
{
    if (ui->trig->maximum() < 9)
    {
        ScopeVis::MsgScopeVisAddTrigger *msg = ScopeVis::MsgScopeVisAddTrigger::create(triggerData);
        m_scopeVis->getInputMessageQueue()->push(msg);
        settingsTriggerAdd(triggerData);
        ui->trig->setMaximum(ui->trig->maximum() + 1);
    }
}

void GLScopeGUI::focusOnTrigger(int triggerIndex)
{
    on_trig_valueChanged(triggerIndex);
}

void GLScopeGUI::traceLengthChange()
{
    on_traceLen_valueChanged(m_settings.m_traceLenMult);
}

void GLScopeGUI::settingsTraceAdd(const GLScopeSettings::TraceData& traceData)
{
    m_settings.m_tracesData.push_back(traceData);
}

void GLScopeGUI::settingsTraceChange(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
{
    m_settings.m_tracesData[traceIndex] = traceData;
}

void GLScopeGUI::settingsTraceDel(uint32_t traceIndex)
{
    unsigned int iDest = 0;

    for (unsigned int iSource = 0; iSource < m_settings.m_tracesData.size(); iSource++)
    {
        if (iSource != traceIndex) {
            m_settings.m_tracesData[iDest++] = m_settings.m_tracesData[iSource];
        }
    }

    if (m_settings.m_tracesData.size() != 0) {
        m_settings.m_tracesData.pop_back();
    }
}

void GLScopeGUI::settingsTraceMove(uint32_t traceIndex, bool upElseDown)
{
    int nextTraceIndex = (traceIndex + (upElseDown ? 1 : -1)) % m_settings.m_tracesData.size();
    GLScopeSettings::TraceData nextTraceData = m_settings.m_tracesData[nextTraceIndex];
    m_settings.m_tracesData[nextTraceIndex] = m_settings.m_tracesData[traceIndex];
    m_settings.m_tracesData[traceIndex] = nextTraceData;
}

void GLScopeGUI::settingsTriggerAdd(const GLScopeSettings::TriggerData& triggerData)
{
    m_settings.m_triggersData.push_back(triggerData);
}

void GLScopeGUI::settingsTriggerChange(const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex)
{
    m_settings.m_triggersData[triggerIndex] = triggerData;
}

void GLScopeGUI::settingsTriggerDel(uint32_t triggerIndex)
{
    unsigned int iDest = 0;

    for (unsigned int iSource = 0; iSource < m_settings.m_triggersData.size(); iSource++)
    {
        if (iSource != triggerIndex) {
            m_settings.m_triggersData[iDest++] = m_settings.m_triggersData[iSource];
        }
    }

    if (m_settings.m_triggersData.size() != 0) {
        m_settings.m_triggersData.pop_back();
    }
}

void GLScopeGUI::settingsTriggerMove(uint32_t triggerIndex, bool upElseDown)
{
    int nextTriggerIndex = (triggerIndex + (upElseDown ? 1 : -1)) % m_settings.m_triggersData.size();
    GLScopeSettings::TriggerData nextTriggerData = m_settings.m_triggersData[nextTriggerIndex];
    m_settings.m_triggersData[nextTriggerIndex] = m_settings.m_triggersData[triggerIndex];
    m_settings.m_triggersData[triggerIndex] = nextTriggerData;
}
