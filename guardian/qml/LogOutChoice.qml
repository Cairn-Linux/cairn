// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// The question before a Save when a child with new games to copy is logged
// in (ADR-0041), as a step on the page, one child at a time: log them out
// now, or copy their games the next time they log out. Logging out now
// first warns what may be lost. LogOutQuestion decides every step; this
// only draws it. The child sees none of it.
Rectangle {
    id: box

    required property LogOutQuestion question

    readonly property string name: box.question.name
    readonly property bool warning: box.question.step === LogOutQuestion.Warning

    // Focus goes to the first button of the choice, and to "Not now" in the
    // warning, so a second Enter never logs a child out by itself.
    function focusStep() {
        if (box.warning)
            notNow.forceActiveFocus();
        else
            logOutNow.forceActiveFocus();
    }

    height: column.implicitHeight + Tokens.bodySize * 2
    visible: box.question.step !== LogOutQuestion.NotAsking
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: box.warning ? qsTr("Log %1 out now?").arg(box.name) : qsTr("%1 is logged in").arg(box.name)

    component Sentence: Text {
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    Column {
        id: column

        x: Tokens.bodySize
        y: Tokens.bodySize
        width: box.width - Tokens.bodySize * 2
        spacing: Tokens.bodySize / 2

        Sentence {
            objectName: "logOutQuestion"
            width: column.width
            visible: !box.warning
            text: qsTr("%1 is logged in.").arg(box.name) + " " + qsTr("%1's new games can be copied now, which logs %1 out, or the next time %1 logs out.").arg(box.name)
        }

        Sentence {
            objectName: "logOutWarning"
            width: column.width
            visible: box.warning
            text: qsTr("Log %1 out now?").arg(box.name) + " " + qsTr("Anything %1 has not saved in a game may be lost. Steam is asked to save first. %1's new games are copied straight after, and %1 can log in again when that ends.").arg(box.name)
        }

        Flow {
            width: column.width
            spacing: Tokens.bodySize / 2
            visible: !box.warning

            TextButton {
                id: logOutNow

                objectName: "logOutNow"
                text: qsTr("Log %1 out now").arg(box.name)
                onActivated: box.question.logOutNow()
            }

            TextButton {
                objectName: "later"
                quiet: true
                text: qsTr("Later")
                Accessible.name: qsTr("Later: copy %1's new games the next time %1 logs out").arg(box.name)
                onActivated: box.question.later()
            }
        }

        Flow {
            width: column.width
            spacing: Tokens.bodySize / 2
            visible: box.warning

            TextButton {
                objectName: "confirmLogOut"
                text: qsTr("Log %1 out").arg(box.name)
                onActivated: box.question.logOut()
            }

            TextButton {
                id: notNow

                objectName: "notNow"
                quiet: true
                text: qsTr("Not now")
                Accessible.name: qsTr("Not now: go back to the choice for %1").arg(box.name)
                onActivated: box.question.notNow()
            }
        }
    }
}
