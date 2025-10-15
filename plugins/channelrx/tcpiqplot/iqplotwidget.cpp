#include "iqplotwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMutexLocker>
#include <QPixmap>
#include <QDebug>
#include <cmath>
#include <complex>
#include <algorithm>
#include "tcpiqplot.h"
IqPlotWidget::IqPlotWidget(QWidget *parent) : 
    QWidget(parent),
    m_plotMode(MODE_TIME_DOMAIN),
    m_autoScale(true),
    m_xMin(0.0f),
    m_xMax(1.0f),
    m_yMin(-1.0f),
    m_yMax(1.0f),
    m_sampleRate(1e6),        // Default 1 MHz
    m_centerFrequency(100e6),  // Default 100 MHz
    m_fftZoomFactor(10.0),     // Show signal ± 10x sample rate for context
    m_persistenceEnabled(false)
{
    setMinimumSize(400, 200);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSweepRange(1e9 , 2e9 , 20e6 ,64 );
}

IqPlotWidget::~IqPlotWidget()
{
}

void IqPlotWidget::setCustomXData(const QVector<float>& xData)
{
    QMutexLocker locker(&m_mutex);
    m_customX = xData;
    update();
}

void IqPlotWidget::setCustomYData(const QVector<float>& yData)
{
    QMutexLocker locker(&m_mutex);
    m_customY = yData;
    update();
}

void IqPlotWidget::setXAxisRange(float min, float max)
{
    m_xMin = min;
    m_xMax = max;
    m_autoScale = false;
    update();
}

void IqPlotWidget::setYAxisRange(float min, float max)
{
    m_yMin = min;
    m_yMax = max;
    m_autoScale = false;
    update();
}

void IqPlotWidget::onNewData(const QVector<float>& iData, const QVector<float>& qData)
{
    static int dataCount = 0;
    dataCount++;
    
    
    QMutexLocker locker(&m_mutex);
    m_iData = iData;
    m_qData = qData;
    update(); // Schedule a repaint
    
}

void IqPlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    static int paintCount = 0;
    paintCount++;
    
    
    QPainter painter(this);
    
    // Handle persistence mode for FFT spectrum
    if (m_persistenceEnabled && m_plotMode == MODE_FFT_SPECTRUM) {
        // Initialize persistence buffer if needed
        if (m_persistenceBuffer.isNull() || m_persistenceBuffer.size() != size()) {
            m_persistenceBuffer = QPixmap(size());
            m_persistenceBuffer.fill(Qt::black);
        }
        
        // Draw old persistence data first
        painter.drawPixmap(0, 0, m_persistenceBuffer);
        
        // Fade old data slightly (optional - creates decay effect)
        QPainter bufferPainter(&m_persistenceBuffer);
        bufferPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        bufferPainter.fillRect(m_persistenceBuffer.rect(), QColor(0, 0, 0, 250)); // 250/255 = 98% opacity (slight fade)
    } else {
        painter.fillRect(rect(), Qt::black);
    }
    
    painter.setRenderHint(QPainter::Antialiasing);

    QMutexLocker locker(&m_mutex);
    
  

    // Draw grid and axes
    drawGrid(painter);
    drawAxes(painter);

    // Draw based on plot mode
    switch (m_plotMode) {
        case MODE_TIME_DOMAIN:
            drawTimeMode(painter);
            break;
        case MODE_MAGNITUDE:
            drawMagnitudeMode(painter);
            break;
        case MODE_PHASE:
            drawPhaseMode(painter);
            break;
        case MODE_CONSTELLATION:
            drawConstellationMode(painter);
            break;
        case MODE_FFT_SPECTRUM:
            drawFFTSpectrumMode(painter);
            break;
        case MODE_CUSTOM_XY:
            drawCustomXYMode(painter);
            break;
    }
    
    // Save current frame to persistence buffer for FFT mode
    if (m_persistenceEnabled && m_plotMode == MODE_FFT_SPECTRUM) {
        if (!m_persistenceBuffer.isNull()) {
            QPainter bufferPainter(&m_persistenceBuffer);
            bufferPainter.setCompositionMode(QPainter::CompositionMode_Plus); // Additive blending
            bufferPainter.drawPixmap(0, 0, grab());
        }
    }
}

