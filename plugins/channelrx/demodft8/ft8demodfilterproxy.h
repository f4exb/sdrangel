///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#ifndef INCLUDE_FT8DEMODFILTERPROXY_H
#define INCLUDE_FT8DEMODFILTERPROXY_H

#include <QSortFilterProxyModel>

class FT8DemodFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    FT8DemodFilterProxy(QObject *parent = nullptr);

    void resetFilter();
    void setFilterUTC(const QString& utcString);
    void setFilterDf(int df);
    void setFilterCall(const QString& utcString);
    void setFilterLoc(const QString& utcString);
    void setFilterInfo(const QString& infoString);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    enum filterActive
    {
        FILTER_NONE,
        FILTER_UTC,
        FILTER_DF,
        FILTER_CALL,
        FILTER_LOC,
        FILTER_INFO
    };

    bool dfInRange(int df) const;
    filterActive m_filterActive;
    QString m_utc;
    int m_df;
    QString m_call;
    QString m_loc;
    QString m_info;
};

#endif // INCLUDE_FT8DEMODFILTERPROXY_H
