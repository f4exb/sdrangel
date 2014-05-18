///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include "gui/scopewindow.h"
#include "ui_scopewindow.h"
#include "util/simpleserializer.h"

ScopeWindow::ScopeWindow(DSPEngine* dspEngine, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::ScopeWindow),
	m_sampleRate(0),
	m_timeBase(1)
{
	ui->setupUi(this);
	ui->scope->setDSPEngine(dspEngine);
}

ScopeWindow::~ScopeWindow()
{
	delete ui;
}

void ScopeWindow::setSampleRate(int sampleRate)
{
	m_sampleRate = sampleRate;
	on_scope_traceSizeChanged(0);
}

void ScopeWindow::resetToDefaults()
{
	m_displayData = GLScope::ModeIQ;
	m_displayOrientation = Qt::Horizontal;
	m_timeBase = 1;
	m_timeOffset = 0;
	m_amplification = 0;
	applySettings();
}

QByteArray ScopeWindow::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_displayData);
	s.writeS32(2, m_displayOrientation);
	s.writeS32(3, m_timeBase);
	s.writeS32(4, m_timeOffset);
	s.writeS32(5, m_amplification);
	return s.final();
}

bool ScopeWindow::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readS32(1, &m_displayData, GLScope::ModeIQ);
		d.readS32(2, &m_displayOrientation, Qt::Horizontal);
		d.readS32(3, &m_timeBase, 1);
		d.readS32(4, &m_timeOffset, 0);
		d.readS32(5, &m_amplification, 0);
		if(m_timeBase < 0)
			m_timeBase = 1;
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void ScopeWindow::on_amp_valueChanged(int value)
{
	static qreal amps[11] = { 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002, 0.0001 };
	ui->ampText->setText(tr("%1\n/div").arg(amps[value], 0, 'f', 4));
	ui->scope->setAmp(0.2 / amps[value]);
	m_amplification = value;
}

void ScopeWindow::on_scope_traceSizeChanged(int)
{
	qreal t = (ui->scope->getTraceSize() * 0.1 / m_sampleRate) / (qreal)m_timeBase;
	if(t < 0.000001)
		ui->timeText->setText(tr("%1\nns/div").arg(t * 1000000000.0));
	else if(t < 0.001)
		ui->timeText->setText(tr("%1\nµs/div").arg(t * 1000000.0));
	else if(t < 1.0)
		ui->timeText->setText(tr("%1\nms/div").arg(t * 1000.0));
	else ui->timeText->setText(tr("%1\ns/div").arg(t * 1.0));
}

void ScopeWindow::on_time_valueChanged(int value)
{
	m_timeBase = value;
	on_scope_traceSizeChanged(0);
	ui->scope->setTimeBase(m_timeBase);
}

void ScopeWindow::on_timeOfs_valueChanged(int value)
{
	m_timeOffset = value;
	ui->scope->setTimeOfsProMill(value);
}

void ScopeWindow::on_displayMode_currentIndexChanged(int index)
{
	m_displayData = index;
	switch(index) {
		case 0: // i+q
			ui->scope->setMode(GLScope::ModeIQ);
			break;
		case 1: // mag(lin)+pha
			ui->scope->setMode(GLScope::ModeMagLinPha);
			break;
		case 2: // mag(dB)+pha
			ui->scope->setMode(GLScope::ModeMagdBPha);
			break;
		case 3: // derived1+derived2
			ui->scope->setMode(GLScope::ModeDerived12);
			break;
		case 4: // clostationary
			ui->scope->setMode(GLScope::ModeCyclostationary);
			break;

		default:
			break;
	}
}

void ScopeWindow::on_horizView_clicked()
{
	m_displayOrientation = Qt::Horizontal;
	if(ui->horizView->isChecked()) {
		ui->vertView->setChecked(false);
		ui->scope->setOrientation(Qt::Horizontal);
	} else {
		ui->horizView->setChecked(true);
	}
}

void ScopeWindow::on_vertView_clicked()
{
	m_displayOrientation = Qt::Vertical;
	if(ui->vertView->isChecked()) {
		ui->horizView->setChecked(false);
		ui->scope->setOrientation(Qt::Vertical);
	} else {
		ui->vertView->setChecked(true);
		ui->scope->setOrientation(Qt::Vertical);
	}
}

void ScopeWindow::applySettings()
{
	ui->displayMode->setCurrentIndex(m_displayData);
	if(m_displayOrientation == Qt::Horizontal) {
		ui->scope->setOrientation(Qt::Horizontal);
		ui->horizView->setChecked(true);
		ui->vertView->setChecked(false);
	} else {
		ui->scope->setOrientation(Qt::Vertical);
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(true);
	}
	ui->time->setValue(m_timeBase);
	ui->timeOfs->setValue(m_timeOffset);
	ui->amp->setValue(m_amplification);
}
