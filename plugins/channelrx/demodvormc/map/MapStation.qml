import QtQuick 2.12
import QtLocation 5.12
import QtPositioning 5.12

MapQuickItem {
    id: station
    property string stationName // Name of the station, E.g. Home

    coordinate: QtPositioning.coordinate(51.5, 0.125) // Location of the antenna (QTH) - London
    zoomLevel: 11

    anchorPoint.x: image.width/2
    anchorPoint.y: image.height/2

    sourceItem: Grid {
        columns: 1
        Grid {
            horizontalItemAlignment: Grid.AlignHCenter
            layer.enabled: true
            layer.smooth: true
            Image {
                id: image
                source: "antenna.png"
            }
            Rectangle {
                id: bubble
                color: "lightblue"
                border.width: 1
                width: text.width * 1.3
                height: text.height * 1.3
                radius: 5
                Text {
                    id: text
                    anchors.centerIn: parent
                    text: stationName
                }
            }
        }
    }
}
