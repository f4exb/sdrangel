///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include <QBuffer>
#include <QDataStream>

#include "SWGGLSpectrum.h"

#include "util/simpleserializer.h"
#include "spectrumsettings.h"

SpectrumSettings::SpectrumSettings()
{
    resetToDefaults();
}

SpectrumSettings::~SpectrumSettings()
{}


void SpectrumSettings::resetToDefaults()
{
	m_fftSize = 1024;
	m_fftOverlap = 0;
	m_fftWindow = FFTWindow::Hanning;
	m_refLevel = 0;
	m_powerRange = 100;
	m_fpsPeriodMs = 50;
	m_decay = 1;
	m_decayDivisor = 1;
	m_histogramStroke = 30;
	m_displayGridIntensity = 5;
	m_displayTraceIntensity = 50;
	m_waterfallShare = 0.5;
	m_displayCurrent = true;
	m_displayWaterfall = true;
	m_invertedWaterfall = true;
    m_display3DSpectrogram = false;
	m_displayMaxHold = false;
	m_displayHistogram = false;
	m_displayGrid = false;
    m_truncateFreqScale = false;
	m_averagingMode = AvgModeNone;
	m_averagingIndex = 0;
    m_averagingValue = 1;
	m_linear = false;
    m_ssb = false;
    m_usb = true;
	m_wsSpectrum = false;
    m_wsSpectrumAddress = "127.0.0.1";
    m_wsSpectrumPort = 8887;
	m_markersDisplay = MarkersDisplayNone;
	m_useCalibration = false;
	m_calibrationInterpMode = CalibInterpLinear;
    m_3DSpectrogramStyle = Outline;
    m_colorMap = "Angel";
    m_spectrumStyle = Line;
    m_measurement = MeasurementNone;
    m_measurementCenterFrequencyOffset = 0;
    m_measurementBandwidth = 10000;
    m_measurementChSpacing = 10000;
    m_measurementAdjChBandwidth = 10000;
    m_measurementHarmonics = 5;
    m_measurementPeaks = 5;
    m_measurementHighlight = true;
    m_measurementsPosition = PositionBelow;
    m_measurementPrecision = 1;
    m_findHistogramPeaks = false;
#ifdef ANDROID
    m_showAllControls = false;
#else
    m_showAllControls = true;
#endif
}

QByteArray SpectrumSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_fftSize);
	s.writeS32(2, m_fftOverlap);
	s.writeS32(3, (int) m_fftWindow);
	s.writeReal(4, m_refLevel);
	s.writeReal(5, m_powerRange);
	s.writeBool(6, m_displayWaterfall);
	s.writeBool(7, m_invertedWaterfall);
	s.writeBool(8, m_displayMaxHold);
	s.writeBool(9, m_displayHistogram);
	s.writeS32(10, m_decay);
	s.writeBool(11, m_displayGrid);
	s.writeS32(13, m_displayGridIntensity);
	s.writeS32(14, m_decayDivisor);
	s.writeS32(15, m_histogramStroke);
	s.writeBool(16, m_displayCurrent);
	s.writeS32(17, m_displayTraceIntensity);
	s.writeReal(18, m_waterfallShare);
	s.writeS32(19, (int) m_averagingMode);
	s.writeS32(20, (qint32) getAveragingValue(m_averagingIndex, m_averagingMode));
	s.writeBool(21, m_linear);
    s.writeString(22, m_wsSpectrumAddress);
    s.writeU32(23, m_wsSpectrumPort);
    s.writeBool(24, m_ssb);
    s.writeBool(25, m_usb);
	s.writeS32(26, m_fpsPeriodMs);
	s.writeBool(27, m_wsSpectrum);
	s.writeS32(28, (int) m_markersDisplay);
	s.writeBool(29, m_useCalibration);
	s.writeS32(30, (int) m_calibrationInterpMode);
    s.writeBool(31, m_display3DSpectrogram);
    s.writeS32(32, (int) m_3DSpectrogramStyle);
    s.writeString(33, m_colorMap);
    s.writeS32(34, (int) m_spectrumStyle);
    s.writeS32(35, (int) m_measurement);
    s.writeS32(36, m_measurementBandwidth);
    s.writeS32(37, m_measurementChSpacing);
    s.writeS32(38, m_measurementAdjChBandwidth);
    s.writeS32(39, m_measurementHarmonics);
    // 41, 42 used below
    s.writeBool(42, m_measurementHighlight);
    s.writeS32(43, m_measurementPeaks);
    s.writeS32(44, (int)m_measurementsPosition);
    s.writeS32(45, m_measurementPrecision);
    s.writeS32(46, m_measurementCenterFrequencyOffset);
    s.writeBool(47, m_findHistogramPeaks);
    s.writeBool(48, m_truncateFreqScale);
    s.writeBool(49, m_showAllControls);
    s.writeS32(100, m_histogramMarkers.size());

	for (int i = 0; i < m_histogramMarkers.size(); i++) {
		s.writeBlob(101+i, m_histogramMarkers[i].serialize());
	}

	s.writeS32(110, m_waterfallMarkers.size());

	for (int i = 0; i < m_waterfallMarkers.size(); i++) {
		s.writeBlob(111+i, m_waterfallMarkers[i].serialize());
	}

    s.writeList(40, m_annoationMarkers);
    s.writeList(41, m_calibrationPoints);

	return s.final();
}

