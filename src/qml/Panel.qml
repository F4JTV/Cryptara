// Panel.qml - a titled, raised group (the QGroupBox of the Python version).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Pane {
    id: panel
    property string title: ""
    default property alias content: body.data

    padding: 8
    Material.elevation: 3
    Material.background: window.theme.surface

    ColumnLayout {
        anchors.fill: parent
        spacing: 6
        Label {
            text: panel.title
            font.bold: true
            color: window.colourOf("heading")
            visible: text.length > 0
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        ColumnLayout {
            id: body
            spacing: 6
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
