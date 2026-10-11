// TableDialog.qml - a list of rows in columns: the QSO log and the BBS
// file list.  columns: [{ title, role, stretch }]
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: dialog
    property var columns: []
    property alias model: list.model
    property string info: ""
    property bool showRefresh: false
    signal activated(int row)
    signal refresh()
    readonly property var s: I18n.s

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay ? Overlay.overlay.width - 40 : 640)
    height: Math.min(440, Overlay.overlay ? Overlay.overlay.height - 40 : 440)
    Material.background: window.theme.surface

    function widthOf(column, total) {
        let stretch = 0
        for (const c of columns)
            stretch += c.stretch
        return total * column.stretch / stretch
    }

    contentItem: ColumnLayout {
        spacing: 6
        Label {
            visible: dialog.info.length > 0
            text: dialog.info
            font.italic: true
            color: window.colourOf("dim")
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: window.theme.field
            border.color: window.theme.line
            radius: 4
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    color: window.theme.button
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: dialog.columns
                            delegate: Label {
                                width: dialog.widthOf(modelData, parent.width)
                                height: 28
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.title
                                font.bold: true
                                elide: Text.ElideRight
                                color: window.theme.buttonText
                            }
                        }
                    }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: row
                        required property int index
                        required property var model
                        width: ListView.view.width
                        height: 26
                        color: ListView.isCurrentItem ? window.theme.selection : "transparent"
                        Row {
                            anchors.fill: parent
                            Repeater {
                                model: dialog.columns
                                delegate: Label {
                                    width: dialog.widthOf(modelData, row.width)
                                    height: 26
                                    leftPadding: 8
                                    verticalAlignment: Text.AlignVCenter
                                    text: row.model[modelData.role] || ""
                                    elide: Text.ElideRight
                                    color: window.theme.foreground
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: list.currentIndex = row.index
                            onDoubleClicked: { list.currentIndex = row.index; dialog.activated(row.index) }
                        }
                    }
                }
            }
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { width: 8 }
        CButton {
            visible: dialog.showRefresh
            text: "🔄 " + dialog.s.bbs_refresh
            onClicked: dialog.refresh()
        }
        Item { Layout.fillWidth: true }
        CButton { text: dialog.s.btn_close; onClicked: dialog.close() }
        Item { width: 8 }
    }
}
