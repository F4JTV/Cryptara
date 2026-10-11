// CButton.qml - the raised button of every theme: its own face colour, so
// the window background (Material.background) does not flatten it, and
// a "lit" state for toggles (the encryption button).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import CryptaraBackend

Button {
    id: control
    property bool lit: false
    property string tip: ""

    Material.elevation: lit ? 4 : 2
    // lit: a toggle that is on; highlighted: the default button of a dialog.
    Material.background: lit ? window.theme.lit : highlighted ? window.theme.strong : window.theme.button
    Material.foreground: lit ? window.theme.onLit : highlighted ? window.theme.onStrong : window.theme.buttonText
    font.bold: lit
    // Desktop buttons: "Call", not Material's "CALL" (shorter, easier to read).
    font.capitalization: Font.MixedCase

    hoverEnabled: true
    Tip { shown: control.hovered; text: control.tip }
}
