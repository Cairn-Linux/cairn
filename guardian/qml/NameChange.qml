// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// Changing a child's name, inside their row on the Children page: the new
// name, and a button to save it or to leave the name as it is. Only a
// grown-up changes a child's name (ADR-0056).
Column {
    id: change

    // The child's name now, for the screen reader.
    required property string childName
    property alias text: field.text

    signal chosen(string name)
    signal cancelled

    function takeFocus() {
        field.takeFocus();
    }

    spacing: Tokens.bodySize / 2

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Change the name of %1").arg(change.childName)

    // From the field or either button.
    Keys.onEscapePressed: change.cancelled()

    LabelledField {
        id: field

        objectName: "newNameField"
        width: change.width
        label: qsTr("New name")
        hint: qsTr("The name on their login tile. Children cannot change it themselves.")
        onSubmitted: change.chosen(field.text)
    }

    Row {
        spacing: Tokens.bodySize / 2

        TextButton {
            objectName: "saveName"
            text: qsTr("Save name")
            Accessible.name: qsTr("Save the new name for %1").arg(change.childName)
            onActivated: change.chosen(field.text)
        }

        TextButton {
            objectName: "cancelName"
            quiet: true
            text: qsTr("Cancel")
            Accessible.name: qsTr("Cancel, and keep the name %1").arg(change.childName)
            onActivated: change.cancelled()
        }
    }
}
