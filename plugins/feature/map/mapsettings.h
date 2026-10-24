///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_MAPSETTINGS_H_
#define INCLUDE_FEATURE_MAPSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QHash>
#include <QRegularExpression>

#define MAX_FILTER_DISTANCE_KM 10000

class Serializable;

struct MapSettings
{
    struct MapItemSettings {
        QString m_group;          // Name of the group the settings apply to
        bool m_enabled;           // Whether enabled at all on 2D or 3D map
        bool m_display2DIcon;     // Display image 2D map
        bool m_display2DLabel;    // Display label on 2D map
        bool m_display2DTrack;    // Display tracks (or polygon) on 2D map
        quint32 m_2DTrackColor;
        QString m_2DTrackPalette; // Palette used to colour the track by altitude, on BOTH the
                                  // 2D and 3D maps. Empty for a plain single colour track
        int m_2DMinZoom;
        bool m_display3DModel;    // Draw 3D model for item
        bool m_display3DLabel;    // Display a label next to this item on the 3D map
        bool m_display3DPoint;    // Draw a point for this item on the 3D map
        quint32 m_3DPointColor;
        bool m_display3DTrack;    // Display a ground track for this item on the 3D map
        quint32 m_3DTrackColor;
        int m_3DModelMinPixelSize;
        float m_3DLabelScale;

        // Everything CZML::update() reads from these settings, rolled into one value so
        // it can tell in a few instructions whether an item's 3D representation could
        // have changed. ADD TO THIS whenever CZML starts reading another field, or a
        // change to it will not reach the 3D map.
        quint32 czmlSignature() const
        {
            quint32 h = m_enabled ? 1u : 0u;
            h = (h << 1) | (m_display3DModel ? 1u : 0u);
            h = (h << 1) | (m_display3DLabel ? 1u : 0u);
            h = (h << 1) | (m_display3DPoint ? 1u : 0u);
            h = (h << 1) | (m_display3DTrack ? 1u : 0u);
            h ^= m_3DPointColor * 2654435761u;
            h ^= m_3DTrackColor * 2246822519u;
            h ^= ((quint32) m_3DModelMinPixelSize) * 3266489917u;
            h ^= ((quint32) (m_3DLabelScale * 1000.0f)) * 668265263u;
            h ^= qHash(m_2DTrackPalette); // Colours the 3D track's altitude segments
            h ^= ((quint32) m_smoothingWindow) * 2166136261u; // Switches the 3D track between path and smoothed polylines
            return h;
        }
        QString m_filterName;
        QRegularExpression m_filterNameRE;
        int m_filterDistance;     // Filter items > this distance in metres away from My Position. <= 0 don't filter
        int m_extrapolate;        // Extrapolate duration in seconds on 3D map
        int m_smoothingWindow;    // Window size (numer of points) to smooth over. 0 for no smoothing
        float m_smoothingLambda;  // Lambda parameter for WhittakerEilers, controls how much smoothing to apply

        static QString defaultTrackPalette(const QString& group);

        MapItemSettings(const QString& group, bool enabled, const QColor color, bool display2DTrack=true, bool display3DPoint=true, int minZoom=11, int modelMinPixelSize=0);
        MapItemSettings(const QByteArray& data);
        void resetToDefaults();
        QByteArray serialize() const;
        bool deserialize(const QByteArray& data);
    };

    struct MapPolygonSettings {
        QString m_group;
        bool m_enabled;
        quint32 m_2DColor;
        int m_2DMinZoom;
        quint32 m_3DColor;
    };

    bool m_displayNames;
    QString m_mapProvider;
    QString m_thunderforestAPIKey;
    QString m_maptilerAPIKey;
    QString m_mapBoxAPIKey;
    QString m_osmURL;
    QString m_mapBoxStyles;
    bool m_displayAllGroundTracks;
    bool m_displaySelectedGroundTracks;
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    bool m_map2DEnabled;
    bool m_useLiveryIcons;      // Use aircraft icons with airline liveries on 2D map, instead of the sender's plain icons
    QString m_mapType;          // "Street Map", "Satellite Map", etc.. as selected in combobox
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    // 3D Map settings
    bool m_map3DEnabled;
    QString m_terrain;          // "Ellipsoid" or "Cesium World Terrain"
    QString m_buildings;        // "None" or "Cesium OSM Buildings"
    QString m_modelURL;         // Base URL for 3D models (Not user settable, as depends on web server port)
    QString m_modelDir;         // Directory to store 3D models (not customizable for now, as ADS-B plugin needs to know)
    QString m_msfsNavigraphDir; // Directory recursively scanned for Navigraph waypoint BGL files
    QString m_msfsAircraftDir;  // Parent directory scanned for MSFS aircraft.cfg files
    bool m_sunLightEnabled;     // Light globe from direction of Sun
    float m_lightIntensity;     // Intensity of camera light
    bool m_eciCamera;           // Use ECI instead of ECEF for camera
    QString m_cesiumIonAPIKey;
    bool m_fxaa;
    int m_msaa;
    // Where the 2D map was left, so it can be restored when the Map is next opened.
    // m_mapZoomLevel < 0 means nothing has been saved yet, in which case the map is
    // centred on the station. 0,0 is a valid position, so it can't mean that.
    float m_mapCenterLatitude;
    float m_mapCenterLongitude;
    float m_mapZoomLevel;
    bool m_terrainLighting;
    bool m_water;
    //!< Google Photorealistic 3D Tiles - replaces terrain, buildings and imagery and
    //!< hides the globe (so globe-draped overlays too). Requires a user Cesium Ion key.
    bool m_googlePhotorealistic3DTiles;
    bool m_hdr;
    bool m_fog;
    bool m_fps;

