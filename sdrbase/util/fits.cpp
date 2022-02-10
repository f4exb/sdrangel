///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <algorithm>
#include <cmath>

#include <QtGlobal>
#include <QRegExp>
#include <QDebug>
#include <QResource>

#include "fits.h"

FITS::FITS(QString resourceName) :
    m_valid(false)
{
    QResource m_res(resourceName);
    if (!m_res.isValid()) {
        qWarning() << "FITS: - " << resourceName << " is not a valid resource";
        return;
    }
    int m_headerSize = 2880;
    qint64 m_fileSize;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    m_data = m_res.uncompressedData();
    m_fileSize = m_res.uncompressedSize();
#else
    m_data = QByteArray::fromRawData((const char *)m_res.data(), m_res.size());
    if (m_res.isCompressed()) {
        m_data = qUncompress(m_data);
    }
    m_fileSize = m_res.size();
#endif
    int hLen = std::min((qint64)m_headerSize * 3, m_fileSize);   // Could possibly be bigger
    QByteArray headerBytes = m_data.left(hLen);
    QString header = QString::fromLatin1(headerBytes);
    QRegExp widthRE("NAXIS1 *= *([0-9]+)");
    QRegExp heightRE("NAXIS2 *= *([0-9]+)");
    QRegExp bitsPerPixelRE("BITPIX *= *(-?[0-9]+)");
    QRegExp bzeroRE("BZERO *= *([0-9]+)");
    QRegExp bscaleRE("BSCALE *= *(-?[0-9]+(.[0-9]+)?)");
    QRegExp buintRE("BUNIT *= *\\'([A-Z ]+)\\'");
    QRegExp cdelt1RE("CDELT1 *= *(-?[0-9]+(.[0-9]+)?)");
    QRegExp cdelt2RE("CDELT2 *= *(-?[0-9]+(.[0-9]+)?)");
    QRegExp endRE("END {77}");

    if (widthRE.indexIn(header) != -1)
        m_width = widthRE.capturedTexts()[1].toInt();
    else
    {
        qWarning() << "FITS: NAXIS1 missing";
        return;
    }
    if (heightRE.indexIn(header) != -1)
        m_height = heightRE.capturedTexts()[1].toInt();
    else
    {
        qWarning() << "FITS: NAXIS2 missing";
        return;
    }
    if (bitsPerPixelRE.indexIn(header) != -1)
        m_bitsPerPixel = bitsPerPixelRE.capturedTexts()[1].toInt();
    else
    {
        qWarning() << "FITS: BITPIX missing";
        return;
    }
    m_bytesPerPixel = abs(m_bitsPerPixel)/8;
    if (bzeroRE.indexIn(header) != -1)
        m_bzero = bzeroRE.capturedTexts()[1].toInt();
    else
        m_bzero = 0;
    if (bscaleRE.indexIn(header) != -1)
        m_bscale = bscaleRE.capturedTexts()[1].toDouble();
    else
        m_bscale = 1.0;

    if (cdelt1RE.indexIn(header) != -1)
        m_cdelta1 = cdelt1RE.capturedTexts()[1].toDouble();
    else
        m_cdelta1 = 0.0;
    if (cdelt2RE.indexIn(header) != -1)
        m_cdelta2 = cdelt2RE.capturedTexts()[1].toDouble();
    else
        m_cdelta2 = 0.0;

    if (buintRE.indexIn(header) != -1)
    {
        m_buint = buintRE.capturedTexts()[1].trimmed();
        if (m_buint.contains("MILLI"))
            m_uintScale = 0.001f;
        else
            m_uintScale = 1.0f;
    }
    else
        m_uintScale = 1.0f;
    int endIdx = endRE.indexIn(header);
    if (endIdx == -1)
    {
        qWarning() << "FITS: END missing";
        return;
    }
    m_dataStart = ((endIdx + m_headerSize) / m_headerSize) * m_headerSize;
    m_valid = true;
}

float FITS::value(int x, int y) const
{
    int offset = m_dataStart + (m_height-1-y) * m_width * m_bytesPerPixel + x * m_bytesPerPixel;
    const uchar *data = (const uchar *)m_data.data();
    // Big-endian
    int v = 0;
    for (int i = m_bytesPerPixel - 1; i >= 0; i--)
        v += data[offset++] << (i*8);
    if (m_bitsPerPixel > 0)
    {
        // Sign-extend
        switch (m_bytesPerPixel)
        {
        case 1:
            v = (char)v;
            break;
        case 2:
            v = (qint16)v;
            break;
        case 3:
            v = (qint32)v;
            break;
        }
        return v * m_bscale + m_bzero;
    }
    else
    {
        // Type-punning via unions apparently undefined behaviour in C++
        uint32_t i = (uint32_t)v;
        float f;
        memcpy(&f, &i, sizeof(f));
        return f;
    }
}

float FITS::scaledValue(int x, int y) const
{
    float v = value(x, y);
    return v * m_uintScale;
}

int FITS::mod(int a, int b) const
{
    return a - b * floor(a/(double)b);
}

float FITS::scaledWrappedValue(int x, int y) const
{
    float v = value(mod(x, m_width), mod(y, m_height));
    return v * m_uintScale;
}
