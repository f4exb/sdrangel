// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtLocation as QL
import QtPositioning as QP
import Qt.labs.animation
/*!
    \qmltype MapView
    \inqmlmodule QtLocation
    \brief An interactive map viewer component.

    MapView wraps a Map and adds the typical interactive features:
    changing the zoom level, panning and tilting the map.

    The implementation is a QML assembly of smaller building blocks that are
    available separately. In case you want to make changes in your own version
    of this component, you can copy the QML, which is installed into the
    \c qml/QtLocation module directory, and modify it as needed.

    \sa Map
*/
Item {
    /*!
        \qmlproperty Map MapView::map

        This property provides access to the underlying Map instance.
    */
    property alias map: map

    /*!
        \qmlproperty real minimumZoomLevel

        The minimum zoom level according to the size of the view.

        \sa Map::minimumZoomLevel
    */
    property real minimumZoomLevel: map.minimumZoomLevel

    /*!
        \qmlproperty real maximumZoomLevel

        The maximum valid zoom level for the map.

        \sa Map::maximumZoomLevel
    */
    property real maximumZoomLevel: map.maximumZoomLevel

    // --------------------------------
    // implementation
    id: root
    Component.onCompleted: map.resetPinchMinMax()

    QL.Map {
        id: map
        width: parent.width
        height: parent.height
        tilt: tiltHandler.persistentTranslation.y / -5
        property bool pinchAdjustingZoom: false

        BoundaryRule on zoomLevel {
            id: br
            minimum: map.minimumZoomLevel
            maximum: map.maximumZoomLevel
        }

        onZoomLevelChanged: {
            br.returnToBounds();
            if (!pinchAdjustingZoom) resetPinchMinMax()
        }

        function resetPinchMinMax() {
            pinch.persistentScale = 1
            pinch.scaleAxis.minimum = Math.pow(2, root.minimumZoomLevel - map.zoomLevel + 1)
            pinch.scaleAxis.maximum = Math.pow(2, root.maximumZoomLevel - map.zoomLevel - 1)
        }

        PinchHandler {
            id: pinch
            target: null
            property real rawBearing: 0
            property QP.geoCoordinate startCentroid
            onActiveChanged: if (active) {
                flickAnimation.stop()
                pinch.startCentroid = map.toCoordinate(pinch.centroid.position, false)
            } else {
                flickAnimation.restart(centroid.velocity)
                map.resetPinchMinMax()
            }
            onScaleChanged: (delta) => {
                map.pinchAdjustingZoom = true
                map.zoomLevel += Math.log2(delta)
                map.alignCoordinateToPoint(pinch.startCentroid, pinch.centroid.position)
                map.pinchAdjustingZoom = false
            }
            onRotationChanged: (delta) => {
                pinch.rawBearing -= delta
                // snap to 0° if we're close enough
                map.bearing = (Math.abs(pinch.rawBearing) < 5) ? 0 : pinch.rawBearing
                map.alignCoordinateToPoint(pinch.startCentroid, pinch.centroid.position)
            }
            grabPermissions: PointerHandler.TakeOverForbidden
        }
        WheelHandler {
            id: wheel
            // workaround for QTBUG-87646 / QTBUG-112394 / QTBUG-112432:
            // Magic Mouse pretends to be a trackpad but doesn't work with PinchHandler
            // and we don't yet distinguish mice and trackpads on Wayland either
            acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                             ? PointerDevice.Mouse | PointerDevice.TouchPad
                             : PointerDevice.Mouse
            onWheel: (event) => {
                const loc = map.toCoordinate(wheel.point.position)
                switch (event.modifiers) {
                    case Qt.NoModifier:
                        // jonb - Changed to make more like Qt5
                        //map.zoomLevel += event.angleDelta.y / 120
                        map.zoomLevel += event.angleDelta.y / 1000
                        break
                    case Qt.ShiftModifier:
                        map.bearing += event.angleDelta.y / 15
                        break
                    case Qt.ControlModifier:
                        map.tilt += event.angleDelta.y / 15
                        break
                }
                map.alignCoordinateToPoint(loc, wheel.point.position)
            }
        }
        DragHandler {
            id: drag
            signal flickStarted // for autotests only
            signal flickEnded
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
            onActiveChanged: if (active) {
                flickAnimation.stop()
            } else {
                flickAnimation.restart(centroid.velocity)
            }
        }

        property vector3d animDest
        onAnimDestChanged: if (flickAnimation.running) {
            const delta = Qt.vector2d(animDest.x - flickAnimation.animDestLast.x, animDest.y - flickAnimation.animDestLast.y)
            map.pan(-delta.x, -delta.y)
            flickAnimation.animDestLast = animDest
        }

        Vector3dAnimation on animDest {
            id: flickAnimation
            property vector3d animDestLast
            from: Qt.vector3d(0, 0, 0)
            duration: 500
            easing.type: Easing.OutQuad
            onStarted: drag.flickStarted()
            onStopped: drag.flickEnded()

            function restart(vel) {
                stop()
                map.animDest = Qt.vector3d(0, 0, 0)
                animDestLast = Qt.vector3d(0, 0, 0)
                to = Qt.vector3d(vel.x / duration * 100, vel.y / duration * 100, 0)
                start()
            }
        }

        DragHandler {
            id: tiltHandler
            minimumPointCount: 2
            maximumPointCount: 2
            target: null
            xAxis.enabled: false
            grabPermissions: PointerHandler.TakeOverForbidden
            onActiveChanged: if (active) flickAnimation.stop()
        }
    }
}