void IqPlotWidget::drawGrid(QPainter& painter)
{
    QPen gridPen(QColor(50, 50, 50));
    gridPen.setStyle(Qt::DotLine);
    painter.setPen(gridPen);

    // Draw vertical grid lines
    for (int i = 1; i < 10; i++) {
        int x = (width() * i) / 10;
        painter.drawLine(x, 0, x, height());
    }

    // Draw horizontal grid lines
    for (int i = 1; i < 10; i++) {
        int y = (height() * i) / 10;
        painter.drawLine(0, y, width(), y);
    }
}

void IqPlotWidget::drawAxes(QPainter& painter)
{
    QPen axisPen(Qt::white);
    axisPen.setWidth(2);
    painter.setPen(axisPen);

    // Draw center horizontal line
    int centerY = height() / 2;
    painter.drawLine(0, centerY, width(), centerY);

    // Draw center vertical line (for constellation mode)
    if (m_plotMode == MODE_CONSTELLATION) {
        int centerX = width() / 2;
        painter.drawLine(centerX, 0, centerX, height());
    }
    
    // Draw axis labels based on plot mode
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    
    // Left axis labels (Y-axis)
    if (m_plotMode == MODE_FFT_SPECTRUM) {
        // Power in dB: -100 to 0
        painter.drawText(5, 15, "0 dB");
        painter.drawText(5, height() - 5, "-100 dB");
    } else if (m_plotMode == MODE_CONSTELLATION) {
        // Q axis: -1 to 1
        painter.drawText(5, 15, "1.0");
        painter.drawText(5, height() / 2 + 15, "0.0");
        painter.drawText(5, height() - 5, "-1.0");
    } else if (m_plotMode == MODE_PHASE) {
        // Phase: -π to π
        painter.drawText(5, 15, "π");
        painter.drawText(5, height() / 2 + 15, "0");
        painter.drawText(5, height() - 5, "-π");
    } else {
        // Time domain, magnitude: -1 to 1 (or 0 to 1 for magnitude)
        if (m_plotMode == MODE_MAGNITUDE) {
            painter.drawText(5, 15, "1.0");
            painter.drawText(5, height() - 5, "0.0");
        } else {
            painter.drawText(5, 15, "1.0");
            painter.drawText(5, height() / 2 + 15, "0.0");
            painter.drawText(5, height() - 5, "-1.0");
        }
    }
    
    // Bottom axis labels (X-axis)
    if (m_plotMode == MODE_FFT_SPECTRUM) {
        // Calculate display frequency range (zoomed view)
        double displaySpan = m_sampleRate * m_fftZoomFactor;
        double displayMinFreq = m_centerFrequency - displaySpan / 2.0;
        double displayMaxFreq = m_centerFrequency + displaySpan / 2.0;
        
        // Clamp to 0-6 GHz
        if (displayMinFreq < 0) displayMinFreq = 0;
        if (displayMaxFreq > 6e9) displayMaxFreq = 6e9;
        
        // Format frequency in MHz or GHz
        auto formatFreq = [](double freq) -> QString {
            if (freq >= 1e9) {
                return QString::number(freq / 1e9, 'f', 3) + " GHz";
            } else if (freq >= 1e6) {
                return QString::number(freq / 1e6, 'f', 2) + " MHz";
            } else if (freq >= 1e3) {
                return QString::number(freq / 1e3, 'f', 1) + " kHz";
            } else {
                return QString::number(freq, 'f', 0) + " Hz";
            }
        };
        
        // Show zoomed frequency range
        painter.drawText(5, height() - 5, formatFreq(displayMinFreq));
        painter.drawText(width() / 2 - 40, height() - 5, formatFreq(m_centerFrequency));
        painter.drawText(width() - 100, height() - 5, formatFreq(displayMaxFreq));
    } else if (m_plotMode == MODE_CONSTELLATION) {
        // I axis: -1 to 1
        painter.drawText(5, height() - 5, "-1.0");
        painter.drawText(width() / 2 - 10, height() - 5, "0.0");
        painter.drawText(width() - 35, height() - 5, "1.0");
    } else {
        // Time (samples)
        painter.drawText(5, height() - 5, "0");
        if (!m_iData.isEmpty()) {
            painter.drawText(width() - 60, height() - 5, QString::number(m_iData.size()));
        }
    }
}

