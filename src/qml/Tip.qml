// Tip.qml - a tooltip in the colours of the theme.  Material draws its own
// in a fixed grey, which would break the red night theme.  Declared inside
// a control:  Tip { shown: hovered; text: ... }
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import CryptaraBackend

ToolTip {
    id: tip
    property bool shown: false
    // Not for a control whose dialog has closed under the mouse.
    visible: shown && Controller.tooltips && text.length > 0 && parent !== null && parent.visible
    delay: 600
    timeout: 8000
    contentItem: Text {
        text: tip.text
        font: tip.font
        color: window.theme.tipText
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        color: window.theme.tip
        border.color: window.theme.line
        radius: 3
    }
}
