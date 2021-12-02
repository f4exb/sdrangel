///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_CHANNELMARKER_H
#define INCLUDE_CHANNELMARKER_H

#include <QObject>
#include <QColor>
#include <QByteArray>

#include "settings/serializable.h"
#include "export.h"

class SDRBASE_API ChannelMarker : public QObject, public Serializable {
	Q_OBJECT

public:
	typedef enum sidebands_e
	{
		dsb,
		lsb,
		usb,
		vusb, //!< USB with vestigial LSB
		vlsb  //!< LSB with vestigial USB
	} sidebands_t;

	typedef enum frequencyScaleDisplay_e
	{
	    FScaleDisplay_freq,
	    FScaleDisplay_title,
	    FScaleDisplay_addressSend,
	    FScaleDisplay_addressReceive,
	    FScaleDisplay_none
	} frequencyScaleDisplay_t;

	ChannelMarker(QObject* parent = NULL);

	void emitChangedByAPI();

	void setTitle(const QString& title);
	const QString& getTitle() const { return m_title; }

	void setCenterFrequency(int centerFrequency);
    void setCenterFrequencyByCursor(int centerFrequency);
	int getCenterFrequency() const { return m_centerFrequency; }

	void setBandwidth(int bandwidth);
	int getBandwidth() const { return m_bandwidth; }

    void setOppositeBandwidth(int bandwidth);
    int getOppositeBandwidth() const { return m_oppositeBandwidth; }

	void setLowCutoff(int lowCutoff);
	int getLowCutoff() const { return m_lowCutoff; }

	void setSidebands(sidebands_t sidebands);
	sidebands_t getSidebands() const { return m_sidebands; }

	void setVisible(bool visible);
	bool getVisible() const { return m_visible; }

	void setHighlighted(bool highlighted);
    void setHighlightedByCursor(bool highlighted);
	bool getHighlighted() const { return m_highlighted; }

	void setColor(const QColor& color);
	const QColor& getColor() const { return m_color; }

	void setMovable(bool movable) { m_movable = movable; }
	bool getMovable() const { return m_movable; }

    void setFrequencyScaleDisplayType(frequencyScaleDisplay_t type) { m_fScaleDisplayType = type; }
    frequencyScaleDisplay_t getFrequencyScaleDisplayType() const { return m_fScaleDisplayType; }

    const QString& getDisplayAddressSend() const { return m_displayAddressSend; }
    const QString& getDisplayAddressReceive() const { return m_displayAddressReceive; }

    void setSourceOrSinkStream(bool sourceOrSinkStream) { m_sourceOrSinkStream = sourceOrSinkStream; }
    bool getSourceOrSinkStream() const { return m_sourceOrSinkStream; }
    void addStreamIndex(int streamIndex);
    void removeStreamIndex(int streamIndex);
    void clearStreamIndexes();

    bool streamIndexApplies(int streamIndex) const {
        return m_enabledStreamsBits & (1<<streamIndex);
    }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);
	void updateSettings(const ChannelMarker *channelMarker);

protected:
	static QRgb m_colorTable[];
	static int m_nextColor;

	QString m_title;
	QString m_displayAddressSend;
    QString m_displayAddressReceive;
	int m_centerFrequency;
	int m_bandwidth;
    int m_oppositeBandwidth;
	int m_lowCutoff;
	sidebands_t m_sidebands;
	bool m_visible;
	bool m_highlighted;
	QColor m_color;
	bool m_movable;
    frequencyScaleDisplay_t m_fScaleDisplayType;
    bool m_sourceOrSinkStream;
    uint32_t m_enabledStreamsBits;

    void resetToDefaults();

signals:
	void changedByAPI();
    void changedByCursor();
    void highlightedByCursor();
};

#endif // INCLUDE_CHANNELMARKER_H
