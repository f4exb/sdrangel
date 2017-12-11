///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_CWKEYER_H_
#define SDRBASE_DSP_CWKEYER_H_

#include <QObject>
#include <QMutex>

#include "util/export.h"
#include "util/message.h"
#include "cwkeyersettings.h"

/**
 * Ancillary class to smooth out CW transitions with a sine shape
 */
class CWSmoother
{
public:
    CWSmoother();
    ~CWSmoother();

    void setNbFadeSamples(unsigned int nbFadeSamples);
    bool getFadeSample(bool on, float& sample);

private:
    QMutex m_mutex;
    unsigned int m_fadeInCounter;
    unsigned int m_fadeOutCounter;
    unsigned int m_nbFadeSamples;
    float *m_fadeInSamples;
    float *m_fadeOutSamples;
};

class SDRANGEL_API CWKeyer : public QObject {
    Q_OBJECT

public:
    class MsgConfigureCWKeyer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const CWKeyerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureCWKeyer* create(const CWKeyerSettings& settings, bool force)
        {
            return new MsgConfigureCWKeyer(settings, force);
        }

    private:
        CWKeyerSettings m_settings;
        bool m_force;

        MsgConfigureCWKeyer(const CWKeyerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    typedef enum
    {
        KeySilent,
        KeyDot,
        KeyDash
    } CWKeyIambicState;

    typedef enum
    {
    	TextStart,
		TextStartChar,
		TextStartElement,
		TextElement,
		TextCharSpace,
		TextWordSpace,
		TextEnd,
		TextStop
    } CWTextState;

    CWKeyer();
    ~CWKeyer();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void setSampleRate(int sampleRate);
    void setWPM(int wpm);
    void setText(const QString& text);
    void setMode(CWKeyerSettings::CWMode mode);
    void setLoop(bool loop) { m_settings.m_loop = loop; }
    const CWKeyerSettings& getSettings() const { return m_settings; }

    void reset() { m_keyIambicState = KeySilent; }

    CWSmoother& getCWSmoother() { return m_cwSmoother; }
    int getSample();
    bool eom();
    void resetText() { m_textState = TextStart; }
    void stopText() { m_textState = TextStop; }

private:
    QMutex m_mutex;
    CWKeyerSettings m_settings;
    int m_dotLength;   //!< dot length in samples
    int m_textPointer;
    int m_elementPointer;
    int m_samplePointer;
    bool m_elementSpace;
    bool m_characterSpace;
    bool m_key;
    bool m_dot;
    bool m_dash;
    bool m_elementOn;
    char m_asciiChar;
    CWKeyIambicState m_keyIambicState;
    CWTextState m_textState;
    CWSmoother m_cwSmoother;

    static const signed char m_asciiToMorse[128][7];

    void nextStateIambic();
    void nextStateText();
};

#endif /* SDRBASE_DSP_CWKEYER_H_ */
