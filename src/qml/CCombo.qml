// CCombo.qml - a combo box in the colours of the theme.  Material forces
// the text of a raised ComboBox to its own primary text colour (white on
// a dark theme); the theme's foreground is put back here.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import CryptaraBackend

ComboBox {
    id: control
    property string tip: ""
    Material.foreground: window.theme.foreground
    Material.background: window.theme.button
    hoverEnabled: true
    Tip { shown: control.hovered; text: control.tip }
}
