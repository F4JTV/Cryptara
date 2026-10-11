// LineView.qml - the chat and the system log: coloured lines from a model,
// each one selectable, following the end as lines arrive.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import CryptaraBackend

Rectangle {
    id: view
    property alias model: list.model
    property string tip: ""
    property bool monospace: false
    readonly property var s: I18n.s

    color: window.theme.field
    border.color: window.theme.line
    border.width: 1
    radius: 4

    HoverHandler { id: hover }
    Tip { shown: hover.hovered; text: view.tip; delay: 900 }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 4
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { id: bar; policy: ScrollBar.AsNeeded }
        // Stay at the bottom unless the user scrolled up to read.
        property bool following: true
        onMovementEnded: following = atYEnd
        onCountChanged: if (following) Qt.callLater(positionViewAtEnd)
        onHeightChanged: if (following) Qt.callLater(positionViewAtEnd)

        delegate: TextEdit {
            width: ListView.view.width - bar.width
            text: model.text
            color: window.colourOf(model.kind)
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            font.family: view.monospace ? window.monoFamily : Qt.application.font.family
            font.pixelSize: view.monospace ? 13 : 12
            font.italic: model.kind === "debug"
            selectionColor: window.theme.selection
            selectedTextColor: window.theme.foreground
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    menu.line = parent
                    menu.popup()
                }
            }
        }
    }

    Menu {
        id: menu
        property var line: null
        MenuItem {
            text: view.s.menu_copy
            enabled: menu.line !== null && menu.line.selectedText.length > 0
            onTriggered: menu.line.copy()
        }
        MenuItem {
            text: view.s.menu_copy_all
            onTriggered: {
                clip.text = view.model.plainText()
                clip.selectAll()
                clip.copy()
                clip.text = ""
            }
        }
    }

    // QML has no clipboard object: a hidden text field does the copying.
    TextEdit { id: clip; visible: false }
}
