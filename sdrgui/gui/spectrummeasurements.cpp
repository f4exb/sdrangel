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

#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>

#include "gui/spectrummeasurements.h"

QSize SpectrumMeasurementsTable::sizeHint() const
{
    // QAbstractScrollArea::sizeHint() always returns 256x192 when sizeAdjustPolicy == AdjustIgnored
    // If using AdjustToContents policy, the default sizeHint includes the stretched empty column
    // which we don't want, as that prevents the Auto Stack feature from reducing it
    // So we need some custom code to set size ignoring this column
    int width = 0;
    int height = 0;
    for (int i = 0; i < columnCount() - 1; i++) {   // -1 to ignore empty column at end of row
        width += columnWidth(i);
    }
    for (int i = 0; i < rowCount(); i++) {
        height += rowHeight(i);
    }

    int doubleFrame = 2 * frameWidth();

    width += verticalHeader()->width() + doubleFrame;
    height += horizontalHeader()->height() + doubleFrame;

    return QSize(width, height);
}

QSize SpectrumMeasurementsTable::minimumSizeHint() const
{
    QSize min1 = QTableWidget::minimumSizeHint(); // This seems to include vertical space for scroll bar, which we don't need
    int height = horizontalHeader()->height() + 2 * frameWidth() + rowHeight(0);
    return QSize(min1.width(), height);
}

class SDRGUI_API UnitsDelegate : public QStyledItemDelegate {

public:
    UnitsDelegate(QObject *parent = nullptr);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QString s = text(index);
        return QSize(width(s, option.fontMetrics) + 2, option.fontMetrics.height());
    }

    int width(const QString &s, const QFontMetrics &fm) const
    {
        int left = s.size() > 0 ? fm.leftBearing(s[0]) : 0;
        int right = s.size() > 0 ? fm.rightBearing(s[s.size()-1]) : 0;
        return fm.horizontalAdvance(s) + left + right;
    }

    QString text(const QModelIndex &index) const
    {
        QString units = index.data(UNITS_ROLE).toString();
        QString s;
        if (units == "Hz")
        {
            s = formatEngineering(index.data().toLongLong());
        }
        else
        {
            int precision = index.data(PRECISION_ROLE).toInt();
            double d = index.data().toDouble();
            s = QString::number(d, 'f', precision);
        }
        return s + units;
    }

    enum Roles {
        UNITS_ROLE = Qt::UserRole,
        PRECISION_ROLE,
        SPEC_ROLE
    };

protected:
    QString formatEngineering(int64_t value) const;

};

UnitsDelegate::UnitsDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QString UnitsDelegate::formatEngineering(int64_t value) const
{
    if (value == 0) {
        return "0";
    }
    int64_t absValue = std::abs(value);

    QString digits = QString::number(absValue);
    int cnt = digits.size();

    QString point = QLocale::system().decimalPoint();
    QString group = QLocale::system().groupSeparator();
    int i;
    for (i = cnt - 3; i >= 4; i -= 3)
    {
        digits = digits.insert(i, group);
    }
    if (absValue >= 1000) {
        digits = digits.insert(i, point);
    }
    if (cnt > 9) {
        digits = digits.append("G");
    } else if (cnt > 6) {
        digits = digits.append("M");
    } else if (cnt > 3) {
        digits = digits.append("k");
    }
    if (value < 0) {
        digits = digits.insert(0, "-");
    }

    return digits;
}

void UnitsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFontMetrics fm = painter->fontMetrics();

    QString s = text(index);
    int sWidth = width(s, fm);
    while ((sWidth > option.rect.width()) && !s.isEmpty())
    {
        s = s.mid(1);
        sWidth = width(s, fm);
    }

    int y = option.rect.y() + (option.rect.height()) - ((option.rect.height() - fm.ascent()) / 2); // Align center vertically

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    painter->setPen(opt.palette.color(cg, QPalette::Text));

    painter->drawText(option.rect.x() + option.rect.width() - 1 - sWidth, y, s);
}

