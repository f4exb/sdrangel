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
#include "ft8demodfilterproxy.h"

FT8DemodFilterProxy::FT8DemodFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_filterActive(FILTER_NONE)
{
}

void FT8DemodFilterProxy::resetFilter()
{
    m_filterActive = FILTER_NONE;
    invalidateFilter();
}

void FT8DemodFilterProxy::setFilterUTC(const QString& utcString)
{
    m_filterActive = FILTER_UTC;
    m_utc = utcString;
    invalidateFilter();
}

void FT8DemodFilterProxy::setFilterDf(int df)
{
    m_filterActive = FILTER_DF;
    m_df = df;
    invalidateFilter();
}

void FT8DemodFilterProxy::setFilterCall(const QString& callString)
{
    m_filterActive = FILTER_CALL;
    m_call = callString;
    invalidateFilter();
}

void FT8DemodFilterProxy::setFilterLoc(const QString& locString)
{
    m_filterActive = FILTER_LOC;
    m_loc = locString;
    invalidateFilter();
}

bool FT8DemodFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterActive == FILTER_NONE) {
        return true;
    }

    if (m_filterActive == FILTER_UTC)
    {
        QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        return sourceModel()->data(index).toString() == m_utc;
    }

    if (m_filterActive == FILTER_DF)
    {
        QModelIndex index = sourceModel()->index(sourceRow, 4, sourceParent);
        int df = sourceModel()->data(index).toInt();
        return (df >= m_df - 4) && (df <= m_df + 4); // +/- 4 Hz tolerance which is about one symbol width
    }

    if (m_filterActive == FILTER_CALL)
    {
        QModelIndex indexCall1 = sourceModel()->index(sourceRow, 6, sourceParent);
        QModelIndex indexCall2 = sourceModel()->index(sourceRow, 7, sourceParent);
        return (sourceModel()->data(indexCall1).toString() == m_call) ||
            (sourceModel()->data(indexCall2).toString() == m_call);
    }

    if (m_filterActive == FILTER_LOC)
    {
        QModelIndex index = sourceModel()->index(sourceRow, 8, sourceParent);
        return sourceModel()->data(index).toString() == m_loc;
    }

    return true;
}
