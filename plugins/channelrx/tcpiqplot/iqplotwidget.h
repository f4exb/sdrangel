#ifndef IQPLOTWIDGET_H
#define IQPLOTWIDGET_H

#include <QWidget>
#include <QVector>
#include <QMutex>
#include <complex>

class IqPlotWidget : public QWidget
{
    Q_OBJECT

public:
    enum PlotMode {
        MODE_TIME_DOMAIN,      // I(t) and Q(t) vs time
        MODE_MAGNITUDE,        // sqrt(I²+Q²) vs time
        MODE_PHASE,            // atan2(Q,I) vs time
        MODE_CONSTELLATION,    // Q vs I (scatter plot)
        MODE_FFT_SPECTRUM,     // FFT frequency spectrum (magnitude vs frequency)
        MODE_CUSTOM_XY         // Custom X and Y arrays
    };

    explicit IqPlotWidget(QWidget *parent = nullptr);
    ~IqPlotWidget();

    // Set plot mode
    void setPlotMode(PlotMode mode) { m_plotMode = mode; update(); }
    
    // Set custom X axis data (for MODE_CUSTOM_XY)
    void setCustomXData(const QVector<float>& xData);
    
    // Set custom Y axis data (for MODE_CUSTOM_XY)
    void setCustomYData(const QVector<float>& yData);
    
    // Set X axis range
    void setXAxisRange(float min, float max);
    
    // Set Y axis range (auto-scale if not set)
    void setYAxisRange(float min, float max);
    void setAutoScale(bool enable) { m_autoScale = enable; }
    
    // Set frequency info for axis labels
    void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }
    void setCenterFrequency(double centerFreq) { m_centerFrequency = centerFreq; }
    
    // Persistence mode for frequency scanning
    void setPersistenceMode(bool enable) { m_persistenceEnabled = enable; if (!enable) clearPersistence(); }
    void clearPersistence();

public slots:
    void onNewData(const QVector<float>& iData, const QVector<float>& qData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawTimeMode(QPainter& painter);
    void drawMagnitudeMode(QPainter& painter);
    void drawPhaseMode(QPainter& painter);
    void drawConstellationMode(QPainter& painter);
    void drawFFTSpectrumMode(QPainter& painter);
    void setSweepRange(double startFreq , double endFreq , double bw , int FFT_size);
    void drawCustomXYMode(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    
    // FFT helper functions
    void computeFFT(const QVector<float>& iData, const QVector<float>& qData, 
                    QVector<float>& frequencies, QVector<float>& magnitudes);
    void applyWindow(QVector<float>& data, int windowType = 0); // 0=Hann, 1=Hamming, 2=Blackman
    QVector<std::complex<float>> fft(const QVector<std::complex<float>>& input);
    
    QVector<float> m_iData;
    QVector<float> m_qData;
    QVector<float> m_customX;
    QVector<float> m_customY;
    QVector<QPointF> sweep_chartPoints;

    QMutex m_mutex;
    QMutex chart_plotMutex;

    PlotMode m_plotMode;
    bool m_autoScale;
    float m_xMin, m_xMax;
    float m_yMin, m_yMax;
    
    // For frequency axis labels
    double m_sampleRate;      // Hz
    double m_centerFrequency; // Hz
    double m_fftZoomFactor;   // Zoom factor for FFT display (1.0 = show signal + context, larger = more zoomed out)
    double sweepChart_startFreq;
    double sweepChart_endFreq;
    double sweepChart_bw;
    // Persistence mode for frequency scanning
    bool m_persistenceEnabled;
    QPixmap m_persistenceBuffer;  // Accumulated FFT data
};

#endif // IQPLOTWIDGET_H