const QStringList SpectrumMeasurements::m_measurementColumns = {
    "Current",
    "Mean",
    "Min",
    "Max",
    "Range",
    "Std Dev",
    "Count",
    "Spec",
    "Fails",
    ""
};

const QStringList SpectrumMeasurements::m_tooltips = {
    "Current value",
    "Mean average of values",
    "Minimum value",
    "Maximum value",
    "Range of values (Max-Min)",
    "Standard deviation",
    "Count of values",
    "Specification for value.\n\nE.g. <-100.5, >34.5 or =10.2",
    "Count of values that failed to meet specification",
    ""
};

SpectrumMeasurements::SpectrumMeasurements(QWidget *parent) :
    QWidget(parent),
    m_measurement(SpectrumSettings::MeasurementPeaks),
    m_precision(1),
    m_table(nullptr),
    m_peakTable(nullptr)
{
    m_textBrush.setColor(Qt::white);  // Should get this from the style sheet?
    m_redBrush.setColor(Qt::red);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);
}

void SpectrumMeasurements::createMeasurementsTable(const QStringList &rows, const QStringList &units)
{
    m_table = new SpectrumMeasurementsTable();

    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->verticalHeader()->setSectionsMovable(true);

    m_table->setColumnCount(m_measurementColumns.size());
    for (int i = 0; i < m_measurementColumns.size(); i++)
    {
        QTableWidgetItem *item = new QTableWidgetItem(m_measurementColumns[i]);
        item->setToolTip(m_tooltips[i]);
        m_table->setHorizontalHeaderItem(i, item);
    }
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); i++)
    {
        m_table->setVerticalHeaderItem(i, new QTableWidgetItem(rows[i]));
        for (int j = 0; j < m_measurementColumns.size(); j++)
        {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (j < COL_COUNT)
            {
                item->setData(UnitsDelegate::UNITS_ROLE, units[i]);
                item->setData(UnitsDelegate::PRECISION_ROLE, m_precision);
            }
            else if (j == COL_SPEC)
            {
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
            }
            m_table->setItem(i, j, item);
        }
        Measurement m;
        m.m_units = units[i];
        m_measurements.append(m);
    }
    resizeMeasurementsTable();

    m_table->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    for (int i = 0; i < COL_COUNT; i++) {
        m_table->setItemDelegateForColumn(i, new UnitsDelegate());
    }
    createTableMenus();

    // Cell context menu
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &SpectrumMeasurements::tableContextMenu);
}

void SpectrumMeasurements::createPeakTable(int peaks)
{
    m_peakTable = new SpectrumMeasurementsTable();
    m_peakTable->horizontalHeader()->setSectionsMovable(true);

    QStringList columns = QStringList{"Frequency", "Power", ""};

    m_peakTable->setColumnCount(columns.size());
    m_peakTable->setRowCount(peaks);

    for (int i = 0; i < columns.size(); i++) {
        m_peakTable->setHorizontalHeaderItem(i, new QTableWidgetItem(columns[i]));
    }
    m_peakTable->horizontalHeader()->setStretchLastSection(true);

    for (int i = 0; i < peaks; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setFlags(Qt::ItemIsEnabled);
            if (j == COL_FREQUENCY) {
                item->setData(UnitsDelegate::UNITS_ROLE, "Hz");
            } else if (j == COL_POWER) {
                item->setData(UnitsDelegate::UNITS_ROLE, " dB");
                item->setData(UnitsDelegate::PRECISION_ROLE, m_precision);
            }
            m_peakTable->setItem(i, j, item);
        }
    }
    resizePeakTable();

    m_peakTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_peakTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    m_peakTable->setItemDelegateForColumn(COL_FREQUENCY, new UnitsDelegate());
    m_peakTable->setItemDelegateForColumn(COL_POWER, new UnitsDelegate());

    // Cell context menu
    m_peakTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_peakTable, &QTableWidget::customContextMenuRequested, this, &SpectrumMeasurements::peakTableContextMenu);
}

