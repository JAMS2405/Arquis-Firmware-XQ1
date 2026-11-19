// Arnes de pruebas: aloja ObjectManagerView exactamente como lo hace ObjectManager.qml (backend: manager), sin los modulos UM/Cura.
import QtQuick 2.15
import "../plugins/ARQUISPlugin/ui"

Item {
    width: 790
    height: 420
    ObjectManagerView {
        anchors.fill: parent
        backend: manager
    }
}
