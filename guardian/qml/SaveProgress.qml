// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// A Save's progress window (ADR-0041): from the password box to the end it
// covers the whole Family window, so nothing else can be changed meanwhile.
// It says what the Save is doing, names each game as it is copied, and,
// while any Save runs, warns not to log out or close the lid: logging out
// would end even a Save that only takes games off before the lists are
// written. At the end it says what the Save did, and Close puts it away.
Item {
    id: progress

    required property GamesSaving saving

    readonly property bool ended: progress.saving.step === GamesSaving.Saved || progress.saving.step === GamesSaving.NotSaved
    readonly property int margin: Tokens.headingSize
    // The room Close takes under the words, once the Save has ended.
    readonly property real closeRoom: progress.ended ? closeButton.height + Tokens.bodySize : 0

    // The window was put away; focus goes back to the page.
    signal closed

    visible: progress.saving.step !== GamesSaving.Ready

    Accessible.role: Accessible.Dialog
    Accessible.name: heading.text

    // Plain text, so a game's name from Steam shows exactly as written.
    component Line: Text {
        width: column.width
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // Each game as the copy reported it.
    function copiedSentence(game) {
        const title = game.title !== "" ? game.title : qsTr("Game %1").arg(game.appId);
        if (game.had)
            return qsTr("%1 already had %2.").arg(game.name).arg(title);
        return qsTr("Copied %1 for %2.").arg(title).arg(game.name);
    }

    function close() {
        if (!progress.ended)
            return;
        progress.saving.close();
        progress.closed();
    }

    // The box takes the keyboard when it shows, and not before: hidden, it
    // kept Tab from the page when Family opened (#242).
    onVisibleChanged: {
        if (progress.visible)
            box.forceActiveFocus();
    }
    onEndedChanged: {
        if (progress.ended)
            closeButton.forceActiveFocus();
    }

    // Under the box, the page waits: no click, tap or scroll gets through.
    Rectangle {
        objectName: "saveScrim"
        anchors.fill: parent
        color: Qt.alpha(Tokens.scrim, Tokens.scrimOpacity)
        Accessible.ignored: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onWheel: wheel => wheel.accepted = true
        }
    }

    Rectangle {
        id: box

        objectName: "saveProgressBox"
        anchors.centerIn: parent
        width: Math.min(progress.width - progress.margin * 2, Tokens.headingSize * 20)
        // The words' part, Close's room and the margins. The box is worked
        // out from the part, not the part from the box: adding the margins
        // and Close's room and then taking them away again need not give
        // the words' height back exactly, and a part a hair shorter than
        // its words scrolls and shows the bar.
        height: words.height + progress.closeRoom + progress.margin * 2
        radius: Tokens.radiusCard
        color: Tokens.card
        border.color: Tokens.line
        border.width: Tokens.strokeHairline

        // Focus stays in the box: Tab goes nowhere else, and Escape closes
        // it only once the Save has ended. Close says the same once it has
        // focus.
        Keys.onTabPressed: event => event.accepted = true
        Keys.onBacktabPressed: event => event.accepted = true
        Keys.onEscapePressed: progress.close()

        Flickable {
            id: words

            objectName: "saveWords"
            x: progress.margin
            y: progress.margin
            width: box.width - progress.margin * 2
            // As tall as the words, the very same number when they fit, up
            // to what the window leaves them: a margin above and below the
            // box, the box's own margin above and below them, and Close's
            // room. Taller words scroll, and Close stays in sight (#218).
            height: Math.min(column.implicitHeight, progress.height - progress.margin * 4 - progress.closeRoom)
            contentHeight: column.implicitHeight
            // Only words taller than their part of the box scroll.
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Column {
                id: column

                width: parent.width
                spacing: Tokens.bodySize

                Text {
                    id: heading

                    objectName: "saveProgressHeading"
                    width: column.width
                    wrapMode: Text.WordWrap
                    text: progress.saving.kind === GamesSaving.Look ? qsTr("Looking for new games") : qsTr("Saving games")
                    font.family: Tokens.fontFamily
                    font.pixelSize: Tokens.headingSize
                    font.weight: Tokens.weightBold
                    lineHeight: Tokens.headingLineHeight
                    color: Tokens.text
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Line {
                    objectName: "lidWarning"
                    visible: progress.saving.kind === GamesSaving.Save && progress.saving.busy
                    text: qsTr("Do not log out or close the lid until this is finished.")
                    font.weight: Tokens.weightBold
                }

                SaveOutcome {
                    width: column.width
                    saving: progress.saving
                }

                Repeater {
                    objectName: "copiedGames"
                    model: progress.saving.step === GamesSaving.Saving ? progress.saving.copiedSoFar : []

                    Line {
                        required property var modelData

                        text: progress.copiedSentence(modelData)
                    }
                }
            }
        }

        // The page's own bar, in the box's margin beside the words, as far
        // in from the box's edge as the page's is from the window's.
        ScrollMark {
            objectName: "saveScrollMark"
            flickable: words
            x: box.width - box.border.width - Tokens.focusOffset - width
        }

        // The arrow and page keys, for Close to pass on to the words
        // (#243).
        ScrollKeys {
            id: scrollKeys

            flickable: words
        }

        // Under the words and outside them, so Close and its focus ring
        // always show, however long the words are.
        TextButton {
            id: closeButton

            objectName: "closeSaveProgress"
            anchors.left: parent.left
            anchors.leftMargin: progress.margin
            anchors.bottom: parent.bottom
            anchors.bottomMargin: progress.margin
            visible: progress.ended
            text: qsTr("Close")
            Keys.onTabPressed: event => event.accepted = true
            Keys.onBacktabPressed: event => event.accepted = true
            Keys.onEscapePressed: progress.close()
            Keys.onPressed: event => event.accepted = scrollKeys.scroll(event.key)
            onActivated: progress.close()
        }
    }
}
