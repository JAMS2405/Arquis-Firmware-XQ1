// Mapa proporcional de la cama (vista previa de lo que dibuja la pantalla de la impresora). NO calcula geometria: pinta los pixeles
// que entrega ObjectManagerBackend (bed_map_engine, la misma aritmetica que el firmware). El estado se ve por texto/insignia Y por color.
import QtQuick 2.15

Item {
    id: root
    property var backend
    width: 266
    height: 266

    Rectangle {                                         // marco de la zona del mapa
        anchors.fill: parent
        color: "#0B1220"
        border.color: "#26324A"
        radius: 4
    }
    Rectangle {                                         // cama
        x: backend.bed.x0
        y: backend.bed.y0
        width: backend.bed.x1 - backend.bed.x0 + 1
        height: backend.bed.y1 - backend.bed.y0 + 1
        color: "#111B2E"
        border.color: "#3A4A6B"
    }
    Repeater {
        model: backend.grid
        Rectangle {
            x: modelData.x0
            y: modelData.y0
            width: Math.max(1, modelData.x1 - modelData.x0)
            height: Math.max(1, modelData.y1 - modelData.y0)
            color: "#1B2740"
        }
    }
    Text {
        text: "abajo-izquierda = X0 Y0 (orientaci\u00f3n f\u00edsica sin confirmar)"
        color: "#7C8BA8"
        font.pixelSize: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
    }
    Repeater {
        model: backend.markers
        Rectangle {
            readonly property color base: modelData.state === 1 ? "#1E9E57" : modelData.state === 2 ? "#2D6CDF"
                                        : modelData.state === 3 ? "#D64545" : modelData.state === 4 ? "#C58A1A" : "#46557A"
            x: modelData.x0
            y: modelData.y0
            width: modelData.w
            height: modelData.h
            radius: modelData.shape === "C" ? width / 2 : 4
            color: base
            opacity: modelData.state === 4 ? 0.7 : 1.0
            border.width: modelData.selected ? 3 : (modelData.overlap ? 2 : 1)
            border.color: modelData.selected ? "#FFFFFF" : (modelData.overlap ? "#FFB020" : "#0B1220")
            Text {
                anchors.centerIn: parent
                text: modelData.label + modelData.badge
                color: "#FFFFFF"
                font.pixelSize: 12
                font.bold: true
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: backend.pickAt(mouse.x, mouse.y)
    }
}
