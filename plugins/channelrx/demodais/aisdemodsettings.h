///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#ifndef INCLUDE_AISDEMODSETTINGS_H
#define INCLUDE_AISDEMODSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the tables
#define AISDEMOD_MESSAGE_COLUMNS 10

struct AISDemodSettings
{
    qint32 m_baud;
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_fmDeviation; //!< Peak deviation. M.1371-5 2.3.2 specifies a modulation index of
                        //!< 0.5, and h = 2.dev/baud, so the peak deviation is 2400 Hz at
                        //!< 9600 baud. 4800 Hz is the mark to space separation, not the
                        //!< deviation
    Real m_correlationThreshold;    //!< Normalised matched filter output for the preamble,
                                    //!< 0 to 1. Noise sits near 0.886/sqrt(block length),
                                    //!< so this cannot be retuned without regard to
                                    //!< AISDEMOD_IQ_BLOCKS - the two are coupled
    QString m_filterMMSI;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    enum UDPFormat {
        Binary,
        NMEA
    } m_udpFormat;

    QString m_logFilename;
    bool m_logEnabled;
    bool m_showSlotMap;
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
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_messageColumnIndexes[AISDEMOD_MESSAGE_COLUMNS];//!< How the columns are ordered in the table
    int m_messageColumnSizes[AISDEMOD_MESSAGE_COLUMNS];  //!< Size of the columns in the table

    static const int AISDEMOD_BAUD_RATE = 9600;
    static const int AISDEMOD_CHANNEL_SAMPLE_RATE = 96000; //!< 10x 9600 baud rate (use even multiple so Gaussian filter has odd number of taps).
                                                          //!< 6x was enough for a discriminator, but the sequence detector is sensitive to
                                                          //!< symbol timing and the finer grid is worth measurable sensitivity
    static const int m_scopeStreams = 9;

    //! The trellis models the transmitted waveform, so this is the transmit BT-product of
    //! 0.4 from M.1371-5 2.3.1.2 - not the 0.5 receive BT-product of 2.3.1.3, which is what
    //! m_pulseShape uses for the preamble correlator. They are different numbers on purpose.
    static constexpr float AISDEMOD_MLSE_BT = 0.4f;

    //! Preamble detection runs a matched filter on the complex baseband rather than
    //! correlating the discriminator output, which has an FM threshold and so gives up
    //! exactly where the sequence detector still works. Measured at the noise floor it
    //! reaches the same message count on 629 triggers where the old statistic needed
    //! 173,000.
    //!
    //! Carrier phase is unknown so the blocks are combined non-coherently, and carrier
    //! frequency is unknown too - a single coherent correlation over all 24 symbols would
    //! be cancelled by a few hundred Hz. 6 blocks tolerates about +/-1200 Hz; 4 was tried
    //! and lost real messages on a recording with larger offsets.
    static const int AISDEMOD_IQ_BLOCKS = 6;

    //! The correlation peak is about a symbol wide, so it does not need evaluating on
    //! every one of 10 samples per symbol. Sub sample alignment is recovered by the
    //! timing phase retries, which search +/-2 samples anyway.
    static const int AISDEMOD_IQ_DECIM = 2;

    static const int AISDEMOD_MLSE_SPAN = 4;  //!< Symbols of Gaussian pulse the trellis resolves. 4 gives a 32 state
                                              //!< trellis against 16 for 3, and is the largest single sensitivity
                                              //!< win available - measured +25% messages on a weak recording. An
                                              //!< earlier note here said 3 was enough and 4 measured no better;
                                              //!< that was at 6 samples per symbol with the symbol timing a third
                                              //!< of a symbol out, so the trellis could not use the extra span

    AISDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AISDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_AISDEMODSETTINGS_H */
