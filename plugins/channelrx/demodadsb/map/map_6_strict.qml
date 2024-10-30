import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtPositioning 6.5
import QtLocation 6.5
import QtQuick.Effects

Item {
    id: qmlMap
    property int aircraftZoomLevel: 11
    property int aircraftMinZoomLevel: 11
    property int airportZoomLevel: 11
    property string mapProvider: "osm"
    property variant mapPtr
    property string requestedMapType
    property bool lightIcons
    property variant guiPtr
    property bool smoothing

    function createMap(pluginParameters, requestedMap, gui) {
        requestedMapType = requestedMap
        guiPtr = gui

        var paramString = ""
        for (var prop in pluginParameters) {
            var parameter = 'PluginParameter { name: "' + prop + '"; value: "' + pluginParameters[prop] + '"}'
            paramString = paramString + parameter
        }
        var pluginString = 'import QtLocation 6.5; Plugin{ name:"' + mapProvider + '"; '  + paramString + '}'
        var plugin = Qt.createQmlObject (pluginString, qmlMap)

        if (mapPtr) {
            // Objects aren't destroyed immediately, so don't call findChild("map")
            mapPtr.destroy()
            mapPtr = null
        }
        mapPtr = actualMapComponent.createObject(page)
        mapPtr.map.plugin = plugin
        mapPtr.map.forceActiveFocus()
        return mapPtr
    }

    Item {
        id: page
        anchors.fill: parent
    }

    Component {
        id: actualMapComponent

        ModifiedMapView {
            id: mapView
            objectName: "mapView"
            anchors.fill: parent
            map.center: QtPositioning.coordinate(51.5, 0.125) // London
            map.zoomLevel: 10
            map.objectName: "map"

            // not in 6
            //gesture.enabled: true
            //gesture.acceptedGestures: MapGestureArea.PinchGesture | MapGestureArea.PanGesture

            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                onClicked: {
                    // Unhighlight current aircraft
                    guiPtr.clearHighlighted()
                    mouse.accepted = false
                }
            }

            MapStation {
                id: station
                objectName: "station"
                stationName: "Home"
                parent: mapView.map
            }

            MapItemView {
                model: airspaceModel
                delegate: airspaceComponent
                parent: mapView.map
            }

            MapItemView {
                model: navAidModel
                delegate: navAidComponent
                parent: mapView.map
            }

            MapItemView {
                model: airspaceModel
                delegate: airspaceNameComponent
                parent: mapView.map
            }

            MapItemView {
                model: airportModel
                delegate: airportComponent
                parent: mapView.map
            }

            // This needs to be before aircraftComponent MapItemView, so it's drawn underneath
            MapItemView {
                model: aircraftModel
                delegate: aircraftPathComponent
                parent: mapView.map
            }

            MapItemView {
                model: aircraftModel
                delegate: aircraftComponent
                parent: mapView.map
            }

            map.onZoomLevelChanged: {
                if (map.zoomLevel > aircraftMinZoomLevel) {
                    aircraftZoomLevel = map.zoomLevel
                } else {
                    aircraftZoomLevel = aircraftMinZoomLevel
                }
                if (map.zoomLevel > 11) {
                    station.zoomLevel = map.zoomLevel
                    airportZoomLevel = map.zoomLevel
                } else {
                    station.zoomLevel = 11
                    airportZoomLevel = 11
                }
            }

            map.onSupportedMapTypesChanged : {
                for (var i = 0; i < map.supportedMapTypes.length; i++) {
                    if (requestedMapType == map.supportedMapTypes[i].name) {
                        map.activeMapType = map.supportedMapTypes[i]
                    }
                }
                lightIcons = (requestedMapType == "Night Transit Map") || (requestedMapType == "mapbox://styles/mapbox/dark-v9")
            }

        }
    }

    Component {
        id: navAidComponent
        MapQuickItem {
            id: navAid
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: airportZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    columnSpacing: 5
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    Image {
                        id: image
                        source: navAidImage
                        visible: !lightIcons
                        MouseArea {
                            anchors.fill: parent
                            onClicked: (mouse) => {
                                selected = !selected
                            }
                        }
                    }
                    MultiEffect {
                        width: image.width
                        height: image.height
                        source: image
                        brightness: 1.0
                        colorization: 1.0
                        colorizationColor: "#c0ffffff"
                        visible: lightIcons
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width + 5
                        height: text.height + 5
                        radius: 5
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: navAidData
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: (mouse) => {
                                selected = !selected
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: airspaceComponent
        MapPolygon {
            border.width: 1
            border.color: airspaceBorderColor
            color: airspaceFillColor
            path: airspacePolygon
        }
    }

    Component {
        id: airspaceNameComponent
        MapQuickItem {
            coordinate: position
            anchorPoint.x: airspaceText.width/2
            anchorPoint.y: airspaceText.height/2
            zoomLevel: airportZoomLevel
            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Text {
                        id: airspaceText
                        text: details
                    }
                }
            }
        }
    }

    Component {
        id: aircraftPathComponent
        MapPolyline {
            line.width: 2
            line.color: 'gray'
            path: aircraftPath
        }
    }

    Component {
        id: aircraftComponent
        MapQuickItem {
            id: aircraft
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: aircraftZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    layer.enabled: smoothing
                    layer.smooth: smoothing
                    horizontalItemAlignment: Grid.AlignHCenter
                    Image {
                        id: image
                        rotation: heading
                        source: aircraftImage
                        visible: !lightIcons
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    highlighted = true
                                    console.log("z=" + aircraft.sourceItem.z)
                                    aircraft.sourceItem.z = aircraft.sourceItem.z + 1
                                } else if (mouse.button === Qt.RightButton) {
                                    contextMenu.popup()
                                }
                            }
                            onDoubleClicked: {
                                target = true
                            }
                        }
                    }
                    MultiEffect {
                        width: image.width
                        height: image.height
                        rotation: heading
                        source: image
                        brightness: 1.0
                        colorization: 1.0
                        colorizationColor:  "#c0ffffff"
                        visible: lightIcons
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                highlighted = true
                            }
                            onDoubleClicked: {
                                target = true
                            }
                        }
                    }
                    Rectangle {
                        id: bubble
                        color: bubbleColour
                        border.width: 1
                        width: text.width * 1.1
                        height: text.height * 1.1
                        radius: 5
                        Text {
                            id: text
                            anchors.centerIn: parent
                            text: adsbData
                            textFormat: TextEdit.RichText
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    showAll = !showAll
                                } else if (mouse.button === Qt.RightButton) {
                                    contextMenu.popup()
                                }
                            }
                            Menu {
                                id: contextMenu
                                MenuItem {
                                    text: "Set as target"
                                    onTriggered: target = true
                                }
                                MenuItem {
                                    text: "Find on feature map"
                                    onTriggered: aircraftModel.findOnMap(index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: airportComponent
        MapItemGroup {
            MapItemGroup {
                property var groupVisible: false
                id: rangeGroup
                MapCircle {
                    id: circle5nm
                    center: position
                    color: "transparent"
                    border.color: "gray"
                    radius: 9260 // 5nm in metres
                    visible: rangeGroup.groupVisible
                }
                MapCircle {
                    id: circle10nm
                    center: position
                    color: "transparent"
                    border.color: "gray"
                    radius: 18520
                    visible: rangeGroup.groupVisible
                }
                MapCircle {
                    id: circle15nm
                    center: airport.coordinate
                    color: "transparent"
                    border.color: "gray"
                    radius: 27780
                    visible: rangeGroup.groupVisible
                }
                MapQuickItem {
                    id: text5nm
                    coordinate {
                        latitude: position.latitude
                        longitude: position.longitude + (5/60)/Math.cos(Math.abs(position.latitude)*Math.PI/180)
                    }
                    anchorPoint.x: 0
                    anchorPoint.y: height/2
                    sourceItem: Text {
                        color: "grey"
                        text: "5nm"
                    }
                    visible: rangeGroup.groupVisible
                }
                MapQuickItem {
                    id: text10nm
                    coordinate {
                        latitude: position.latitude
                        longitude: position.longitude + (10/60)/Math.cos(Math.abs(position.latitude)*Math.PI/180)
                    }
                    anchorPoint.x: 0
                    anchorPoint.y: height/2
                    sourceItem: Text {
                        color: "grey"
                        text: "10nm"
                    }
                    visible: rangeGroup.groupVisible
                }
                MapQuickItem {
                    id: text15nm
                    coordinate {
                        latitude: position.latitude
                        longitude: position.longitude + (15/60)/Math.cos(Math.abs(position.latitude)*Math.PI/180)
                    }
                    anchorPoint.x: 0
                    anchorPoint.y: height/2
                    sourceItem: Text {
                        color: "grey"
                        text: "15nm"
                    }
                    visible: rangeGroup.groupVisible
                }
            }

            MapQuickItem {
                id: airport
                anchorPoint.x: image.width/2
                anchorPoint.y: image.height/2
                coordinate: position
                zoomLevel: airportZoomLevel
                sourceItem: Grid {
                    columns: 1
                    Grid {
                        horizontalItemAlignment: Grid.AlignHCenter
                        layer.enabled: smoothing
                        layer.smooth: smoothing
                        Image {
                            id: image
                            source: airportImage
                            visible: !lightIcons
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: (mouse) => {
                                    if (mouse.button === Qt.RightButton) {
                                        showRangeItem.visible = !rangeGroup.groupVisible
                                        hideRangeItem.visible = rangeGroup.groupVisible
                                        menuItems.clear()
                                        var scanners = airportModel.getFreqScanners()
                                        for (var i = 0; i < scanners.length; i++) {
                                            menuItems.append({
                                                text: "Send to Frequency Scanner " + scanners[i],
                                                airport: index,
                                                scanner: scanners[i]
                                            })
                                        }
                                        contextMenu.popup()
                                    }
                                }
                                onDoubleClicked: (mouse) => {
                                    rangeGroup.groupVisible = !rangeGroup.groupVisible
                                }

                                ListModel {
                                    id: menuItems
                                }

                                Menu {
                                    id: contextMenu
                                    MenuItem {
                                        id: showRangeItem
                                        text: "Show range rings"
                                        onTriggered: rangeGroup.groupVisible = true
                                        height: visible ? implicitHeight : 0
                                    }
                                    MenuItem {
                                        id: hideRangeItem
                                        text: "Hide range rings"
                                        onTriggered: rangeGroup.groupVisible = false
                                        height: visible ? implicitHeight : 0
                                    }
                                    Instantiator {
                                        model: menuItems
                                        MenuItem {
                                            text: model.text
                                            onTriggered: airportModel.sendToFreqScanner(model.airport, model.scanner)
                                        }
                                        onObjectAdded: function(index, object) {
                                            contextMenu.insertItem(index, object)
                                        }
                                        onObjectRemoved: function(index, object) {
                                            contextMenu.removeItem(object)
                                        }
                                    }
                                }
                            }
                        }
                        MultiEffect {
                            width: image.width
                            height: image.height
                            source: image
                            brightness: 1.0
                            colorization: 1.0
                            colorizationColor: "#c0ffffff"
                            visible: lightIcons
                        }
                        Rectangle {
                            id: bubble
                            color: bubbleColour
                            border.width: 1
                            width: text.width + 5
                            height: text.height + 5
                            radius: 5
                            Text {
                                id: text
                                anchors.centerIn: parent
                                text: airportData
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: (mouse) => {
                                    if (showFreq) {
                                        var freqIdx = Math.floor((mouse.y-5)/((height-10)/airportDataRows))
                                        if (freqIdx == 0) {
                                            showFreq = false
                                        }
                                    } else {
                                        showFreq = true
                                    }
                                }
                                onDoubleClicked: (mouse) => {
                                    if (showFreq) {
                                        var freqIdx = Math.floor((mouse.y-5)/((height-10)/airportDataRows))
                                        if (freqIdx != 0) {
                                            selectedFreq = freqIdx - 1
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