QDataStream& operator<<(QDataStream& out, const SpectrumAnnotationMarker& marker)
{
	out << marker.m_startFrequency;
	out << marker.m_bandwidth;
	out << marker.m_markerColor;
	out << (int) marker.m_show;
	out << marker.m_text;
	return out;
}

QDataStream& operator<<(QDataStream& out, const SpectrumCalibrationPoint& calibrationPoint)
{
	out << calibrationPoint.m_frequency;
	out << calibrationPoint.m_powerRelativeReference;
	out << calibrationPoint.m_powerCalibratedReference;
	return out;
}

bool SpectrumSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	int tmp;
    uint32_t utmp;
	QByteArray bytetmp;

	if (d.getVersion() == 1)
    {
		d.readS32(1, &m_fftSize, 1024);
		d.readS32(2, &m_fftOverlap, 0);
        d.readS32(3, &tmp, (int) FFTWindow::Hanning);
		m_fftWindow = (FFTWindow::Function) tmp;
		d.readReal(4, &m_refLevel, 0);
		d.readReal(5, &m_powerRange, 100);
		d.readBool(6, &m_displayWaterfall, true);
		d.readBool(7, &m_invertedWaterfall, true);
		d.readBool(8, &m_displayMaxHold, false);
		d.readBool(9, &m_displayHistogram, false);
		d.readS32(10, &m_decay, 1);
		d.readBool(11, &m_displayGrid, false);
		d.readS32(13, &m_displayGridIntensity, 5);
		d.readS32(14, &m_decayDivisor, 1);
		d.readS32(15, &m_histogramStroke, 30);
		d.readBool(16, &m_displayCurrent, true);
		d.readS32(17, &m_displayTraceIntensity, 50);
		d.readReal(18, &m_waterfallShare, 0.66);
		d.readS32(19, &tmp, 0);
		m_averagingMode = tmp < 0 ? AvgModeNone : tmp > 3 ? AvgModeMax : (AveragingMode) tmp;
		d.readS32(20, &tmp, 0);
		m_averagingIndex = getAveragingIndex(tmp, m_averagingMode);
	    m_averagingValue = getAveragingValue(m_averagingIndex, m_averagingMode);
	    d.readBool(21, &m_linear, false);
        d.readString(22, &m_wsSpectrumAddress, "127.0.0.1");
        d.readU32(23, &utmp, 8887);
        m_wsSpectrumPort = utmp < 1024 ? 1024 : utmp > 65535 ? 65535 : utmp;
        d.readBool(24, &m_ssb, false);
        d.readBool(25, &m_usb, true);
		d.readS32(26, &tmp, 50);
		m_fpsPeriodMs = tmp < 5 ? 5 : tmp > 500 ? 500 : tmp;
		d.readBool(27, &m_wsSpectrum, false);
		d.readS32(28, &tmp, 0);
		m_markersDisplay = (MarkersDisplay) tmp;
		d.readBool(29, &m_useCalibration, false);
		d.readS32(30, &tmp, 0);
		m_calibrationInterpMode = (CalibrationInterpolationMode) tmp;
        d.readBool(31, &m_display3DSpectrogram, false);
        d.readS32(32, (int*)&m_3DSpectrogramStyle, (int)Outline);
        d.readString(33, &m_colorMap, "Angel");
        d.readS32(34, (int*)&m_spectrumStyle, (int)Line);
        d.readS32(35, (int*)&m_measurement, (int)MeasurementNone);
        d.readS32(36, &m_measurementBandwidth, 10000);
        d.readS32(37, &m_measurementChSpacing, 10000);
        d.readS32(38, &m_measurementAdjChBandwidth, 10000);
        d.readS32(39, &m_measurementHarmonics, 5);
        d.readBool(42, &m_measurementHighlight, true);
        d.readS32(43, &m_measurementPeaks, 5);
        d.readS32(44, (int*)&m_measurementsPosition, (int)PositionBelow);
        d.readS32(45, &m_measurementPrecision, 1);
        d.readS32(46, &m_measurementCenterFrequencyOffset, 0);
        d.readBool(47, &m_findHistogramPeaks, false);
        d.readBool(48, &m_truncateFreqScale, false);
#ifdef ANDROID
        d.readBool(49, &m_showAllControls, false);
#else
        d.readBool(49, &m_showAllControls, true);
#endif

		int histogramMarkersSize;
		d.readS32(100, &histogramMarkersSize, 0);
		histogramMarkersSize = histogramMarkersSize < 0 ? 0 :
			histogramMarkersSize > SpectrumHistogramMarker::m_maxNbOfMarkers ?
				SpectrumHistogramMarker::m_maxNbOfMarkers : histogramMarkersSize;
		m_histogramMarkers.clear();

		for (int i = 0; i < histogramMarkersSize; i++)
		{
			d.readBlob(101+i, &bytetmp);
			m_histogramMarkers.push_back(SpectrumHistogramMarker());
			m_histogramMarkers.back().deserialize(bytetmp);
		}

		int waterfallMarkersSize;

		d.readS32(110, &waterfallMarkersSize, 0);
		waterfallMarkersSize = waterfallMarkersSize < 0 ? 0 :
			waterfallMarkersSize > SpectrumWaterfallMarker::m_maxNbOfMarkers ?
				SpectrumWaterfallMarker::m_maxNbOfMarkers : waterfallMarkersSize;
		m_waterfallMarkers.clear();

		for (int i = 0; i < waterfallMarkersSize; i++)
		{
			d.readBlob(111+i, &bytetmp);
			m_waterfallMarkers.push_back(SpectrumWaterfallMarker());
			m_waterfallMarkers.back().deserialize(bytetmp);
		}

        d.readList(40, &m_annoationMarkers);
        d.readList(41, &m_calibrationPoints);

		return true;
	}
    else
    {
		resetToDefaults();
		return false;
	}
}

