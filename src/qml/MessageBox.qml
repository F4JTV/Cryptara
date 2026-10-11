// MessageBox.qml - the QMessageBox of the Python version: information,
// warning, critical or a Yes/No question, with an optional informative text.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: box
    property string icon: "information"
    property string text: ""
    property string informative: ""
    property bool question: false
    property bool defaultYes: true
    signal answered(bool yes)
    readonly property var s: I18n.s

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, Overlay.overlay ? Overlay.overlay.width - 40 : 560)
    padding: 20
    Material.background: window.theme.surface

    property bool done: false
    function finish(yes) {
        if (done)
            return
        done = true
        close()
        answered(yes)
    }
    // Escape closes the box: a "No" (or the end of an information).
    onClosed: if (!done) { done = true; answered(false) }
    onOpened: {
        done = false
        if (question && !defaultYes)
            noButton.forceActiveFocus()
        else
            yesButton.forceActiveFocus()
    }

    header: Label {
        text: box.title
        font.bold: true
        font.pixelSize: 15
        padding: 16
        bottomPadding: 0
        elide: Text.ElideRight
        color: window.theme.foreground
    }

    contentItem: RowLayout {
        spacing: 16
        Label {
            Layout.alignment: Qt.AlignTop
            font.pixelSize: 30
            text: ({ "information": "ℹ️", "warning": "⚠️", "critical": "⛔", "question": "❓" })[box.icon] || "ℹ️"
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: box.text
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: window.theme.foreground
            }
            Label {
                Layout.fillWidth: true
                visible: box.informative.length > 0
                text: box.informative
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                color: window.colourOf("system")
            }
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        CButton {
            id: yesButton
            text: box.question ? box.s.btn_yes : box.s.btn_ok
            highlighted: !box.question || box.defaultYes
            onClicked: box.finish(true)
            Keys.onReturnPressed: box.finish(true)
            Keys.onEnterPressed: box.finish(true)
        }
        CButton {
            id: noButton
            visible: box.question
            text: box.s.btn_no
            highlighted: box.question && !box.defaultYes
            onClicked: box.finish(false)
            Keys.onReturnPressed: box.finish(false)
            Keys.onEnterPressed: box.finish(false)
        }
        Item { width: 8 }
    }

}
