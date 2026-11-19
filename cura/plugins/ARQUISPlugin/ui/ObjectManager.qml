// Ventana de Cura (UM.Dialog) que aloja ObjectManagerView. SOLO se carga dentro de Cura (necesita los modulos UM/Cura): ver ARQUISPlugin.py.
import QtQuick 2.2
import QtQuick.Controls 2.15

import UM 1.5 as UM
import Cura 1.0 as Cura

UM.Dialog
{
    id: dialog
    title: "ARQUIS - Gestor de objetos"
    minimumWidth: 800 * screenScaleFactor
    minimumHeight: 480 * screenScaleFactor
    width: minimumWidth
    height: minimumHeight
    backgroundColor: "#0B1220"
    onVisibleChanged: if (visible) manager.refreshFromCura()

    ObjectManagerView
    {
        anchors.fill: parent
        backend: manager
    }

    rightButtons: [
        Cura.SecondaryButton
        {
            text: "Actualizar"
            onClicked: manager.refreshFromCura()
        },
        Cura.PrimaryButton
        {
            text: "Cerrar"
            onClicked: dialog.close()
        }
    ]
}
