///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB.                             //
//                                                                               //
// Swagger server adapter interface                                              //
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
#ifndef INCLUDE_PRESET_H
#define INCLUDE_PRESET_H

#include <QString>
#include <QList>
#include <QMetaType>

#include "export.h"

class SDRBASE_API Preset {
public:
	struct ChannelConfig {
		QString m_channelIdURI; //!< Channel type ID in URI form
		QByteArray m_config;

		ChannelConfig(const QString& channelIdURI, const QByteArray& config) :
			m_channelIdURI(channelIdURI),
			m_config(config)
		{ }
	};
	typedef QList<ChannelConfig> ChannelConfigs;

	struct DeviceConfig
	{
		QString m_deviceId;
		QString m_deviceSerial;
		int m_deviceSequence;
		QByteArray m_config;

		DeviceConfig(const QString& deviceId,
				const QString& deviceSerial,
				int deviceSequence,
				const QByteArray& config) :
					m_deviceId(deviceId),
					m_deviceSerial(deviceSerial),
					m_deviceSequence(deviceSequence),
					m_config(config)
		{ }
	};
	typedef QList<DeviceConfig> DeviceConfigs;

    struct SelectedDevice
    {
		QString m_deviceId;
		QString m_deviceSerial;
		int m_deviceSequence;
        int m_deviceItemIndex;

        SelectedDevice() = default;
        SelectedDevice(const SelectedDevice&) = default;
        SelectedDevice& operator=(const SelectedDevice&) = default;
    };

    enum PresetType
    {
        PresetSource, // Rx
        PresetSink,   // Tx
        PresetMIMO    // MIMO
    };

	Preset();
	Preset(const Preset& other);

	void resetToDefaults();

	void setSourcePreset() { m_presetType = PresetSource; }
	bool isSourcePreset() const { return m_presetType == PresetSource; }
	void setSinkPreset() { m_presetType = PresetSink; }
	bool isSinkPreset() const { return m_presetType == PresetSink; }
	void setMIMOPreset() { m_presetType = PresetMIMO; }
	bool isMIMOPreset() const { return m_presetType == PresetMIMO; }
    PresetType getPresetType() const { return m_presetType; }
    void setPresetType(PresetType presetType) { m_presetType = presetType; }
    static QString getPresetTypeChar(PresetType presetType);

	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setGroup(const QString& group) { m_group = group; }
	const QString& getGroup() const { return m_group; }
	void setDescription(const QString& description) { m_description = description; }
	const QString& getDescription() const { return m_description; }
	void setCenterFrequency(const quint64 centerFrequency) { m_centerFrequency = centerFrequency; }
	quint64 getCenterFrequency() const { return m_centerFrequency; }

	void setSpectrumConfig(const QByteArray& data) { m_spectrumConfig = data; }
	const QByteArray& getSpectrumConfig() const { return m_spectrumConfig; }
    void setSpectrumGeometry(const QByteArray& data) { m_spectrumGeometry = data; }
	const QByteArray& getSpectrumGeometry() const { return m_spectrumGeometry; }
    void setSpectrumWorkspaceIndex(int workspaceIndex) { m_spectrumWorkspaceIndex = workspaceIndex; }
	int getSpectrumWorkspaceIndex() const { return m_spectrumWorkspaceIndex; }
    void setSelectedDevice(const SelectedDevice& selectedDevice) { m_selectedDevice = selectedDevice; }
    SelectedDevice getSelectedDevice() const { return m_selectedDevice; }
    void setDeviceGeometry(const QByteArray& data) { m_deviceGeometry = data; }
	const QByteArray& getDeviceGeometry() const { return m_deviceGeometry; }
    void setDeviceWorkspaceIndex(int workspaceIndex) { m_deviceWorkspaceIndex = workspaceIndex; }
	int getDeviceWorkspaceIndex() const { return m_deviceWorkspaceIndex; }

	bool hasDCOffsetCorrection() const { return m_dcOffsetCorrection; }
    void setDCOffsetCorrection(bool dcOffsetCorrection) { m_dcOffsetCorrection = dcOffsetCorrection; }
	bool hasIQImbalanceCorrection() const { return m_iqImbalanceCorrection; }
    void setIQImbalanceCorrection(bool iqImbalanceCorrection) { m_iqImbalanceCorrection = iqImbalanceCorrection; }

	void setShowSpectrum(bool show) { m_showSpectrum = show; }
	bool getShowSpectrum() const { return m_showSpectrum; }
	void setLayout(const QByteArray& data) { m_layout = data; }
	const QByteArray& getLayout() const { return m_layout; }

	void clearChannels() { m_channelConfigs.clear(); }
	void addChannel(const QString& channel, const QByteArray& config) { m_channelConfigs.append(ChannelConfig(channel, config)); }
	int getChannelCount() const { return m_channelConfigs.count(); }
	const ChannelConfig& getChannelConfig(int index) const { return m_channelConfigs.at(index); }

    void clearDevices() { m_deviceConfigs.clear(); }
	void setDeviceConfig(const QString& deviceId, const QString& deviceSerial, int deviceSequence, const QByteArray& config) {
		addOrUpdateDeviceConfig(deviceId, deviceSerial, deviceSequence, config);
	}
    int getDeviceCount() const { return m_deviceConfigs.count(); }
    const DeviceConfig& getDeviceConfig(int index) const { return m_deviceConfigs.at(index); }

	void addOrUpdateDeviceConfig(const QString& deviceId,
			const QString& deviceSerial,
			int deviceSequence,
			const QByteArray& config);

	const QByteArray* findBestDeviceConfig(
            const QString& deviceId,
			const QString& deviceSerial,
			int deviceSequence) const;

	const QByteArray* findDeviceConfig(
            const QString& deviceId,
			const QString& deviceSerial,
			int deviceSequence) const;

	static bool presetCompare(const Preset *p1, Preset *p2)
	{
	    if (p1->m_group != p2->m_group)
	    {
	        return p1->m_group < p2->m_group;
	    }
	    else
	    {
            if (p1->m_centerFrequency != p2->m_centerFrequency) {
                return p1->m_centerFrequency < p2->m_centerFrequency;
            } else {
                return p1->m_description < p2->m_description;
            }
	    }
	}

protected:
    PresetType m_presetType;

    // group and preset description
	QString m_group;
	QString m_description;
	quint64 m_centerFrequency;

	// general configuration
	QByteArray m_spectrumConfig;
    QByteArray m_spectrumGeometry;
    int m_spectrumWorkspaceIndex;
    QByteArray m_deviceGeometry;
    int m_deviceWorkspaceIndex;
    SelectedDevice m_selectedDevice;

	// dc offset and i/q imbalance correction TODO: move it into the source data
	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;

	// channels and configurations
	ChannelConfigs m_channelConfigs;

	// devices and configurations
	DeviceConfigs m_deviceConfigs;

	// screen and dock layout
	bool m_showSpectrum;
	QByteArray m_layout;

private:
	const QByteArray* findBestDeviceConfigSoapy(const QString& deviceId, const QString& deviceSerial) const;
};

Q_DECLARE_METATYPE(const Preset*)
Q_DECLARE_METATYPE(Preset*)

#endif // INCLUDE_PRESET_H