void IqPlotWidget::drawTimeMode(QPainter& painter)
{
    if (m_iData.isEmpty() || m_qData.isEmpty()) return;

    int numSamples = qMin(m_iData.size(), m_qData.size());
    if (numSamples == 0) return;

    // Draw I data in green
    QPen iPen(Qt::green);
    iPen.setWidth(2);
    painter.setPen(iPen);

    QPolygonF iPolyline;
    for (int i = 0; i < numSamples; ++i) {
        float x = (float)i / (numSamples - 1) * width();
        float y = (0.5f - m_iData[i] * 0.45f) * height();
        iPolyline << QPointF(x, y);
    }
    painter.drawPolyline(iPolyline);

    // Draw Q data in red
    QPen qPen(Qt::red);
    qPen.setWidth(2);
    painter.setPen(qPen);

    QPolygonF qPolyline;
    for (int i = 0; i < numSamples; ++i) {
        float x = (float)i / (numSamples - 1) * width();
        float y = (0.5f - m_qData[i] * 0.45f) * height();
        qPolyline << QPointF(x, y);
    }
    painter.drawPolyline(qPolyline);
}

void IqPlotWidget::drawMagnitudeMode(QPainter& painter)
{
    if (m_iData.isEmpty() || m_qData.isEmpty()) return;

    int numSamples = qMin(m_iData.size(), m_qData.size());
    if (numSamples == 0) return;

    QPen magPen(Qt::cyan);
    magPen.setWidth(2);
    painter.setPen(magPen);

    QPolygonF magPolyline;
    for (int i = 0; i < numSamples; ++i) {
        float magnitude = std::sqrt(m_iData[i] * m_iData[i] + m_qData[i] * m_qData[i]);
        float x = (float)i / (numSamples - 1) * width();
        float y = (1.0f - magnitude) * height();
        magPolyline << QPointF(x, y);
    }
    painter.drawPolyline(magPolyline);
}

void IqPlotWidget::drawPhaseMode(QPainter& painter)
{
    if (m_iData.isEmpty() || m_qData.isEmpty()) return;

    int numSamples = qMin(m_iData.size(), m_qData.size());
    if (numSamples == 0) return;

    QPen phasePen(Qt::yellow);
    phasePen.setWidth(2);
    painter.setPen(phasePen);

    QPolygonF phasePolyline;
    for (int i = 0; i < numSamples; ++i) {
        float phase = std::atan2(m_qData[i], m_iData[i]);
        float x = (float)i / (numSamples - 1) * width();
        float y = (0.5f - (phase / M_PI) * 0.45f) * height();
        phasePolyline << QPointF(x, y);
    }
    painter.drawPolyline(phasePolyline);
}

void IqPlotWidget::drawConstellationMode(QPainter& painter)
{
    if (m_iData.isEmpty() || m_qData.isEmpty()) return;

    int numSamples = qMin(m_iData.size(), m_qData.size());
    if (numSamples == 0) return;

    QPen pointPen(Qt::green);
    pointPen.setWidth(3);
    painter.setPen(pointPen);

    // Draw points (I vs Q scatter plot)
    for (int i = 0; i < numSamples; ++i) {
        float x = (0.5f + m_iData[i] * 0.45f) * width();
        float y = (0.5f - m_qData[i] * 0.45f) * height();
        painter.drawPoint(QPointF(x, y));
    }
}

void IqPlotWidget::drawCustomXYMode(QPainter& painter)
{
    if (m_customX.isEmpty() || m_customY.isEmpty()) return;

    int numSamples = qMin(m_customX.size(), m_customY.size());
    if (numSamples == 0) return;

    // Auto-scale if enabled
    float xMin = m_xMin, xMax = m_xMax;
    float yMin = m_yMin, yMax = m_yMax;

    if (m_autoScale) {
        xMin = *std::min_element(m_customX.begin(), m_customX.end());
        xMax = *std::max_element(m_customX.begin(), m_customX.end());
        yMin = *std::min_element(m_customY.begin(), m_customY.end());
        yMax = *std::max_element(m_customY.begin(), m_customY.end());
    }

    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    if (xRange == 0) xRange = 1.0f;
    if (yRange == 0) yRange = 1.0f;

    QPen customPen(Qt::cyan);
    customPen.setWidth(2);
    painter.setPen(customPen);

    QPolygonF customPolyline;
    for (int i = 0; i < numSamples; ++i) {
        float x = ((m_customX[i] - xMin) / xRange) * width();
        float y = (1.0f - (m_customY[i] - yMin) / yRange) * height();
        customPolyline << QPointF(x, y);
    }
    painter.drawPolyline(customPolyline);
}

void IqPlotWidget::drawFFTSpectrumMode(QPainter& painter) {
    if (m_iData.isEmpty() || m_qData.isEmpty()) return;

    QVector<float> frequencies;
    QVector<float> magnitudes;
    
    // Compute FFT
    computeFFT(m_iData, m_qData, frequencies, magnitudes);
    
    if (frequencies.isEmpty() || magnitudes.isEmpty()) return;
    
    int numPoints = frequencies.size();
    
    // Find min/max for scaling
    float maxMag = *std::max_element(magnitudes.begin(), magnitudes.end());
    float minMag = *std::min_element(magnitudes.begin(), magnitudes.end());
    
    if (maxMag <= minMag) {
        maxMag = minMag + 1.0f;
    }
    
    // Draw spectrum with zoomed view around signal
    QPen spectrumPen(Qt::yellow);
    spectrumPen.setWidth(2);
    painter.setPen(spectrumPen);
    // fprintf(stderr , "m=%f      sc=%f         zoom=%d  \n" ,m_sampleRate , sweepChart_bw , m_fftZoomFactor);
    if (m_sampleRate != sweepChart_bw)
    {
        return;
    }
    
    // Calculate display frequency range (zoomed around signal)
    // double displaySpan = m_sampleRate * m_fftZoomFactor; // Show signal ± some context
    // double displayMinFreq = m_centerFrequency - displaySpan / 2.0;
    // double displayMaxFreq = m_centerFrequency + displaySpan / 2.0;
    double displayMinFreq = m_centerFrequency - m_sampleRate / 2.0;
    double displayMaxFreq = m_centerFrequency + m_sampleRate / 2.0;

    // fprintf(stderr, "zm=%f , sr=%.3f , ds=%.3f , asd=%.3f\n" ,m_fftZoomFactor ,  m_sampleRate, displaySpan ,( m_centerFrequency - displaySpan / 2e6) );
    // double chartSpan = sweepChart_endFreq - sweepChart_startFreq;


    // Clamp to 0-6 GHz range
    if (displayMinFreq < 0) displayMinFreq = 0;
    if (displayMaxFreq > 6e9) displayMaxFreq = 6e9;
    
    double displayRange = displayMaxFreq - displayMinFreq;
    
    // Calculate actual frequency range of this signal
    double signalMinFreq = m_centerFrequency - m_sampleRate / 2.0;
    double signalMaxFreq = m_centerFrequency + m_sampleRate / 2.0;
    
    QPolygonF spectrumPolyline;
    int startIndex = (displayMinFreq - sweepChart_startFreq) * sweep_chartPoints.length() / (sweepChart_endFreq - sweepChart_startFreq);
    // fprintf(stderr, "MinF=%.3f ,MaxF=%.3f SF=%.3f , EF=%.3f , CHP = %d , I= %d\n , nP = %d", 
    //             displayMinFreq/1e6 ,displayMaxFreq/1e6 , sweepChart_startFreq/1e6 , sweepChart_endFreq/1e6 , sweep_chartPoints.length() , startIndex , numPoints);
    for (int i = 0; i < numPoints; ++i) {
       int index = i+startIndex;
        if(index>=0 && index<sweep_chartPoints.length()) 
       {
           sweep_chartPoints[i+startIndex].setY((magnitudes[i] - minMag) / (maxMag - minMag));
       }
    }
    double chartSpan = sweepChart_endFreq - sweepChart_startFreq;
    for (int i = 0; i < sweep_chartPoints.length(); ++i) {

        float x = ((sweep_chartPoints[i].x() - sweepChart_startFreq) / chartSpan) * width();

        // Only draw if within display bounds
        if (x >= 0 && x <= width()) {
            float y = i%3 ;
            spectrumPolyline << QPointF(x, y);
        }
    }
    if (!spectrumPolyline.isEmpty()) {
        painter.drawPolyline(spectrumPolyline);
    }
    // for (int i = 0; i < sweep_chartPoints.length(); ++i) {

    //     float x = ((sweep_chartPoints[i].x() - sweepChart_startFreq) / chartSpan) * width();

    //     // Only draw if within display bounds
    //     if (x >= 0 && x <= width()) {
    //         float y = (1.0f - sweep_chartPoints[i].y()) * height();
    //         spectrumPolyline << QPointF(x, y);
    //     }
    // }
    // if (!spectrumPolyline.isEmpty()) {
    //     painter.drawPolyline(spectrumPolyline);
    // }
}