QDataStream& operator>>(QDataStream& in, SpectrumAnnotationMarker& marker)
{
	int tmp;
    in >> marker.m_startFrequency;
    in >> marker.m_bandwidth;
    in >> marker.m_markerColor;
    in >> tmp;
    in >> marker.m_text;
	marker.m_show = (SpectrumAnnotationMarker::ShowState) tmp;
    return in;
}

QDataStream& operator>>(QDataStream& in, SpectrumCalibrationPoint& calibrationPoint)
{
    in >> calibrationPoint.m_frequency;
    in >> calibrationPoint.m_powerRelativeReference;
    in >> calibrationPoint.m_powerCalibratedReference;
    return in;
}

void SpectrumSettings::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
	SWGSDRangel::SWGGLSpectrum *swgSpectrum = static_cast<SWGSDRangel::SWGGLSpectrum *>(swgObject);

	swgSpectrum->setFftWindow((int) m_fftWindow);
    swgSpectrum->setFftSize(m_fftSize);
    swgSpectrum->setFftOverlap(m_fftOverlap);
    swgSpectrum->setAveragingMode((int) m_averagingMode);
    swgSpectrum->setAveragingValue(SpectrumSettings::getAveragingValue(m_averagingIndex, m_averagingMode));
	swgSpectrum->setRefLevel(m_refLevel);
	swgSpectrum->setPowerRange(m_powerRange);
    swgSpectrum->setFpsPeriodMs(m_fpsPeriodMs);
	swgSpectrum->setLinear(m_linear ? 1 : 0);
	swgSpectrum->setWsSpectrum(m_wsSpectrum ? 1 : 0);
	swgSpectrum->setWsSpectrumPort(m_wsSpectrumPort);

    if (swgSpectrum->getWsSpectrumAddress()) {
        *swgSpectrum->getWsSpectrumAddress() = m_wsSpectrumAddress;
    } else {
        swgSpectrum->setWsSpectrumAddress(new QString(m_wsSpectrumAddress));
    }

    swgSpectrum->setDisplayHistogram(m_displayHistogram ? 1 : 0);
    swgSpectrum->setDecay(m_decay);
    swgSpectrum->setDecayDivisor(m_decayDivisor);
	swgSpectrum->setHistogramStroke(m_histogramStroke);
    swgSpectrum->setDisplayMaxHold(m_displayMaxHold ? 1 : 0);
    swgSpectrum->setDisplayCurrent(m_displayCurrent ? 1 : 0);
    swgSpectrum->setDisplayTraceIntensity(m_displayTraceIntensity);
	swgSpectrum->setInvertedWaterfall(m_invertedWaterfall ? 1 : 0);
    swgSpectrum->setDisplayWaterfall(m_displayWaterfall ? 1 : 0);
    swgSpectrum->setDisplayGrid(m_displayGrid ? 1 : 0);
    swgSpectrum->setDisplayGridIntensity(m_displayGridIntensity);
	swgSpectrum->setSsb(m_ssb ? 1 : 0);
	swgSpectrum->setUsb(m_usb ? 1 : 0);
	swgSpectrum->setWaterfallShare(m_waterfallShare);
	swgSpectrum->setMarkersDisplay((int) m_markersDisplay);
	swgSpectrum->setUseCalibration(m_useCalibration ? 1 : 0);
	swgSpectrum->setCalibrationInterpMode((int) m_calibrationInterpMode);

	if (m_histogramMarkers.size() > 0)
	{
		swgSpectrum->setHistogramMarkers(new QList<SWGSDRangel::SWGSpectrumHistogramMarker *>);

		for (const auto &marker : m_histogramMarkers)
		{
			swgSpectrum->getHistogramMarkers()->append(new SWGSDRangel::SWGSpectrumHistogramMarker);
			swgSpectrum->getHistogramMarkers()->back()->setFrequency(marker.m_frequency);
			swgSpectrum->getHistogramMarkers()->back()->setPower(marker.m_power);
			swgSpectrum->getHistogramMarkers()->back()->setMarkerType((int) marker.m_markerType);
			swgSpectrum->getHistogramMarkers()->back()->setMarkerColor(qColorToInt(marker.m_markerColor));
			swgSpectrum->getHistogramMarkers()->back()->setShow(marker.m_show ? 1 : 0);
		}
	}

	if (m_waterfallMarkers.size() > 0)
	{
		swgSpectrum->setWaterfallMarkers(new QList<SWGSDRangel::SWGSpectrumWaterfallMarker *>);

		for (const auto &marker : m_waterfallMarkers)
		{
			swgSpectrum->getWaterfallMarkers()->append(new SWGSDRangel::SWGSpectrumWaterfallMarker);
			swgSpectrum->getWaterfallMarkers()->back()->setFrequency(marker.m_frequency);
			swgSpectrum->getWaterfallMarkers()->back()->setTime(marker.m_time);
			swgSpectrum->getWaterfallMarkers()->back()->setMarkerColor(qColorToInt(marker.m_markerColor));
			swgSpectrum->getWaterfallMarkers()->back()->setShow(marker.m_show ? 1 : 0);
		}
	}

	if (m_annoationMarkers.size() > 0)
	{
		swgSpectrum->setAnnotationMarkers(new QList<SWGSDRangel::SWGSpectrumAnnotationMarker *>);

		for (const auto &marker : m_annoationMarkers)
		{
			swgSpectrum->getAnnotationMarkers()->append(new SWGSDRangel::SWGSpectrumAnnotationMarker);
			swgSpectrum->getAnnotationMarkers()->back()->setStartFrequency(marker.m_startFrequency);
			swgSpectrum->getAnnotationMarkers()->back()->setBandwidth(marker.m_bandwidth);
			swgSpectrum->getAnnotationMarkers()->back()->setMarkerColor(qColorToInt(marker.m_markerColor));
			swgSpectrum->getAnnotationMarkers()->back()->setShow((int) marker.m_show);
		}
	}

	if (m_calibrationPoints.size() > 0)
	{
		swgSpectrum->setCalibrationPoints(new QList<SWGSDRangel::SWGSpectrumCalibrationPoint *>);

		for (const auto &calibrationPoint : m_calibrationPoints)
		{
			swgSpectrum->getCalibrationPoints()->append(new SWGSDRangel::SWGSpectrumCalibrationPoint);
			swgSpectrum->getCalibrationPoints()->back()->setFrequency(calibrationPoint.m_frequency);
			swgSpectrum->getCalibrationPoints()->back()->setPowerRelativeReference(calibrationPoint.m_powerRelativeReference);
			swgSpectrum->getCalibrationPoints()->back()->setPowerAbsoluteReference(calibrationPoint.m_powerCalibratedReference);
		}
	}
}