void SpectrumMeasurements::createTableMenus()
{
    // Add context menu to allow hiding/showing of columns
    m_rowMenu = new QMenu(m_table);
    for (int i = 0; i < m_table->verticalHeader()->count() - 1; i++) // -1 to skip empty column
    {
        QString text = m_table->verticalHeaderItem(i)->text();
        m_rowMenu->addAction(createCheckableItem(text, i, true, true));
    }
    m_table->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table->verticalHeader(), &QTableWidget::customContextMenuRequested, this, &SpectrumMeasurements::rowSelectMenu);

    // Add context menu to allow hiding/showing of rows
    m_columnMenu = new QMenu(m_table);
    for (int i = 0; i < m_table->horizontalHeader()->count(); i++)
    {
        QString text = m_table->horizontalHeaderItem(i)->text();
        m_columnMenu->addAction(createCheckableItem(text, i, true, false));
    }
    m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table->horizontalHeader(), &QTableWidget::customContextMenuRequested, this, &SpectrumMeasurements::columnSelectMenu);
}

void SpectrumMeasurements::createChannelPowerTable()
{
    QStringList rows = {"Channel power"};
    QStringList units = {" dB"};

    createMeasurementsTable(rows, units);
}

void SpectrumMeasurements::createAdjacentChannelPowerTable()
{
    QStringList rows = {"Left power", "Left ACPR", "Center power", "Right power", "Right ACPR"};
    QStringList units = {" dB", " dBc", " dB", " dB", " dBc"};

    createMeasurementsTable(rows, units);
}

void SpectrumMeasurements::createOccupiedBandwidthTable()
{
    QStringList rows = {"Occupied B/W"};
    QStringList units = {"Hz"};

    createMeasurementsTable(rows, units);
}

void SpectrumMeasurements::create3dBBandwidthTable()
{
    QStringList rows = {"3dB B/W"};
    QStringList units = {"Hz"};

    createMeasurementsTable(rows, units);
}

void SpectrumMeasurements::createSNRTable()
{
    QStringList rows = {"SNR", "SNFR", "THD", "THD+N", "SINAD", "SFDR",};
    QStringList units = {" dB", " dB", " dB", " dB", " dB", " dBc"};

    createMeasurementsTable(rows, units);
}

// Create column select menu item
QAction *SpectrumMeasurements::createCheckableItem(QString &text, int idx, bool checked, bool row)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    if (row) {
        connect(action, &QAction::triggered, this, &SpectrumMeasurements::rowSelectMenuChecked);
    } else {
        connect(action, &QAction::triggered, this, &SpectrumMeasurements::columnSelectMenuChecked);
    }
    return action;
}

// Right click in table header - show row select menu
void SpectrumMeasurements::rowSelectMenu(QPoint pos)
{
    m_rowMenu->popup(m_table->verticalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show row when menu selected
void SpectrumMeasurements::rowSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        m_table->setRowHidden(idx, !action->isChecked());
    }
}

// Right click in table header - show column select menu
void SpectrumMeasurements::columnSelectMenu(QPoint pos)
{
    m_columnMenu->popup(m_table->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void SpectrumMeasurements::columnSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        m_table->setColumnHidden(idx, !action->isChecked());
    }
}

void SpectrumMeasurements::tableContextMenu(QPoint pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(m_table);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        tableContextMenu->popup(m_table->viewport()->mapToGlobal(pos));
   }
}

void SpectrumMeasurements::peakTableContextMenu(QPoint pos)
{
    QTableWidgetItem *item = m_peakTable->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(m_peakTable);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        tableContextMenu->popup(m_peakTable->viewport()->mapToGlobal(pos));
   }
}

