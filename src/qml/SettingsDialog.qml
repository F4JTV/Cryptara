// SettingsDialog.qml - the five tabs of the Python settings dialog
// (Modem, Station Info, Auto-responder, Interface, BBS); theme and language
// join the Interface tab.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import CryptaraBackend

Dialog {
    id: dialog
    readonly property var s: I18n.s
    title: s.dlg_settings
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay ? Overlay.overlay.width - 40 : 640)
    height: Math.min(600, Overlay.overlay ? Overlay.overlay.height - 40 : 600)
    padding: 0
    Material.background: window.theme.surface

    readonly property var modemTypes: ["VARA HF", "VARA FM", "VARA SAT"]
    readonly property var themeKeys: ["light", "dark", "red", "amber"]
    readonly property var languageKeys: ["auto", "en", "fr"]

    function load() {
        const v = Controller.settingsMap()
        modemCombo.currentIndex = Math.max(0, modemTypes.indexOf(v.modem_type))
        hfPath.text = v.vara_hf_path
        fmPath.text = v.vara_fm_path
        satPath.text = v.vara_sat_path
        autoStart.checked = v.auto_start_vara
        host.text = v.host
        cmdPort.value = v.cmd_port
        dataPort.value = v.data_port
        listen.checked = v.listen
        compression.checked = v.compression
        mycall.text = v.mycall
        operatorName.text = v.operator_name
        firstName.text = v.operator_firstname
        qth.text = v.qth
        grid.text = v.grid_square
        rig.text = v.rig
        antenna.text = v.antenna
        autoEnabled.checked = v.auto_enabled
        autoMessage.text = v.auto_message
        autoDisconnect.checked = v.auto_disconnect
        autoDelay.value = v.auto_delay
        timestamp.checked = v.timestamp
        sound.checked = v.sound
        saveLog.checked = v.save_log
        rxFolder.text = v.file_rx_path
        themeCombo.currentIndex = Math.max(0, themeKeys.indexOf(v.theme))
        languageCombo.currentIndex = Math.max(0, languageKeys.indexOf(v.language))
        bbsEnabled.checked = v.bbs_enabled
        bbsFolder.text = v.bbs_folder
        tabs.currentIndex = 0
    }

    function values() {
        return {
            "modem_type": modemTypes[modemCombo.currentIndex],
            "host": host.text,
            "cmd_port": cmdPort.value,
            "data_port": dataPort.value,
            "mycall": mycall.text.toUpperCase(),
            "operator_name": operatorName.text,
            "operator_firstname": firstName.text,
            "qth": qth.text,
            "grid_square": grid.text.toUpperCase(),
            "rig": rig.text,
            "antenna": antenna.text,
            "listen": listen.checked,
            "compression": compression.checked,
            "vara_hf_path": hfPath.text,
            "vara_fm_path": fmPath.text,
            "vara_sat_path": satPath.text,
            "auto_start_vara": autoStart.checked,
            "auto_enabled": autoEnabled.checked,
            "auto_message": autoMessage.text,
            "auto_disconnect": autoDisconnect.checked,
            "auto_delay": autoDelay.value,
            "timestamp": timestamp.checked,
            "sound": sound.checked,
            "save_log": saveLog.checked,
            "file_rx_path": rxFolder.text,
            "theme": themeKeys[themeCombo.currentIndex],
            "language": languageKeys[languageCombo.currentIndex],
            "bbs_enabled": bbsEnabled.checked,
            "bbs_folder": bbsFolder.text
        }
    }

    onAccepted: Controller.applySettings(values())

    // Screenshot mode: one tab, optionally with the automatic message focused.
    function showTab(index, focusMessage) {
        tabs.currentIndex = index
        if (focusMessage)
            autoMessage.forceActiveFocus()
    }

    // ---- file and folder pickers
    property var pathTarget: null
    FileDialog {
        id: exeDialog
        title: dialog.s.dlg_select_vara
        fileMode: FileDialog.OpenFile
        nameFilters: [dialog.s.filter_exe, dialog.s.file_all_files + " (*)"]
        onAccepted: if (dialog.pathTarget) dialog.pathTarget.text = Controller.toLocalPath(selectedFile)
    }
    FolderDialog {
        id: folderDialog
        onAccepted: if (dialog.pathTarget) dialog.pathTarget.text = Controller.toLocalPath(selectedFolder)
    }
    function browseExe(field) {
        pathTarget = field
        if (field.text.length > 0)
            exeDialog.currentFolder = Controller.folderUrlOf(field.text)
        exeDialog.open()
    }
    function browseFolder(field, title) {
        pathTarget = field
        folderDialog.title = title
        if (field.text.length > 0)
            folderDialog.currentFolder = Controller.toFileUrl(field.text)
        folderDialog.open()
    }

    // ---- small building blocks
    component FormLabel: Label {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        color: window.theme.foreground
    }
    component PathRow: RowLayout {
        id: pathRow
        property alias text: field.text
        property alias placeholder: field.hint
        property alias tip: field.tip
        signal browse()
        Layout.fillWidth: true
        CField {
            id: field
            Layout.fillWidth: true
        }
        CButton { text: "📁"; implicitWidth: 44; onClicked: pathRow.browse() }
    }
    component TipField: CField {
        Layout.fillWidth: true
    }
    component TipCheck: CheckBox {
        id: tipCheck
        property string tip: ""
        hoverEnabled: true
        Tip { shown: tipCheck.hovered; text: tipCheck.tip }
    }

    header: TabBar {
        id: tabs
        Material.background: window.theme.surface
        TabButton { text: dialog.s.tab_modem; font.capitalization: Font.MixedCase }
        TabButton { text: dialog.s.tab_info_station; font.capitalization: Font.MixedCase }
        TabButton { text: dialog.s.tab_auto_responder; font.capitalization: Font.MixedCase }
        TabButton { text: dialog.s.tab_interface; font.capitalization: Font.MixedCase }
        TabButton { text: dialog.s.tab_bbs; font.capitalization: Font.MixedCase }
    }

    contentItem: StackLayout {
        currentIndex: tabs.currentIndex

        // ------------------------------------------------------- Modem
        ScrollView {
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: parent.width - 32
                x: 16
                y: 12
                columns: 2
                columnSpacing: 12
                rowSpacing: 10

                FormLabel { text: dialog.s.lbl_modem_type }
                CCombo {
                    id: modemCombo
                    Layout.fillWidth: true
                    model: dialog.modemTypes
                    tip: dialog.s.tt_modem_type
                }

                Label {
                    Layout.columnSpan: 2
                    Layout.topMargin: 8
                    text: dialog.s.lbl_vara_paths
                    font.bold: true
                    color: window.colourOf("heading")
                }
                FormLabel { text: "VARA HF:" }
                PathRow { id: hfPath; placeholder: "C:\\VARA\\VARA.exe"; tip: dialog.s.tt_vara_path; onBrowse: dialog.browseExe(hfPath) }
                FormLabel { text: "VARA FM:" }
                PathRow { id: fmPath; placeholder: "C:\\VARA FM\\VARAFM.exe"; tip: dialog.s.tt_vara_path; onBrowse: dialog.browseExe(fmPath) }
                FormLabel { text: "VARA SAT:" }
                PathRow { id: satPath; placeholder: "C:\\VARA SAT\\VARASAT.exe"; tip: dialog.s.tt_vara_path; onBrowse: dialog.browseExe(satPath) }
                Item { width: 1; height: 1 }
                TipCheck { id: autoStart; text: dialog.s.lbl_auto_start; tip: dialog.s.tt_auto_start }

                Item { Layout.columnSpan: 2; height: 8 }
                FormLabel { text: dialog.s.lbl_host }
                TipField { id: host; tip: dialog.s.tt_host }
                FormLabel { text: dialog.s.lbl_cmd_port }
                SpinBox {
                    id: cmdPort
                    from: 1024; to: 65535; editable: true
                    hoverEnabled: true
                    Tip { shown: parent.hovered; text: dialog.s.tt_cmd_port }
                    textFromValue: (value) => String(value)
                    valueFromText: (text) => parseInt(text)
                }
                FormLabel { text: dialog.s.lbl_data_port }
                SpinBox {
                    id: dataPort
                    from: 1024; to: 65535; editable: true
                    hoverEnabled: true
                    Tip { shown: parent.hovered; text: dialog.s.tt_data_port }
                    textFromValue: (value) => String(value)
                    valueFromText: (text) => parseInt(text)
                }
                Item { width: 1; height: 1 }
                TipCheck { id: listen; text: dialog.s.lbl_listen; tip: dialog.s.tt_listen_mode }
                Item { width: 1; height: 1 }
                TipCheck { id: compression; text: dialog.s.lbl_compression; tip: dialog.s.tt_compression }
            }
        }

        // ------------------------------------------------- Station info
        ScrollView {
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: parent.width - 32
                x: 16
                y: 12
                columns: 2
                columnSpacing: 12
                rowSpacing: 10

                Label {
                    Layout.columnSpan: 2
                    text: dialog.s.lbl_station_info
                    font.bold: true
                    color: window.colourOf("heading")
                }
                FormLabel { text: dialog.s.lbl_callsign_required; font.bold: true }
                TipField {
                    id: mycall
                    hint: dialog.s.placeholder_callsign
                    tip: dialog.s.tt_callsign
                    // Capitals as you type, as in AX25Chat.
                    onTextEdited: { const p = cursorPosition; text = text.toUpperCase(); cursorPosition = p }
                }
                FormLabel { text: dialog.s.lbl_name }
                TipField { id: operatorName; hint: dialog.s.placeholder_name; tip: dialog.s.tt_name }
                FormLabel { text: dialog.s.lbl_firstname }
                TipField { id: firstName; hint: dialog.s.placeholder_firstname; tip: dialog.s.tt_firstname }
                FormLabel { text: dialog.s.lbl_qth }
                TipField { id: qth; hint: dialog.s.placeholder_qth; tip: dialog.s.tt_qth }
                FormLabel { text: dialog.s.lbl_grid_square }
                TipField {
                    id: grid
                    maximumLength: 6
                    hint: dialog.s.placeholder_grid
                    tip: dialog.s.tt_grid
                    onTextEdited: { const p = cursorPosition; text = text.toUpperCase(); cursorPosition = p }
                }
                FormLabel { text: dialog.s.lbl_rig }
                TipField { id: rig; hint: dialog.s.placeholder_rig; tip: dialog.s.tt_rig }
                FormLabel { text: dialog.s.lbl_antenna }
                TipField { id: antenna; hint: dialog.s.placeholder_antenna; tip: dialog.s.tt_antenna }
                Label {
                    Layout.columnSpan: 2
                    Layout.topMargin: 12
                    text: dialog.s.required_field_note
                    font.italic: true
                    color: window.colourOf("dim")
                }
            }
        }

        // ------------------------------------------------ Auto-responder
        ScrollView {
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width - 32
                x: 16
                y: 12
                spacing: 10
                TipCheck { id: autoEnabled; text: dialog.s.lbl_enable_auto; tip: dialog.s.tt_auto_enable }
                Label { text: dialog.s.lbl_auto_message; color: window.theme.foreground; Layout.topMargin: 6 }
                TextArea {
                    id: autoMessage
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    // An example in the empty box only, as CField does.
                    placeholderText: length === 0 && !activeFocus ? dialog.s.placeholder_auto : ""
                    hoverEnabled: true
                    Tip { shown: parent.hovered; text: dialog.s.tt_auto_message }
                }
                TipCheck { id: autoDisconnect; text: dialog.s.lbl_auto_disconnect; tip: dialog.s.tt_auto_disconnect }
                RowLayout {
                    Label { text: dialog.s.lbl_auto_delay; color: window.theme.foreground }
                    SpinBox {
                        id: autoDelay
                        from: 1; to: 60; editable: true
                        hoverEnabled: true
                        Tip { shown: parent.hovered; text: dialog.s.tt_auto_delay }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ----------------------------------------------------- Interface
        ScrollView {
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: parent.width - 32
                x: 16
                y: 12
                columns: 2
                columnSpacing: 12
                rowSpacing: 10

                Item { width: 1; height: 1 }
                TipCheck { id: timestamp; text: dialog.s.lbl_timestamp; tip: dialog.s.tt_timestamp }
                Item { width: 1; height: 1 }
                TipCheck { id: sound; text: dialog.s.lbl_sound; tip: dialog.s.tt_sound }
                Item { width: 1; height: 1 }
                TipCheck { id: saveLog; text: dialog.s.lbl_save_log; tip: dialog.s.tt_save_log }

                Item { Layout.columnSpan: 2; height: 8 }
                FormLabel { text: dialog.s.file_rx_folder }
                PathRow {
                    id: rxFolder
                    placeholder: dialog.s.file_rx_folder_placeholder
                    tip: dialog.s.tt_file_rx_path
                    onBrowse: dialog.browseFolder(rxFolder, dialog.s.file_rx_folder_browse)
                }

                Item { Layout.columnSpan: 2; height: 8 }
                FormLabel { text: dialog.s.lbl_theme }
                CCombo {
                    id: themeCombo
                    Layout.fillWidth: true
                    model: [dialog.s.theme_light, dialog.s.theme_dark, dialog.s.theme_red, dialog.s.theme_amber]
                    tip: dialog.s.tt_theme
                }
                FormLabel { text: dialog.s.lbl_language }
                CCombo {
                    id: languageCombo
                    Layout.fillWidth: true
                    model: [dialog.s.lang_auto, "English", "Français"]
                    tip: dialog.s.tt_language
                }
            }
        }

        // ----------------------------------------------------------- BBS
        ScrollView {
            clip: true
            contentWidth: availableWidth
            GridLayout {
                width: parent.width - 32
                x: 16
                y: 12
                columns: 2
                columnSpacing: 12
                rowSpacing: 10

                Item { width: 1; height: 1 }
                TipCheck { id: bbsEnabled; text: dialog.s.bbs_enable; tip: dialog.s.tt_bbs_enable }
                Item { Layout.columnSpan: 2; height: 8 }
                FormLabel { text: dialog.s.bbs_shared_folder }
                PathRow {
                    id: bbsFolder
                    placeholder: dialog.s.bbs_folder_placeholder
                    tip: dialog.s.tt_bbs_folder
                    onBrowse: dialog.browseFolder(bbsFolder, dialog.s.bbs_folder_browse)
                }
                Label {
                    Layout.columnSpan: 2
                    Layout.topMargin: 12
                    text: dialog.s.bbs_info
                    font.italic: true
                    color: window.colourOf("dim")
                }
            }
        }
    }

    footer: DialogButtonBox {
        Material.background: window.theme.surface
        CButton { text: dialog.s.btn_ok; highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        CButton { text: dialog.s.btn_cancel; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
    }
}
