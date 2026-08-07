///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_PACKETDEMODSETTINGS_H
#define INCLUDE_PACKETDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define PACKETDEMOD_COLUMNS 9

struct PacketDemodSettings
{
    enum Mode {
        ModeAFSK1200
    };

    qint32 m_inputFrequencyOffset;
    Mode m_mode;
    Real m_rfBandwidth;
    int m_chase;        //!< Chase decoding depth: on CRC failure retry inverting the n least
                        //!< confident symbols. 0 disables
    bool m_mlse;        //!< Detect on the complex baseband with AfskMlse instead of running
                        //!< tone correlators on the discriminator output. Worth about 9 dB,
                        //!< because the discriminator has an FM threshold and this does not.
                        //!< On by default - the burst estimator measures the transmitter's
                        //!< real symbol rate, so the fixed timing grid that used to make this
                        //!< worse than the discriminator beyond a few hundred ppm is gone
    QString m_filterFrom;
    QString m_filterTo;
    QString m_filterPID;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    bool m_useFileTime;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    QString m_logFilename;
    bool m_logEnabled;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_columnIndexes[PACKETDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[PACKETDEMOD_COLUMNS];  //!< Size of the columns in the table

    static const int PACKETDEMOD_CHANNEL_SAMPLE_RATE  = 38400; // Must be integer multiple of baud rate (x32, x4)

    // Spacing of the MLSE carrier hypotheses. Measured against a swept carrier, 300 Hz steps
    // hold 97-100% of packets everywhere from 0 to 700 Hz of offset and match a 200 Hz grid
    // for two fewer hypotheses; the per survivor loop takes out the residual.
    static const int PACKETDEMOD_MLSE_FREQ_STEP = 300;

    // Assumed peak deviation, in Hz. Measured 2500 on real APRS, and neither path is
    // sensitive to a few hundred Hz of error: the discriminator's symbol decision is
    // sign(f1 - f0), where both terms scale together, and the MLSE branch waveforms are
    // built from a measurement the burst estimator makes for itself.
    //
    // This was a user setting with no GUI control, still reachable from presets and the
    // REST API. Nothing good could come of that. It scales the discriminator output, which
    // is what the deviation estimator's calibration is expressed in, so the two have to
    // agree exactly - and a value that only a preset can carry is a value nobody can see.
    static constexpr float PACKETDEMOD_FM_DEVIATION = 2500.0f;

    // Tone transitions are modelled as a raised cosine frequency sweep this many channel
    // samples long, not an instant switch. NO-84's transmitter measures about a third of a
    // symbol - band limited audio somewhere in its chain - and every symbol adjacent to a
    // transition is then 37 millicycles of audio phase away from the instant switch model,
    // enough that the branch correlations stay strong but stop telling the bits apart.
    static const int PACKETDEMOD_MLSE_RAMP = 8;

    // Per survivor phase loop gains. 0.1/0.01 measures best on a static carrier, but a
    // satellite pass drifts tens of Hz per second and the sluggish loop leaves tens of
    // symbol errors per frame; 0.5/0.05 decodes every strong NO-84 burst and costs nothing
    // on AWGN. Keep the 10:1 ratio - 0.2/0.05 is disastrous everywhere, because it is the
    // damping ratio that matters, not the speed.
    static constexpr double PACKETDEMOD_MLSE_PHASE_GAIN = 0.5;
    static constexpr double PACKETDEMOD_MLSE_FREQ_GAIN = 0.05;

    // Symbol timing hypotheses run in parallel: timing is searched rather than tracked, so
    // this trades CPU for sensitivity. Every result the detector has been validated against
    // was measured at 16, and the replay multiplies it by branch metric, symbol rate and
    // tone pair - 32 builds 640 replay detectors, which measured as feed() taking 443 ms
    // and dropping FIFO samples in the field. It was a settings field with no GUI control
    // and a deserialize fallback that disagreed with resetToDefaults, so presets silently
    // loaded 32; a constant cannot drift from itself.
    static const int PACKETDEMOD_MLSE_PHASES = 16;

    // Carrier hypotheses span +/- this many Hz in PACKETDEMOD_MLSE_FREQ_STEP steps. The
    // estimator retunes the whole grid onto the measured carrier, so this only has to cover
    // what one burst's measurement can be wrong by, plus drift between bursts.
    static const int PACKETDEMOD_MLSE_FREQ_RANGE = 600;

    PacketDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const PacketDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    int getBaudRate() const;
};

#endif /* INCLUDE_PACKETDEMODSETTINGS_H */