void SpectrumSettings::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
	SWGSDRangel::SWGGLSpectrum *swgSpectrum =
		static_cast<SWGSDRangel::SWGGLSpectrum *>(const_cast<SWGSDRangel::SWGObject *>(swgObject));

	if (keys.contains("spectrumConfig.fftWindow")) {
		m_fftWindow = (FFTWindow::Function) swgSpectrum->getFftWindow();
	}
	if (keys.contains("spectrumConfig.fftSize")) {
		m_fftSize = swgSpectrum->getFftSize();
	}
	if (keys.contains("spectrumConfig.fftOverlap")) {
		m_fftOverlap = swgSpectrum->getFftOverlap();
	}
	if (keys.contains("spectrumConfig.averagingMode")) {
		m_averagingMode = (SpectrumSettings::AveragingMode) swgSpectrum->getAveragingMode();
	}
	if (keys.contains("spectrumConfig.averagingValue"))
	{
		m_averagingValue = swgSpectrum->getAveragingValue();
		m_averagingIndex = SpectrumSettings::getAveragingIndex(m_averagingValue, m_averagingMode);
	}
	if (keys.contains("spectrumConfig.refLevel")) {
		m_refLevel = swgSpectrum->getRefLevel();
	}
	if (keys.contains("spectrumConfig.powerRange")) {
		m_powerRange = swgSpectrum->getPowerRange();
	}
	if (keys.contains("spectrumConfig.fpsPeriodMs")) {
		m_fpsPeriodMs = swgSpectrum->getFpsPeriodMs();
	}
	if (keys.contains("spectrumConfig.linear")) {
		m_linear = swgSpectrum->getLinear() != 0;
	}
	if (keys.contains("spectrumConfig.wsSpectrum")) {
		m_wsSpectrum = swgSpectrum->getWsSpectrum() != 0;
	}
	if (keys.contains("spectrumConfig.wsSpectrum")) {
		m_wsSpectrum = swgSpectrum->getWsSpectrum() != 0;
	}
    if (keys.contains("spectrumConfig.wsSpectrumAddress")) {
		m_wsSpectrumAddress = *swgSpectrum->getWsSpectrumAddress();
    }
	if (keys.contains("spectrumConfig.wsSpectrumPort")) {
		m_wsSpectrumPort = swgSpectrum->getWsSpectrumPort();
	}
	if (keys.contains("spectrumConfig.displayHistogram")) {
		m_displayHistogram = swgSpectrum->getDisplayHistogram() != 0;
	}
	if (keys.contains("spectrumConfig.decay")) {
		m_decay = swgSpectrum->getDecay();
	}
	if (keys.contains("spectrumConfig.decayDivisor")) {
		m_decayDivisor = swgSpectrum->getDecayDivisor();
	}
	if (keys.contains("spectrumConfig.histogramStroke")) {
		m_histogramStroke = swgSpectrum->getHistogramStroke();
	}
	if (keys.contains("spectrumConfig.displayMaxHold")) {
		m_displayMaxHold = swgSpectrum->getDisplayMaxHold() != 0;
	}
	if (keys.contains("spectrumConfig.displayCurrent")) {
		m_displayCurrent = swgSpectrum->getDisplayCurrent() != 0;
	}
	if (keys.contains("spectrumConfig.displayTraceIntensity")) {
		m_displayTraceIntensity = swgSpectrum->getDisplayTraceIntensity();
	}
	if (keys.contains("spectrumConfig.invertedWaterfall")) {
		m_invertedWaterfall = swgSpectrum->getInvertedWaterfall() != 0;
	}
	if (keys.contains("spectrumConfig.displayWaterfall")) {
		m_displayWaterfall = swgSpectrum->getDisplayWaterfall() != 0;
	}
	if (keys.contains("spectrumConfig.displayGrid")) {
		m_displayGrid = swgSpectrum->getDisplayGrid() != 0;
	}
	if (keys.contains("spectrumConfig.displayGridIntensity")) {
		m_displayGridIntensity = swgSpectrum->getDisplayGridIntensity();
	}
	if (keys.contains("spectrumConfig.ssb")) {
		m_ssb = swgSpectrum->getSsb() != 0;
	}
	if (keys.contains("spectrumConfig.usb")) {
		m_usb = swgSpectrum->getUsb() != 0;
	}
	if (keys.contains("spectrumConfig.waterfallShare")) {
		m_waterfallShare = swgSpectrum->getWaterfallShare();
	}
	if (keys.contains("spectrumConfig.markersDisplay")) {
		m_markersDisplay = (SpectrumSettings::MarkersDisplay) swgSpectrum->getMarkersDisplay();
	}
	if (keys.contains("spectrumConfig.useCalibration")) {
		m_useCalibration = swgSpectrum->getUseCalibration() != 0;
	}
	if (keys.contains("spectrumConfig.calibrationInterpMode")) {
		m_calibrationInterpMode = (CalibrationInterpolationMode) swgSpectrum->getCalibrationInterpMode();
	}

	if (keys.contains("spectrumConfig.histogramMarkers"))
	{
        QList<SWGSDRangel::SWGSpectrumHistogramMarker *> *swgHistogramMarkers = swgSpectrum->getHistogramMarkers();
        m_histogramMarkers.clear();
		int i = 0;

		for (const auto &swgHistogramMarker : *swgHistogramMarkers)
		{
			m_histogramMarkers.push_back(SpectrumHistogramMarker());
			m_histogramMarkers.back().m_frequency = swgHistogramMarker->getFrequency();
			m_histogramMarkers.back().m_power = swgHistogramMarker->getPower();
			m_histogramMarkers.back().m_markerType = (SpectrumHistogramMarker::SpectrumMarkerType) swgHistogramMarker->getMarkerType();
			m_histogramMarkers.back().m_markerColor = intToQColor(swgHistogramMarker->getMarkerColor());
			m_histogramMarkers.back().m_show = swgHistogramMarker->getShow() != 0;

			if (i++ == 10) { // no more than 10 markers
				break;
			}
		}
	}

	if (keys.contains("spectrumConfig.waterfallMarkers"))
	{
        QList<SWGSDRangel::SWGSpectrumWaterfallMarker *> *swgWaterfallMarkers = swgSpectrum->getWaterfallMarkers();
        m_waterfallMarkers.clear();
		int i = 0;

		for (const auto &swgWaterfallMarker : *swgWaterfallMarkers)
		{
			m_waterfallMarkers.push_back(SpectrumWaterfallMarker());
			m_waterfallMarkers.back().m_frequency = swgWaterfallMarker->getFrequency();
			m_waterfallMarkers.back().m_time = swgWaterfallMarker->getTime();
			m_waterfallMarkers.back().m_markerColor = intToQColor(swgWaterfallMarker->getMarkerColor());
			m_waterfallMarkers.back().m_show = swgWaterfallMarker->getShow() != 0;

			if (i++ == 10) { // no more than 10 markers
				break;
			}
		}
	}

	if (keys.contains("spectrumConfig.annotationMarkers"))
	{
        QList<SWGSDRangel::SWGSpectrumAnnotationMarker *> *swgAnnotationMarkers = swgSpectrum->getAnnotationMarkers();
        m_waterfallMarkers.clear();

		for (const auto &swgAnnotationMarker : *swgAnnotationMarkers)
		{
			m_annoationMarkers.push_back(SpectrumAnnotationMarker());
			m_annoationMarkers.back().m_startFrequency = swgAnnotationMarker->getStartFrequency();
			m_annoationMarkers.back().m_bandwidth = swgAnnotationMarker->getBandwidth() < 0 ? 0 : swgAnnotationMarker->getBandwidth();
			m_annoationMarkers.back().m_markerColor = intToQColor(swgAnnotationMarker->getMarkerColor());
			m_annoationMarkers.back().m_show = (SpectrumAnnotationMarker::ShowState) swgAnnotationMarker->getShow();
		}
	}

	if (keys.contains("spectrumConfig.calibrationPoints"))
	{
		QList<SWGSDRangel::SWGSpectrumCalibrationPoint *> *swgCalibrationPoints = swgSpectrum->getCalibrationPoints();
		m_calibrationPoints.clear();

		for (const auto &swgCalibrationPoint : *swgCalibrationPoints)
		{
			m_calibrationPoints.push_back(SpectrumCalibrationPoint());
			m_calibrationPoints.back().m_frequency = swgCalibrationPoint->getFrequency();
			m_calibrationPoints.back().m_powerRelativeReference = swgCalibrationPoint->getPowerRelativeReference();
			m_calibrationPoints.back().m_powerCalibratedReference = swgCalibrationPoint->getPowerAbsoluteReference();
		}
	}
}

