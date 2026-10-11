// MacrosDialog.qml - the F1-F12 macros (Tools > Macros).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: dialog
    readonly property var s: I18n.s
    title: s.dlg_macros_title
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(700, Overlay.overlay ? Overlay.overlay.width - 40 : 700)
    height: Math.min(600, Overlay.overlay ? Overlay.overlay.height - 40 : 600)
    Material.background: window.theme.surface

    function load() {
        const list = Controller.macros()
        for (let i = 0; i < 12; ++i)
            fields.itemAt(i).text = list[i] || ""
    }
    function values() {
        const list = []
        for (let i = 0; i < 12; ++i)
            list.push(fields.itemAt(i).text)
        return list
    }

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            text: "<b>" + dialog.s.macros_info_title + "</b><br><i>" + dialog.s.macros_variables + "</i>"
            textFormat: Text.StyledText
            wrapMode: Text.Wrap
            color: window.theme.foreground
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: parent.width - 8
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    id: fields
                    model: 12
                    delegate: CField {
                        Layout.row: index
                        Layout.column: 1
                        Layout.fillWidth: true
                        hint: dialog.s.macro_placeholder
                        tip: dialog.s.tt_macro_field.replace("%1", index + 1)
                    }
                }
                Repeater {
                    model: 12
                    delegate: Label {
                        Layout.row: index
                        Layout.column: 0
                        text: "<b>F" + (index + 1) + ":</b>"
                        textFormat: Text.StyledText
                        color: window.theme.foreground
                    }
                }
            }
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { width: 8 }
        CButton {
            text: dialog.s.btn_test_macro
            tip: dialog.s.tt_test_macro
            onClicked: Controller.testMacro(fields.itemAt(0).text)
        }
        Item { Layout.fillWidth: true }
        CButton {
            text: dialog.s.btn_save
            highlighted: true
            onClicked: { Controller.saveMacros(dialog.values()); dialog.close() }
        }
        CButton { text: dialog.s.btn_cancel; onClicked: dialog.close() }
        Item { width: 8 }
    }
}