void SpectrumMeasurements::resizeMeasurementsTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = m_table->rowCount();
    m_table->setRowCount(row + 1);
    m_table->setItem(row, COL_CURRENT, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_MEAN, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_MIN, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_MAX, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_RANGE, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_STD_DEV, new QTableWidgetItem("-120.0 dBc"));
    m_table->setItem(row, COL_COUNT, new QTableWidgetItem("100000"));
    m_table->setItem(row, COL_SPEC, new QTableWidgetItem(">= -120.0"));
    m_table->setItem(row, COL_FAILS, new QTableWidgetItem("100000"));
    m_table->resizeColumnsToContents();
    m_table->removeRow(row);
}

void SpectrumMeasurements::resizePeakTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = m_peakTable->rowCount();
    m_peakTable->setRowCount(row + 1);
    m_peakTable->setItem(row, COL_FREQUENCY, new QTableWidgetItem("6.000,000,000GHz"));
    m_peakTable->setItem(row, COL_POWER, new QTableWidgetItem("-120.0 dB"));
    m_peakTable->resizeColumnsToContents();
    m_peakTable->removeRow(row);
}

void SpectrumMeasurements::setMeasurementParams(SpectrumSettings::Measurement measurement, int peaks, int precision)
{
    if (   (measurement != m_measurement)
        || (m_precision != precision)
        || ((m_peakTable == nullptr) && (m_table == nullptr))
        || ((m_peakTable != nullptr) && (peaks != m_peakTable->rowCount()))
       )
    {
        // Tried using setVisible(), but that would hang, so delete and recreate
        delete m_peakTable;
        m_peakTable = nullptr;
        delete m_table;
        m_table = nullptr;

        m_measurement = measurement;
        m_precision = precision;

        switch (measurement)
        {
        case SpectrumSettings::MeasurementPeaks:
            createPeakTable(peaks);
            layout()->addWidget(m_peakTable);
            break;
        case SpectrumSettings::MeasurementChannelPower:
            reset();
            createChannelPowerTable();
            layout()->addWidget(m_table);
            break;
        case SpectrumSettings::MeasurementAdjacentChannelPower:
            reset();
            createAdjacentChannelPowerTable();
            layout()->addWidget(m_table);
            break;
        case SpectrumSettings::MeasurementOccupiedBandwidth:
            reset();
            createOccupiedBandwidthTable();
            layout()->addWidget(m_table);
            break;
        case SpectrumSettings::Measurement3dBBandwidth:
            reset();
            create3dBBandwidthTable();
            layout()->addWidget(m_table);
            break;
        case SpectrumSettings::MeasurementSNR:
            reset();
            createSNRTable();
            layout()->addWidget(m_table);
            break;
        default:
            break;
        }

        // Set size to show full table
        if (m_peakTable)
        {
            m_peakTable->show();  // Need to call show() so that sizeHint() is not 0,0
            resize(sizeHint());
        }
        else if (m_table)
        {
            m_table->show();
            resize(sizeHint());
        }
    }
}

void SpectrumMeasurements::reset()
{
    for (int i = 0; i < m_measurements.size(); i++) {
        m_measurements[i].reset();
    }
    if (m_table)
    {
        for (int i = 0; i < m_table->rowCount(); i++)
        {
            for (int j = 0; j < m_table->columnCount(); j++)
            {
                if (j != COL_SPEC) {
                    m_table->item(i, j)->setText("");
                }
            }
        }
    }
}

// Check the value meets the user-defined specification
bool SpectrumMeasurements::checkSpec(const QString &spec, double value) const
{
    if (spec.isEmpty()) {
        return true;
    }
    if (spec.startsWith("<="))
    {
        double limit = spec.mid(2).toDouble();
        return value <= limit;
    }
    else if (spec[0] == '<')
    {
        double limit = spec.mid(1).toDouble();
        return value < limit;
    }
    else if (spec.startsWith(">="))
    {
        double limit = spec.mid(2).toDouble();
        return value >= limit;
    }
    else if (spec[0] == '>')
    {
        double limit = spec.mid(1).toDouble();
        return value > limit;
    }
    else if (spec[0] == '=')
    {
        double limit = spec.mid(1).toDouble();
        return value == limit;
    }
    return false;
}

