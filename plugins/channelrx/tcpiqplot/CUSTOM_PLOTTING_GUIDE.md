# TCP IQ Plot - Custom X/Y Axis Control Guide

## Overview
Your plugin now uses **IqPlotWidget** with full control over X and Y axes, instead of GLSpectrum's fixed frequency display.

## Current Features

### 1. No paintGL() - Uses QPainter instead
- **CPU-based rendering** (not GPU)
- **Full control** over what gets plotted
- **No dependency** on SDRangel's GLSpectrum code

### 2. Multiple Plot Modes

Change the plot mode in `tcpiqplotgui.cpp` constructor:

```cpp
// Time domain - I(t) and Q(t) vs sample index
ui->plotWidget->setPlotMode(IqPlotWidget::MODE_TIME_DOMAIN);

// Constellation - Q vs I scatter plot
ui->plotWidget->setPlotMode(IqPlotWidget::MODE_CONSTELLATION);

// Magnitude - sqrt(I²+Q²) vs sample index
ui->plotWidget->setPlotMode(IqPlotWidget::MODE_MAGNITUDE);

// Phase - atan2(Q,I) vs sample index
ui->plotWidget->setPlotMode(IqPlotWidget::MODE_PHASE);

// Custom XY - plot YOUR data
ui->plotWidget->setPlotMode(IqPlotWidget::MODE_CUSTOM_XY);
```

### 3. Custom X/Y Data Example

To plot custom data (e.g., frequency bins vs magnitude):

```cpp
// In tcpiqplot.cpp, modify processData():
void TcpIqPlot::processData(const QByteArray& buffer)
{
    // ... existing code to fill m_iSamples and m_qSamples ...
    
    // Example: Create frequency axis and magnitude data
    QVector<float> freqAxis(numSamples);
    QVector<float> magnitude(numSamples);
    
    for (int i = 0; i < numSamples; i++) {
        // X axis: frequency in MHz
        freqAxis[i] = i * (sampleRate / numSamples) / 1e6;
        
        // Y axis: magnitude in dB
        float mag = std::sqrt(m_iSamples[i]*m_iSamples[i] + 
                             m_qSamples[i]*m_qSamples[i]);
        magnitude[i] = 20 * std::log10(mag + 1e-10);
    }
    
    // Send custom X/Y data to GUI
    if (getMessageQueueToGUI()) {
        MsgCustomData *msg = MsgCustomData::create(freqAxis, magnitude);
        getMessageQueueToGUI()->push(msg);
    }
}
```

### 4. Set X/Y Axis Ranges

```cpp
// Fixed axis ranges (disable auto-scaling)
ui->plotWidget->setXAxisRange(0.0f, 100.0f);    // X from 0 to 100
ui->plotWidget->setYAxisRange(-50.0f, 0.0f);    // Y from -50 to 0

// Or use auto-scaling
ui->plotWidget->setAutoScale(true);
```

## Modifying the Plot

### Location of Plotting Code

**File: `iqplotwidget.cpp`**

- `drawTimeMode()` - Line ~98: Plots I(t) and Q(t)
- `drawMagnitudeMode()` - Line ~121: Plots magnitude
- `drawPhaseMode()` - Line ~142: Plots phase
- `drawConstellationMode()` - Line ~163: Plots Q vs I
- `drawCustomXYMode()` - Line ~183: Plots custom X vs Y data

### Example: Change X Axis from Sample Index to Time (seconds)

In `drawTimeMode()`:

```cpp
// OLD (sample index):
float x = (float)i / (numSamples - 1) * width();

// NEW (time in seconds):
float sampleRate = 48000.0f;  // Your sample rate
float timeSeconds = i / sampleRate;
float x = (timeSeconds / maxTime) * width();  // maxTime = total duration
```

### Example: Change Y Axis Scaling

In `drawMagnitudeMode()`:

```cpp
// OLD (linear):
float y = (1.0f - magnitude) * height();

// NEW (logarithmic dB scale):
float magnitudeDB = 20 * std::log10(magnitude + 1e-10);
float y = ((maxDB - magnitudeDB) / (maxDB - minDB)) * height();
```

## Comparison with GLSpectrum

| Feature | GLSpectrum (paintGL) | IqPlotWidget (paintEvent) |
|---------|---------------------|---------------------------|
| **Rendering** | GPU OpenGL | CPU QPainter |
| **X Axis Control** | Fixed (frequency bins) | **Full control** |
| **Y Axis Control** | Fixed (power spectrum) | **Full control** |
| **Custom Data** | No | **Yes** |
| **Modify SDRangel** | Required | **Not required** |
| **Performance** | Faster (GPU) | Slower (CPU) |

## Quick Modifications

### 1. Plot FFT Magnitude Spectrum (like before, but with control)

```cpp
// In tcpiqplot.cpp, keep processFFT() but send results to custom plot:
void TcpIqPlot::processFFT(const QVector<float>& iData, const QVector<float>& qData)
{
    // Perform FFT...
    m_fft->transform();
    Complex* fftOut = m_fft->out();
    
    // Convert to magnitude
    QVector<float> freqBins(m_fftSize);
    QVector<float> magnitudeDB(m_fftSize);
    
    for (int j = 0; j < m_fftSize; j++) {
        freqBins[j] = j;  // Or convert to actual frequency
        float mag = std::abs(fftOut[j]);
        magnitudeDB[j] = 20 * std::log10(mag + 1e-10);
    }
    
    // NOW YOU CONTROL THE AXES!
    // Send to GUI with custom X/Y mode
}
```

### 2. Change Number of Samples Displayed

In `tcpiqplot.cpp`:

```cpp
// OLD: Process when 1024 samples received
if (m_iSamples.size() >= 1024) {

// NEW: Process when YOUR desired number received  
if (m_iSamples.size() >= 2048) {  // Or any number you want!
```

## Summary

✅ **You now have full control over X and Y axes**
✅ **No paintGL() - uses standard QPainter**
✅ **No need to modify SDRangel core code**
✅ **Can plot anything: time, frequency, custom data**
✅ **Multiple built-in plot modes**
✅ **Easy to customize in your plugin code**

The key files to modify:
- `iqplotwidget.cpp` - Change HOW data is drawn
- `tcpiqplot.cpp` - Change WHAT data is sent
- `tcpiqplotgui.cpp` - Change plot mode selection
