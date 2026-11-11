// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand

// Choosing a child's language, inside their row on the Children page: a
// button for each language Family offers them, and Cancel. Only a grown-up
// picks a child's language (ADR-0058).
Column {
    id: change

    // The child's name now, for the screen reader.
    required property string childName
    // Each a map of locale and name, from the child's otherLanguages.
    required property var languages

    signal chosen(string locale)
    signal cancelled

    function takeFocus() {
        const first = languageRepeater.itemAt(0);
        if (first)
            first.forceActiveFocus(Qt.TabFocusReason);
        else
            cancelLanguage.forceActiveFocus(Qt.TabFocusReason);
    }

    spacing: Tokens.bodySize / 2

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Choose a language for %1").arg(change.childName)

    // From any button.
    Keys.onEscapePressed: change.cancelled()

    Text {
        objectName: "languageHint"
        width: change.width
        wrapMode: Text.WordWrap
        text: qsTr("Their screens are in this language from the next time they log in. The way to change it is hidden from children.")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Flow {
        width: change.width
        spacing: Tokens.bodySize / 2

        Repeater {
            id: languageRepeater

            model: change.languages

            TextButton {
                required property var modelData

                objectName: "language_" + modelData.locale
                text: modelData.name
                Accessible.name: qsTr("Use %1 for %2").arg(modelData.name).arg(change.childName)
                onActivated: change.chosen(modelData.locale)
            }
        }

        TextButton {
            id: cancelLanguage

            objectName: "cancelLanguage"
            quiet: true
            text: qsTr("Cancel")
            Accessible.name: qsTr("Cancel, and keep the language of %1").arg(change.childName)
            onActivated: change.cancelled()
        }
    }
}
