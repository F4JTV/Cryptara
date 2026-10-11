// CField.qml - the text field of the program.
//
// Since Qt 6.5 the Material style draws outlined fields whose placeholder
// turns into a label floating on the top border as soon as the field holds
// text or has the focus.  Here the placeholder is only an example next to a
// label that already names the field: shown in an empty, unfocused field,
// gone otherwise, so it never floats over the field above.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import CryptaraBackend

TextField {
    id: control
    property string hint: ""
    property string tip: ""
    placeholderText: length === 0 && !activeFocus ? hint : ""
    selectByMouse: true
    hoverEnabled: true
    Tip { shown: control.hovered; text: control.tip }
}
