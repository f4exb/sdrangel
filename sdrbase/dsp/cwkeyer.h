///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_CWKEYER_H_
#define SDRBASE_DSP_CWKEYER_H_

#include <QObject>
#include <QMutex>

#include "export.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "cwkeyersettings.h"
#include "SWGChannelSettings.h"

/**
 * Ancillary class to smooth out CW transitions with a sine shape
 */
class SDRBASE_API CWSmoother
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

class SDRBASE_API CWKeyer : public QObject {
    Q_OBJECT

public:
    class SDRBASE_API MsgConfigureCWKeyer : public Message {
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

    enum CWKeyIambicState
    {
        KeySilent,
        KeyDot,
        KeyDash
    };

    enum CWTextState
    {
    	TextStart,
		TextStartChar,
		TextStartElement,
		TextElement,
		TextCharSpace,
		TextWordSpace,
		TextEnd,
		TextStop
    };

    CWKeyer();
    ~CWKeyer();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

    void setSampleRate(int sampleRate);
    const CWKeyerSettings& getSettings() const { return m_settings; }

    void reset() { m_keyIambicState = KeySilent; }

    CWSmoother& getCWSmoother() { return m_cwSmoother; }
    int getSample();
    bool eom();
    void resetText() { m_textState = TextStart; }
    void stopText() { m_textState = TextStop; }
    void setKeyboardDots();
    void setKeyboardDashes();
    void setKeyboardSilence();

    static void webapiSettingsPutPatch(
        const QStringList& channelSettingsKeys,
        CWKeyerSettings& cwKeyerSettings,
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings
    );

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings,
        const CWKeyerSettings& cwKeyerSettings
    );

private:
    QMutex m_mutex;
    CWKeyerSettings m_settings;
    MessageQueue m_inputMessageQueue;
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
    signed char m_asciiChar;
    CWKeyIambicState m_keyIambicState;
    CWTextState m_textState;
    CWSmoother m_cwSmoother;

    static const signed char m_asciiToMorse[128][7];

    void applySettings(const CWKeyerSettings& settings, bool force = false);
    bool handleMessage(const Message& cmd);
    void nextStateIambic();
    void nextStateText();

private slots:
    void handleInputMessages();
};

#endif /* SDRBASE_DSP_CWKEYER_H_ */