void IqPlotWidget::setSweepRange(double startFreq , double endFreq , double bw, int FFT_size)
{
    if (bw < 1 || (endFreq - startFreq) <bw)
    {
        fprintf(stderr , "Sweep Range not enough");
        return;
    }
    QMutexLocker locker(&chart_plotMutex);
    sweepChart_startFreq = startFreq;
    sweepChart_endFreq = endFreq;
    sweepChart_bw = bw;
    

    sweep_chartPoints = QVector<QPointF>();
    double delta_f = bw/FFT_size;
    for(double fc = startFreq ; fc <= endFreq ; fc+=delta_f)
    {
        sweep_chartPoints.append(QPointF(fc,0));
    }
}


void IqPlotWidget::computeFFT(const QVector<float>& iData, const QVector<float>& qData,
                               QVector<float>& frequencies, QVector<float>& magnitudes) {
    int N = std::min(iData.size(), qData.size());
    if (N == 0) return;
    
    // Find the next power of 2 for FFT size
    int fftSize = 1;
    while (fftSize < N) {
        fftSize *= 2;
    }
    
    // Limit FFT size for performance
    if (fftSize > 4096) {
        fftSize = 4096;
    }
    
    // Prepare input data with windowing
    QVector<std::complex<float>> complexData(fftSize);
    int actualSamples = std::min(N, fftSize);
    
    for (int i = 0; i < actualSamples; ++i) {
        // Apply Hann window
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (actualSamples - 1)));
        complexData[i] = std::complex<float>(iData[i] * window, qData[i] * window);
    }
    
    // Zero padding
    for (int i = actualSamples; i < fftSize; ++i) {
        complexData[i] = std::complex<float>(0.0f, 0.0f);
    }
    
    // Perform FFT using radix-2 Cooley-Tukey algorithm
    QVector<std::complex<float>> fftResult = fft(complexData);
    
    // For complex I/Q data, we need the full FFT spectrum (negative and positive frequencies)
    // We'll perform FFT shift: move negative frequencies to the left, positive to the right
    frequencies.resize(fftSize);
    magnitudes.resize(fftSize);
    
    // FFT shift and compute magnitudes
    for (int i = 0; i < fftSize; ++i) {
        // Shift: second half (negative freq) goes to first half, first half (positive) goes to second
        int shiftedIndex = (i + fftSize / 2) % fftSize;
        
        float real = fftResult[shiftedIndex].real();
        float imag = fftResult[shiftedIndex].imag();
        float magnitude = std::sqrt(real * real + imag * imag);
        
        // Convert to dB (with floor to avoid log(0))
        magnitudes[i] = 20.0f * std::log10(std::max(magnitude, 1e-10f));
        
        // Frequency bins: map to -0.5 to +0.5 (after FFT shift)
        // i=0 corresponds to -SR/2, i=fftSize-1 corresponds to +SR/2
        frequencies[i] = (static_cast<float>(i) / fftSize) - 0.5f;
    }
}

QVector<std::complex<float>> IqPlotWidget::fft(const QVector<std::complex<float>>& input) {
    int N = input.size();
    
    // Base case
    if (N <= 1) {
        return input;
    }
    
    // Divide
    QVector<std::complex<float>> even(N / 2);
    QVector<std::complex<float>> odd(N / 2);
    
    for (int i = 0; i < N / 2; ++i) {
        even[i] = input[2 * i];
        odd[i] = input[2 * i + 1];
    }
    
    // Conquer
    QVector<std::complex<float>> evenFFT = fft(even);
    QVector<std::complex<float>> oddFFT = fft(odd);
    
    // Combine
    QVector<std::complex<float>> result(N);
    for (int k = 0; k < N / 2; ++k) {
        float angle = -2.0f * M_PI * k / N;
        std::complex<float> t = std::polar(1.0f, angle) * oddFFT[k];
        result[k] = evenFFT[k] + t;
        result[k + N / 2] = evenFFT[k] - t;
    }
    
    return result;
}

void IqPlotWidget::clearPersistence()
{
    m_persistenceBuffer = QPixmap();
    update();
}
