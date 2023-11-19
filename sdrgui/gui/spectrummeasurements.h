///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_SPECTRUMMEASUREMENTS_H_
#define SDRGUI_SPECTRUMMEASUREMENTS_H_

#include <QWidget>
#include <QMenu>
#include <QTableWidget>

#include "dsp/spectrumsettings.h"
#include "export.h"

class SDRGUI_API SpectrumMeasurementsTable : public QTableWidget {
    Q_OBJECT

public:
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;
};

// Displays spectrum measurements in a table
class SDRGUI_API SpectrumMeasurements : public QWidget {
    Q_OBJECT

    struct Measurement {
        QList<float> m_values;
        float m_min;
        float m_max;
        double m_sum;
        int m_fails;
        QString m_units;

        Measurement()
        {
            reset();
        }

        void reset()
        {
            m_values.clear();
            m_min = std::numeric_limits<float>::max();
            m_max = -std::numeric_limits<float>::max();
            m_sum = 0.0;
            m_fails = 0;
        }

        void add(float value)
        {
            m_min = std::min(value, m_min);
            m_max = std::max(value, m_max);
            m_sum += value;
            m_values.append(value);
        }

        double mean() const
        {
            return m_sum / m_values.size();
        }

        double stdDev() const
        {
            double u = mean();
            double sum = 0.0;
            for (int i = 0; i < m_values.size(); i++)
            {
                double d = m_values[i] - u;
                sum += d * d;
            }
            if (m_values.size() < 2) {
                return 0.0;
            } else {
                return sqrt(sum / (m_values.size() - 1));
            }
        }
    };

public:
    SpectrumMeasurements(QWidget *parent = nullptr);
    void setMeasurementParams(SpectrumSettings::Measurement measurement, int peaks, int precision);
    void setSNR(float snr, float snfr, float thd, float thdpn, float sinad);
    void setSFDR(float sfdr);
    void setChannelPower(float power);
    void setAdjacentChannelPower(float left, float leftACPR, float center, float right, float rightACPR);
    void setOccupiedBandwidth(float occupiedBandwidth);
    void set3dBBandwidth(float bandwidth);
    void setPeak(int peak, int64_t frequency, float power);
    void reset();

private:
    void createMeasurementsTable(const QStringList &rows, const QStringList &units);
    void createPeakTable(int peaks);
    void createTableMenus();
    void createChannelPowerTable();
    void createAdjacentChannelPowerTable();
    void createOccupiedBandwidthTable();
    void create3dBBandwidthTable();
    void createSNRTable();
    void tableContextMenu(QPoint pos);
    void peakTableContextMenu(QPoint pos);
    void rowSelectMenu(QPoint pos);
    void rowSelectMenuChecked(bool checked);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked);
    QAction *createCheckableItem(QString &text, int idx, bool checked, bool row);
    void resizeMeasurementsTable();
    void resizePeakTable();
    void updateMeasurement(int row, float value);
    bool checkSpec(const QString &spec, double value) const;

    SpectrumSettings::Measurement m_measurement;
    int m_precision;

    SpectrumMeasurementsTable *m_table;
    QMenu *m_rowMenu;
    QMenu *m_columnMenu;
    QList<Measurement> m_measurements;

    SpectrumMeasurementsTable *m_peakTable;
    QBrush m_textBrush;
    QBrush m_redBrush;

    enum MeasurementsCol {
        COL_CURRENT,
        COL_MEAN,
        COL_MIN,
        COL_MAX,
        COL_RANGE,
        COL_STD_DEV,
        COL_COUNT,
        COL_SPEC,
        COL_FAILS,
        COL_EMPTY
    };

    enum PeakTableCol {
        COL_FREQUENCY,
        COL_POWER,
        COL_PEAK_EMPTY
    };

    static const QStringList m_measurementColumns;
    static const QStringList m_tooltips;

};

#endif // SDRGUI_SPECTRUMMEASUREMENTS_H_
