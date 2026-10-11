// PasswordDialog.qml - QInputDialog.getText(..., EchoMode.Password).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: dialog
    property string prompt: ""
    signal answered(bool ok, string text)
    readonly property var s: I18n.s

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 40 : 520)
    padding: 20
    Material.background: window.theme.surface

    property bool done: false
    function finish(ok) {
        if (done)
            return
        done = true
        const value = field.text
        field.text = ""
        close()
        answered(ok, value)
    }
    onClosed: if (!done) { done = true; field.text = ""; answered(false, "") }
    onOpened: {
        done = false
        field.text = ""
        field.forceActiveFocus()
    }

    header: Label {
        text: dialog.title
        font.bold: true
        font.pixelSize: 15
        padding: 16
        bottomPadding: 0
        color: window.theme.foreground
    }

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            text: dialog.prompt
            wrapMode: Text.Wrap
            color: window.theme.foreground
        }
        TextField {
            id: field
            Layout.fillWidth: true
            echoMode: TextInput.Password
            onAccepted: dialog.finish(true)
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        CButton { text: dialog.s.btn_ok; highlighted: true; onClicked: dialog.finish(true) }
        CButton { text: dialog.s.btn_cancel; onClicked: dialog.finish(false) }
        Item { width: 8 }
    }

}
