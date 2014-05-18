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

#ifndef INCLUDE_SAMPLESOURCE_H
#define INCLUDE_SAMPLESOURCE_H

#include <QtGlobal>
#include "dsp/samplefifo.h"
#include "util/message.h"
#include "util/export.h"

class PluginGUI;
class MessageQueue;

class SDRANGELOVE_API SampleSource {
public:
	struct SDRANGELOVE_API GeneralSettings {
		quint64 m_centerFrequency;

		GeneralSettings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	SampleSource(MessageQueue* guiMessageQueue);
	virtual ~SampleSource();

	virtual bool startInput(int device) = 0;
	virtual void stopInput() = 0;

	virtual const QString& getDeviceDescription() const = 0;
	virtual int getSampleRate() const = 0;
	virtual quint64 getCenterFrequency() const = 0;

	virtual bool handleMessage(Message* message) = 0;

	SampleFifo* getSampleFifo() { return &m_sampleFifo; }

protected:
	SampleFifo m_sampleFifo;
	MessageQueue* m_guiMessageQueue;

	GeneralSettings m_generalSettings;
};

#endif // INCLUDE_SAMPLESOURCE_H