void SpectrumMeasurements::updateMeasurement(int row, float value)
{
    m_measurements[row].add(value);
    double mean = m_measurements[row].mean();

    m_table->item(row, COL_CURRENT)->setData(Qt::DisplayRole, value);
    m_table->item(row, COL_MEAN)->setData(Qt::DisplayRole, mean);
    m_table->item(row, COL_MIN)->setData(Qt::DisplayRole, m_measurements[row].m_min);
    m_table->item(row, COL_MAX)->setData(Qt::DisplayRole, m_measurements[row].m_max);
    m_table->item(row, COL_RANGE)->setData(Qt::DisplayRole, m_measurements[row].m_max - m_measurements[row].m_min);
    m_table->item(row, COL_STD_DEV)->setData(Qt::DisplayRole, m_measurements[row].stdDev());
    m_table->item(row, COL_COUNT)->setData(Qt::DisplayRole, m_measurements[row].m_values.size());

    QString spec = m_table->item(row, COL_SPEC)->text();
    bool valueOK = checkSpec(spec, value);
    bool meanOK = checkSpec(spec, mean);
    bool minOK = checkSpec(spec, m_measurements[row].m_min);
    bool mmaxOK = checkSpec(spec, m_measurements[row].m_max);

    if (!valueOK)
    {
        m_measurements[row].m_fails++;
        m_table->item(row, 8)->setData(Qt::DisplayRole, m_measurements[row].m_fails);
    }

    // item->setForeground doesn't work, perhaps as we have style sheet applied?
    m_table->item(row, COL_CURRENT)->setData(Qt::ForegroundRole, valueOK ? m_textBrush : m_redBrush);
    m_table->item(row, COL_MEAN)->setData(Qt::ForegroundRole, meanOK ? m_textBrush : m_redBrush);
    m_table->item(row, COL_MIN)->setData(Qt::ForegroundRole, minOK ? m_textBrush : m_redBrush);
    m_table->item(row, COL_MAX)->setData(Qt::ForegroundRole, mmaxOK ? m_textBrush : m_redBrush);
}

void SpectrumMeasurements::setSNR(float snr, float snfr, float thd, float thdpn, float sinad)
{
    updateMeasurement(0, snr);
    updateMeasurement(1, snfr);
    updateMeasurement(2, thd);
    updateMeasurement(3, thdpn);
    updateMeasurement(4, sinad);
}

void SpectrumMeasurements::setSFDR(float sfdr)
{
    updateMeasurement(5, sfdr);
}

void SpectrumMeasurements::setChannelPower(float power)
{
    updateMeasurement(0, power);
}

void SpectrumMeasurements::setAdjacentChannelPower(float left, float leftACPR, float center, float right, float rightACPR)
{
    updateMeasurement(0, left);
    updateMeasurement(1, leftACPR);
    updateMeasurement(2, center);
    updateMeasurement(3, right);
    updateMeasurement(4, rightACPR);
}

void SpectrumMeasurements::setOccupiedBandwidth(float occupiedBandwidth)
{
    updateMeasurement(0, occupiedBandwidth);
}

void SpectrumMeasurements::set3dBBandwidth(float bandwidth)
{
    updateMeasurement(0, bandwidth);
}

void SpectrumMeasurements::setPeak(int peak, int64_t frequency, float power)
{
    if (peak < m_peakTable->rowCount())
    {
        m_peakTable->item(peak, COL_FREQUENCY)->setData(Qt::DisplayRole, QVariant((qlonglong)frequency));
        m_peakTable->item(peak, COL_POWER)->setData(Qt::DisplayRole, power);
    }
    else
    {
        qDebug() << "SpectrumMeasurements::setPeak: Attempt to set peak " << peak << " when only " << m_peakTable->rowCount() << " rows in peak table";
    }
}
