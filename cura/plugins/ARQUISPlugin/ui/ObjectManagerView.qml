// Contenido del Gestor de objetos: mapa de cama + lista, sincronizados (un unico indice de seleccion en el backend). Sin importar UM:
// se prueba fuera de Cura. ObjectManager.qml lo coloca dentro de un UM.Dialog.
import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var backend
    implicitWidth: 760
    implicitHeight: 400

    Rectangle {                                         // fondo propio: legible tambien fuera de un UM.Dialog
        anchors.fill: parent
        color: "#0B1220"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        Rectangle {                                         // cama del perfil distinta de la de la impresora: el mapa no se habilita
            visible: !backend.mapValid
            Layout.fillWidth: true
            implicitHeight: banner.implicitHeight + 8
            color: "#7A1F1F"
            border.color: "#FF6B6B"
            Text { id: banner; anchors.fill: parent; anchors.margins: 4; text: backend.bedMismatch; color: "#FFFFFF"; font.pixelSize: 12; font.bold: true; wrapMode: Text.WordWrap }
        }
        Text {
            text: backend.summary
            font.pixelSize: 15
            font.bold: true
            color: "#E6ECF7"
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            BedPreview {
                backend: root.backend
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2
                Rectangle {                                         // cabecera de la lista
                    Layout.fillWidth: true
                    height: 22
                    color: "#1B2740"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        spacing: 0
                        Text { text: "ID";       Layout.preferredWidth: 44;  color: "#9FB0CF"; font.pixelSize: 12 }
                        Text { text: "Nombre";   Layout.preferredWidth: 150; color: "#9FB0CF"; font.pixelSize: 12 }
                        Text { text: "X / Y mm"; Layout.preferredWidth: 100; color: "#9FB0CF"; font.pixelSize: 12 }
                        Text { text: "Tamano";   Layout.preferredWidth: 90;  color: "#9FB0CF"; font.pixelSize: 12 }
                        Text { text: "Estado";   Layout.fillWidth: true;     color: "#9FB0CF"; font.pixelSize: 12 }
                    }
                }
                ListView {                                          // lista (QML solo dibuja las filas visibles)
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: backend.rows
                    delegate: Rectangle {
                        width: list.width
                        height: 24
                        color: modelData.selected ? "#2A4A86" : (index % 2 ? "#101A2D" : "#0E1626")
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            spacing: 0
                            Text { text: modelData.label + modelData.badge; Layout.preferredWidth: 44; color: "#FFFFFF"; font.pixelSize: 13; font.bold: true }
                            Text { text: modelData.name; Layout.preferredWidth: 150; color: "#E6ECF7"; font.pixelSize: 13; elide: Text.ElideRight }
                            Text { text: modelData.pos; Layout.preferredWidth: 100; color: "#C7D2E8"; font.pixelSize: 12 }
                            Text { text: modelData.size; Layout.preferredWidth: 90; color: "#C7D2E8"; font.pixelSize: 12 }
                            Text { text: modelData.stateText; Layout.fillWidth: true; color: "#C7D2E8"; font.pixelSize: 12 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: backend.select(modelData.id) }
                    }
                }
            }
        }
        Text {
            text: backend.selectedInfo
            color: "#FFB020"
            font.pixelSize: 13
        }
        Repeater {
            model: backend.warnings
            Text { text: "! " + modelData; color: "#C7D2E8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
