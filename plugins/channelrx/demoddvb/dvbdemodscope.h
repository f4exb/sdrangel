///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DVBDEMODSCOPE_H
#define INCLUDE_DVBDEMODSCOPE_H

#include <complex>
#include <deque>
#include <vector>

#include <QColor>
#include <QImage>
#include <QWidget>

// Constellation of the equalised data cells at the left, and at the right, chosen by right-clicking: the channel
// frequency response across the carriers, the response over time as a waterfall, the impulse response, or a history
// of MER, LDPC iterations and uncorrected errors
class DVBDemodScope : public QWidget
{
    Q_OBJECT

public:
    enum View { Response, Waterfall, ImpulseResponse, History };

    explicit DVBDemodScope(QWidget *parent = nullptr);

    //! cells have unit average power. constellationSize is 4, 16, 64 or 256, or 0 if not known. alpha is of DVB-T's
    //! hierarchical modes: the points nearest the axes are alpha - 1 further from them than in a uniform constellation.
    //! A new response is added to the waterfall
    void setData(const std::vector<std::complex<float>>& cells, const std::vector<float>& responseDb, int constellationSize, int alpha = 1);
    //! The constellation's ideal points, unit average power, for those that aren't square QAM (DVB-S2's PSK and APSK).
    //! Empty to draw them from the size given to setData()
    void setPoints(const std::vector<std::complex<float>>& points);
    //! Satellite standards show the input's spectrum where the OFDM ones show the channel's frequency response
    void setSpectrum(bool spectrum);
    //! powerDb: dB from the strongest path against delay from the FFT window's start, resolution samples a bin, wrapping
    //! round. guard: the guard interval, samples. sampleRate: of the samples, for the delays in microseconds
    void setImpulseResponse(const std::vector<float>& powerDb, double resolution, int guard, double sampleRate);
    //! A point of the history, added every interval seconds: MER (dB, NaN if not known), LDPC iterations (negative if
    //! not DVB-T2), and the packets or FEC blocks that couldn't be corrected since the last
    void addHistory(double mer, double iterations, int errors, double interval);
    void clearHistory();
    View getView() const { return m_view; }
    void setView(View view);

protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct HistoryPoint
    {
        float m_mer;
        float m_iterations;
        int m_errors;
    };

    void paintConstellation(QPainter& painter, const QRect& rect);
    void paintResponse(QPainter& painter, const QRect& rect);
    void paintWaterfall(QPainter& painter, const QRect& rect);
    void paintImpulseResponse(QPainter& painter, const QRect& rect);
    void paintHistory(QPainter& painter, const QRect& rect);
    void paintTitle(QPainter& painter, const QRect& rect, const QString& title);
    void addWaterfallRow(const std::vector<float>& responseDb);
    //! dB relative to the mean, -30 to +10, from black through blue, cyan and yellow to red
    static QRgb waterfallColour(float db);

    View m_view;
    std::vector<std::complex<float>> m_cells;
    std::vector<float> m_responseDb;
    int m_constellationSize;
    int m_alpha;
    std::vector<std::complex<float>> m_points;
    bool m_spectrum = false;

    QImage m_waterfall;                 //!< Newest response at the top
    int m_waterfallRows;                //!< Filled
    double m_rowInterval;               //!< Seconds between rows, measured

    std::vector<float> m_impulseDb;
    double m_impulseResolution;
    int m_impulseGuard;
    double m_sampleRate;

    std::deque<HistoryPoint> m_history; //!< Oldest first
    double m_historyInterval;
    qint64 m_lastRowTime;               //!< ms, of the last waterfall row

    static const int m_historyLength = 300;
    static const int m_waterfallLength = 300;

    static const QColor m_gridColour;
    static const QColor m_zeroColour;
    static const QColor m_labelColour;
    static const QColor m_merColour;
    static const QColor m_iterationsColour;
    static const QColor m_errorsColour;
};

#endif // INCLUDE_DVBDEMODSCOPE_H
