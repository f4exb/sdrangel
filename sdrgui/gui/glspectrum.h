///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef SDRGUI_GLSPECTRUM_H_
#define SDRGUI_GLSPECTRUM_H_

#include <QWidget>

#include "export.h"
#include "glspectrumview.h"

class QSplitter;
class SpectrumMeasurements;

// Combines GLSpectrumView with SpectrumMeasurements in a QSplitter
class SDRGUI_API GLSpectrum : public QWidget, public GLSpectrumInterface {
    Q_OBJECT

public:
    GLSpectrum(QWidget *parent = nullptr);
    GLSpectrumView *getSpectrumView() const { return m_spectrum; }
    SpectrumMeasurements *getMeasurements() const { return m_measurements; }
    void setMeasurementsVisible(bool visible);
    void setMeasurementsPosition(SpectrumSettings::MeasurementsPosition position);

    void setCenterFrequency(qint64 frequency) { m_spectrum->setCenterFrequency(frequency); }
    qint64 getCenterFrequency() const { return m_spectrum->getCenterFrequency(); }
    float getPowerMax() const { return m_spectrum->getPowerMax(); }
    float getTimeMax() const { return m_spectrum->getTimeMax(); }
    void setSampleRate(qint32 sampleRate) { m_spectrum->setSampleRate(sampleRate); }
    void setTimingRate(qint32 timingRate) { m_spectrum->setTimingRate(timingRate); }
    void setFFTOverlap(int overlap) { m_spectrum->setFFTOverlap(overlap); }
    void setReferenceLevel(Real referenceLevel) { m_spectrum->setReferenceLevel(referenceLevel); }
    void setPowerRange(Real powerRange){ m_spectrum->setPowerRange(powerRange); }
    void setDecay(int decay) { m_spectrum->setDecay(decay); }
    void setDecayDivisor(int decayDivisor) { m_spectrum->setDecayDivisor(decayDivisor); }
    void setHistoStroke(int stroke) { m_spectrum->setHistoStroke(stroke); }
    void setDisplayWaterfall(bool display) { m_spectrum->setDisplayWaterfall(display); }
    void setDisplay3DSpectrogram(bool display) { m_spectrum->setDisplay3DSpectrogram(display); }
    void set3DSpectrogramStyle(SpectrumSettings::SpectrogramStyle style) { m_spectrum->set3DSpectrogramStyle(style); }
    void setColorMapName(const QString &colorMapName) { m_spectrum->setColorMapName(colorMapName); }
    void setSpectrumStyle(SpectrumSettings::SpectrumStyle style) { m_spectrum->setSpectrumStyle(style); }
    void setSsbSpectrum(bool ssbSpectrum) { m_spectrum->setSsbSpectrum(ssbSpectrum); }
    void setLsbDisplay(bool lsbDisplay) { m_spectrum->setLsbDisplay(lsbDisplay); }
    void setInvertedWaterfall(bool inv) { m_spectrum->setInvertedWaterfall(inv); }
    void setDisplayMaxHold(bool display) { m_spectrum->setDisplayMaxHold(display); }
    void setDisplayCurrent(bool display) { m_spectrum->setDisplayCurrent(display); }
    void setDisplayHistogram(bool display) { m_spectrum->setDisplayHistogram(display); }
    void setDisplayGrid(bool display) { m_spectrum->setDisplayGrid(display); }
    void setDisplayGridIntensity(int intensity) { m_spectrum->setDisplayGridIntensity(intensity); }
    void setDisplayTraceIntensity(int intensity) { m_spectrum->setDisplayTraceIntensity(intensity); }
    void setFreqScaleTruncationMode(bool mode) { m_spectrum->setFreqScaleTruncationMode(mode); }
    void setLinear(bool linear) { m_spectrum->setLinear(linear); }
    void setUseCalibration(bool useCalibration) { m_spectrum->setUseCalibration(useCalibration); }
    void setMeasurementParams(SpectrumSettings::Measurement measurement,
                              int centerFrequencyOffset, int bandwidth, int chSpacing, int adjChBandwidth,
                              int harmonics, int peaks, bool highlight, int precision);
    qint32 getSampleRate() const { return m_spectrum->getSampleRate(); }
    void addChannelMarker(ChannelMarker* channelMarker) { m_spectrum->addChannelMarker(channelMarker); }
    void removeChannelMarker(ChannelMarker* channelMarker) { m_spectrum->removeChannelMarker(channelMarker); }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_spectrum->setMessageQueueToGUI(messageQueue); }
    void newSpectrum(const Real* spectrum, int nbBins, int fftSize) { m_spectrum->newSpectrum(spectrum, nbBins, fftSize); }
    void clearSpectrumHistogram() { m_spectrum->clearSpectrumHistogram(); }
    Real getWaterfallShare() const { return  m_spectrum->getWaterfallShare(); }
    void setWaterfallShare(Real waterfallShare) { m_spectrum->setWaterfallShare(waterfallShare); }
    void setFPSPeriodMs(int fpsPeriodMs) { m_spectrum->setFPSPeriodMs(fpsPeriodMs); }
    void setDisplayedStream(bool sourceOrSink, int streamIndex) { m_spectrum->setDisplayedStream(sourceOrSink, streamIndex); }
    void setSpectrumVis(SpectrumVis *spectrumVis) { m_spectrum->setSpectrumVis(spectrumVis); }
    SpectrumVis *getSpectrumVis() { return m_spectrum->getSpectrumVis(); }
    const QList<SpectrumHistogramMarker>& getHistogramMarkers() const { return m_spectrum->getHistogramMarkers(); }
    QList<SpectrumHistogramMarker>& getHistogramMarkers() { return m_spectrum->getHistogramMarkers(); }
    void setHistogramMarkers(const QList<SpectrumHistogramMarker>& histogramMarkers) { m_spectrum->setHistogramMarkers(histogramMarkers); }
    const QList<SpectrumWaterfallMarker>& getWaterfallMarkers() const { return m_spectrum->getWaterfallMarkers(); }
    QList<SpectrumWaterfallMarker>& getWaterfallMarkers() { return m_spectrum->getWaterfallMarkers(); }
    void setWaterfallMarkers(const QList<SpectrumWaterfallMarker>& waterfallMarkers) { m_spectrum->setWaterfallMarkers(waterfallMarkers); }
    const QList<SpectrumAnnotationMarker>& getAnnotationMarkers() const { return m_spectrum->getAnnotationMarkers(); }
    QList<SpectrumAnnotationMarker>& getAnnotationMarkers() { return m_spectrum->getAnnotationMarkers(); }
    void setAnnotationMarkers(const QList<SpectrumAnnotationMarker>& annotationMarkers) { m_spectrum->setAnnotationMarkers(annotationMarkers); };
    void updateHistogramMarkers() { m_spectrum->updateHistogramMarkers(); }
    void updateWaterfallMarkers() { m_spectrum->updateWaterfallMarkers(); }
    void updateAnnotationMarkers() { m_spectrum->updateAnnotationMarkers();}
    void updateMarkersDisplay() { m_spectrum->updateMarkersDisplay(); }
    void updateCalibrationPoints() { m_spectrum->updateCalibrationPoints(); }
    SpectrumSettings::MarkersDisplay& getMarkersDisplay() { return m_spectrum->getMarkersDisplay(); }
    bool& getHistogramFindPeaks() { return m_spectrum->getHistogramFindPeaks(); }
    void setHistogramFindPeaks(bool value) { m_spectrum->setHistogramFindPeaks(value); }
	void setMarkersDisplay(SpectrumSettings::MarkersDisplay markersDisplay) { m_spectrum->setMarkersDisplay(markersDisplay); }
    QList<SpectrumCalibrationPoint>& getCalibrationPoints() { return m_spectrum->getCalibrationPoints(); }
    void setCalibrationPoints(const QList<SpectrumCalibrationPoint>& calibrationPoints) { m_spectrum->setCalibrationPoints(calibrationPoints); }
    SpectrumSettings::CalibrationInterpolationMode& getCalibrationInterpMode() { return m_spectrum->getCalibrationInterpMode(); }
    void setCalibrationInterpMode(SpectrumSettings::CalibrationInterpolationMode mode) { m_spectrum->setCalibrationInterpMode(mode); }
    void setIsDeviceSpectrum(bool isDeviceSpectrum) { m_spectrum->setIsDeviceSpectrum(isDeviceSpectrum); }
    bool isDeviceSpectrum() const { return m_spectrum->isDeviceSpectrum(); }

private:
    QSplitter *m_splitter;
    GLSpectrumView *m_spectrum;
    SpectrumMeasurements *m_measurements;
    SpectrumSettings::MeasurementsPosition m_position;

};

#endif // SDRGUI_GLSPECTRUM_H_
