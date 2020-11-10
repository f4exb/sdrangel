import QtQuick 2.12
import QtQuick.Window 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    id: qmlMap
    property int aircraftZoomLevel: 11
    property int airportZoomLevel: 11

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        objectName: "map"
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(51.5, 0.125) // London
        zoomLevel: 10

        MapStation {
            id: station
            objectName: "station"
            stationName: "Home"
            coordinate:  QtPositioning.coordinate(51.5, 0.125)
        }

        // This needs to be before aircraftComponent MapItemView, so it's drawn underneath
        MapItemView {
            model: aircraftModel
            delegate: aircraftPathComponent
        }

        MapItemView {
            model: airportModel
            delegate: airportComponent
        }

        MapItemView {
            model: aircraftModel
            delegate: aircraftComponent
        }

        onZoomLevelChanged: {
            if (zoomLevel > 11) {
                station.zoomLevel = zoomLevel
                aircraftZoomLevel = zoomLevel
                airportZoomLevel = zoomLevel
            } else {
                station.zoomLevel = 11
                aircraftZoomLevel = 11
                airportZoomLevel = 11
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
                    layer.enabled: true
                    layer.smooth: true
                    horizontalItemAlignment: Grid.AlignHCenter
                    Image {
                        id: image
                        rotation: heading
                        source: aircraftImage
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
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
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                showAll = !showAll
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: airportComponent
        MapQuickItem {
            id: aircraft
            anchorPoint.x: image.width/2
            anchorPoint.y: image.height/2
            coordinate: position
            zoomLevel: airportZoomLevel

            sourceItem: Grid {
                columns: 1
                Grid {
                    horizontalItemAlignment: Grid.AlignHCenter
                    layer.enabled: true
                    layer.smooth: true
                    Image {
                        id: image
                        source: airportImage
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
                            hoverEnabled: true
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
