///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_FEATURE_DENOISER_VST3EFFECT_H_
#define INCLUDE_FEATURE_DENOISER_VST3EFFECT_H_

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <functional>
#include <memory>

class QWidget;

struct Vst3PluginInfo
{
    QString modulePath;
    QByteArray classId; // The factory's 16-byte platform-specific class ID.
    QString name;
};

struct Vst3ParameterInfo
{
    quint32 id;
    QString name;
    double defaultValue;
    double currentValue;
    int steps;
    bool readOnly;
    bool hidden;
};

// Owns one audio effect. Construct, configure and destroy on the feature thread;
// only process() is called by the audio worker while the instance is active.
class Vst3Effect
{
public:
    Vst3Effect();
    ~Vst3Effect();

    static QVector<Vst3PluginInfo> discover(const QStringList& extraPaths = {});
    static QStringList modulePaths(const QStringList& extraPaths = {});
    // state is a blob from state(); it is ignored if it belongs to a different class.
    bool open(const QString& modulePath, const QByteArray& classId, int sampleRate,
              int sourceChannels, QString& error, const QByteArray& state = QByteArray());
    bool process(const float *left, const float *right, float *outLeft, float *outRight,
                 int count, QString& error);
    void close();
    unsigned int latencySamples() const;
    QVector<Vst3ParameterInfo> parameters() const;
    double parameterValue(quint32 id) const;
    // Not safe while another thread calls process(); flushes queued parameter changes.
    QByteArray state(QString& error);
    bool setState(const QByteArray& state, QString& error);
    bool setParameter(quint32 id, double value);
    QString parameterText(quint32 id, double value) const;
    void setParameterEditCallback(std::function<void(quint32, double)> callback);
#ifdef QT_WIDGETS_LIB
    QWidget *createEditorWidget(QWidget *parent, QString& error);
#endif

private:
#ifdef QT_WIDGETS_LIB
    class EditorWidget;
#endif
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif
