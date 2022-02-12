///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRGUI_DSP_SCOPEVISXY_H_
#define SDRGUI_DSP_SCOPEVISXY_H_

#include "dsp/basebandsamplesink.h"
#include "export.h"
#include "util/messagequeue.h"

#include <QObject>
#include <QColor>
#include <vector>
#include <complex>

class TVScreen;

class SDRGUI_API ScopeVisXY : public QObject, public BasebandSampleSink {
	Q_OBJECT
public:
	ScopeVisXY(TVScreen *tvScreen);
	virtual ~ScopeVisXY();

	using BasebandSampleSink::feed;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

	void setScale(float scale) { m_scale = scale; }
	void setStroke(int stroke) { m_alphaTrace = stroke; }
    void setDecay(int decay) { m_alphaReset = 255 - decay; }

	void setPixelsPerFrame(int pixelsPerFrame);
	void setPlotRGB(const QRgb& plotRGB) { m_plotRGB = plotRGB; }
	void setGridRGB(const QRgb& gridRGB) { m_gridRGB = gridRGB; }

	void addGraticulePoint(const std::complex<float>& z);
	void calculateGraticule(int rows, int cols);
	void clearGraticule();

private:
	virtual bool handleMessage(const Message& message);
	void drawGraticule();

	TVScreen *m_tvScreen;
	float m_scale;
	int m_cols;
	int m_rows;
	int m_pixelsPerFrame;
	int m_pixelCount;
	int m_alphaTrace;  //!< this is the stroke value [0:255]
	int m_alphaReset;  //!< alpha channel of screen blanking (blackening) is 255 minus decay value [0:255]
	QRgb m_plotRGB;
	QRgb m_gridRGB;
	std::vector<std::complex<float> > m_graticule;
	std::vector<int> m_graticuleRows;
	std::vector<int> m_graticuleCols;
	MessageQueue m_inputMessageQueue;

private slots:
	void handleInputMessages();
};


#endif /* SDRGUI_DSP_SCOPEVISXY_H_ */