    bool m_displayMUF;          // Plot MUF contours
    bool m_displayfoF2;         // Plot foF2 contours
    bool m_displayRain;
    bool m_displayClouds;
    bool m_displaySeaMarks;
    bool m_displayRailways;
    bool m_displayNASAGlobalImagery;
    QString m_nasaGlobalImageryIdentifier;
    int m_nasaGlobalImageryOpacity;
    bool m_displayAurora;
    bool m_displayMagDec;
    bool m_displayMaidenheadGrid;
    bool m_displayPFD;
    bool m_displayND;
    int m_mapItemSoundVolume;   //!< [0..100] Engine and animation sounds located at map items
    int m_cockpitSoundVolume;   //!< [0..100] Altitude callouts, datalink ring, etc
    bool m_audioMuted;          //!< Toolbar mute: silences all 3D map audio, volumes kept
    //!< Master switch for the cockpit panels: the toolbar button turns everything
    //!< selected in its menu on or off together. A panel is shown only when this and
    //!< its own flag below are both set
    bool m_displayCockpitPanels;
    bool m_displayEFIS;         //!< Display the EFIS control panel on the 3D map
    bool m_displayRMP;          //!< Display the RMP (radio management / DRAIMS) panel on the 3D map
    bool m_displayDCDU;         //!< Display the DCDU (and allow it to pop on messages)
    // The RMP's two VHF COM channels: the AM Demod each is linked to, as "R<deviceSet>:<channel>"
    // (empty for unassigned), and its standby frequency in Hz. The active frequency is
    // not stored - it lives in the AM Demod and is read back from it
    QString m_vhf1Channel;
    QString m_vhf2Channel;
    int m_vhf1Standby;
    int m_vhf2Standby;
    // Position and size of each cockpit panel as the 3D map page last reported them, as
    // compact JSON: {"pfd": {"l": left, "t": top, "w": width, "h": height}, ...} in CSS
    // pixels. One setting rather than four per panel, so adding a panel needs no new key
    QString m_cockpitPanelGeometry;
    // What the EFIS panel's knobs and pushbuttons were left set to, as compact JSON:
    // {"mode", "range", "wx", "terr", "arpt", "vor", "tcas"}
    QString m_efisState;
    //!< Bumped when a default in m_itemSettings changes in a way a stored preset would
    //!< otherwise override for ever - see deserialize(). 0 means a preset written before
    //!< airspaces were given a 2D fill.
    int m_itemSettingsVersion;
    //!< How the 2D map, the 3D map and the NASA panel divide the window, as QSplitter
    //!< writes it. Not user editable, so it is nowhere in the settings dialog - it is
    //!< saved because the layout is worth keeping across a restart, like the map
    //!< position beside it.
    QByteArray m_splitterState;
    // How the camera follows the tracked object on the 3D map
    enum CameraMode {
        ThirdPerson = 0,
        FirstPerson = 1,
        FlyBy = 2       //!< Held ahead of and beside the object, watching it pass
    };
    int m_cameraMode;
    QString m_defaultImagery;

    QString m_checkWXAPIKey;    //!< checkwxapi.com API key
    QString m_arcGISAPIKey;

    // Per source settings
    QHash<QString, MapItemSettings *> m_itemSettings;

    MapSettings();
    MapSettings(const MapSettings& other);
    MapSettings& operator=(const MapSettings& other);
    ~MapSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serializeItemSettings(QHash<QString, MapItemSettings *> itemSettings) const;
    void deserializeItemSettings(const QByteArray& data, QHash<QString, MapItemSettings *>& itemSettings);
    void applySettings(const QStringList& settingsKeys, const MapSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    //!< Whether the user has entered their own Cesium Ion API key. Options in
    //!< m_cesiumIon* use Cesium Ion quota, so are unavailable without one.
    bool hasCesiumIonUserKey() const { return !m_cesiumIonAPIKey.trimmed().isEmpty(); }

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;

    static const QStringList m_mapProviders;
    static const QStringList m_mapProviderNames;

    // Imagery/terrain/buildings options that consume Cesium Ion quota
    static const QStringList m_cesiumIonImagery;
    static const QStringList m_cesiumIonTerrain;
    static const QStringList m_cesiumIonBuildings;
};

Q_DECLARE_METATYPE(MapSettings::MapItemSettings *);

#endif // INCLUDE_FEATURE_MAPSETTINGS_H_