int SpectrumSettings::getAveragingMaxScale(AveragingMode averagingMode)
{
    if (averagingMode == AvgModeMoving) {
        return 3; // max 10k
    } else {
        return 5; // max 1M
    }
}

int SpectrumSettings::getAveragingValue(int averagingIndex, AveragingMode averagingMode)
{
    if (averagingIndex <= 0) {
        return 1;
    }

    int v = averagingIndex - 1;
    int m = pow(10.0, v/3 > getAveragingMaxScale(averagingMode) ? getAveragingMaxScale(averagingMode) : v/3);
    int x = 1;

    if (v % 3 == 0) {
        x = 2;
    } else if (v % 3 == 1) {
        x = 5;
    } else if (v % 3 == 2) {
        x = 10;
    }

    return x * m;
}

int SpectrumSettings::getAveragingIndex(int averagingValue, AveragingMode averagingMode)
{
    if (averagingValue <= 1) {
        return 0;
    }

    int v = averagingValue;
    int j = 0;

    for (int i = 0; i <= getAveragingMaxScale(averagingMode); i++)
    {
        if (v < 20)
        {
            if (v < 2) {
                j = 0;
            } else if (v < 5) {
                j = 1;
            } else if (v < 10) {
                j = 2;
            } else {
                j = 3;
            }

            return 3*i + j;
        }

        v /= 10;
    }

    return 3*getAveragingMaxScale(averagingMode) + 3;
}

uint64_t SpectrumSettings::getMaxAveragingValue(int fftSize, AveragingMode averagingMode)
{
	if (averagingMode == AvgModeMoving)
	{
		uint64_t limit = (1UL<<28) / (sizeof(double)*fftSize); // 256 MB max
		return limit > (1<<14) ? (1<<14) : limit; // limit to 16 kS anyway
	}
	else
	{
		return (1<<20); // fixed 1 MS
	}
}

int SpectrumSettings::qColorToInt(const QColor& color)
{
    return 256*256*color.blue() + 256*color.green() + color.red();
}

QColor SpectrumSettings::intToQColor(int intColor)
{
    int r = intColor % 256;
    int bg = intColor / 256;
    int g = bg % 256;
    int b = bg / 256;
    return QColor(r, g, b);
}
