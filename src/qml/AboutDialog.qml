// AboutDialog.qml - Help > About.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: dialog
    readonly property var s: I18n.s
    title: s.dlg_about
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(500, Overlay.overlay ? Overlay.overlay.width - 40 : 500)
    Material.background: window.theme.surface

    contentItem: RowLayout {
        spacing: 18
        Image {
            Layout.alignment: Qt.AlignTop
            source: "qrc:/assets/cryptara.png"
            sourceSize.width: 72
            sourceSize.height: 72
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.StyledText
            color: window.theme.foreground
            text: "<h3>CRYPTARA</h3>"
                  + "<p><b>C</b>ryptographic <b>R</b>adio <b>Y</b>ield <b>P</b>rotection<br>"
                  + "<b>T</b>ransmission <b>A</b>utomated <b>R</b>eliable <b>A</b>ssistant</p>"
                  + "<p>" + dialog.s.about_description + "</p>"
                  + "<p>" + dialog.s.about_features + "</p>"
                  + "<p><b>" + dialog.s.about_version + "</b> " + Controller.version + "<br>"
                  + "<b>" + dialog.s.about_crypto + "</b> " + Controller.cryptoBackend + "</p>"
                  + "<p>73! 🔒📻</p>"
        }
    }

    footer: RowLayout {
        Item { Layout.fillWidth: true }
        CButton { text: dialog.s.btn_close; highlighted: true; onClicked: dialog.close() }
        Item { width: 8 }
    }
}
