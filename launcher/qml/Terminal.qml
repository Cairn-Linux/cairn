// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// The restricted shell's screen: large mono type on Ink, a prompt that is
// the location, ghost completion after the cursor, and one way out. It draws
// what the session says and decides nothing (TERMINAL-DESIGN §6).
Rectangle {
    id: terminal

    required property TerminalSession session

    // Escape, or the child typed exit.
    signal exited

    color: Tokens.ink

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Terminal")

    function takeFocus() {
        input.forceActiveFocus();
    }

    function iconColor(icon) {
        switch (icon) {
        case OutputModel.Folder:
            return Tokens.sky;
        case OutputModel.Make:
            return Tokens.make;
        case OutputModel.Practice:
            return Tokens.practice;
        case OutputModel.Machine:
            return Tokens.fjord;
        case OutputModel.Note:
            return Tokens.paper;
        }
        return Qt.alpha(Tokens.ink, 0);
    }

    ListView {
        id: output

        objectName: "terminalOutput"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: promptRow.top
        anchors.margins: Tokens.headingSize
        anchors.bottomMargin: 0
        interactive: false
        clip: true
        model: terminal.session.output
        // The newest line is always in view; the child never scrolls.
        onCountChanged: positionViewAtEnd()
        onHeightChanged: positionViewAtEnd()

        delegate: Row {
            id: line

            required property string text
            required property int icon
            required property bool isInput

            width: output.width
            spacing: Tokens.terminalSize / 2

            Rectangle {
                width: Tokens.terminalSize * 0.8
                height: width
                radius: Tokens.radiusSm / 2
                anchors.verticalCenter: parent.verticalCenter
                color: terminal.iconColor(line.icon)
            }

            Text {
                width: line.width - Tokens.terminalSize * 0.8 - line.spacing
                wrapMode: Text.WordWrap
                text: line.text
                font.family: Tokens.fontFamilyMono
                font.pixelSize: Tokens.terminalSize
                lineHeight: Tokens.terminalLineHeight
                color: line.isInput ? Tokens.sky : Tokens.sand
            }
        }
    }

    Row {
        id: promptRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.headingSize
        spacing: Tokens.terminalSize / 2

        Text {
            id: prompt

            text: terminal.session.location + " >"
            font.family: Tokens.fontFamilyMono
            font.pixelSize: Tokens.terminalSize
            color: Tokens.sky
        }

        Item {
            width: promptRow.width - prompt.width - promptRow.spacing
            height: input.height

            // The rest of the most likely word, after what is typed.
            Text {
                x: input.contentWidth
                text: terminal.session.ghost(input.text)
                font: input.font
                color: Tokens.sky
                opacity: 0.6
            }

            TextInput {
                id: input

                objectName: "terminalInput"
                property int historySteps: 0

                width: parent.width
                font.family: Tokens.fontFamilyMono
                font.pixelSize: Tokens.terminalSize
                color: Tokens.sand
                selectionColor: Tokens.sky
                selectedTextColor: Tokens.ink
                cursorVisible: activeFocus
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText

                Accessible.role: Accessible.EditableText
                Accessible.name: qsTr("Type a command")

                Keys.onReturnPressed: accept()
                Keys.onEnterPressed: accept()
                Keys.onTabPressed: takeGhost()
                Keys.onEscapePressed: terminal.exited()
                Keys.onUpPressed: recall(historySteps + 1)
                Keys.onDownPressed: recall(historySteps - 1)
                Keys.onRightPressed: event => {
                    if (cursorPosition === text.length)
                        takeGhost();
                    else
                        event.accepted = false;
                }

                function accept() {
                    terminal.session.run(text);
                    text = "";
                    historySteps = 0;
                }

                function takeGhost() {
                    text = text + terminal.session.ghost(text);
                    cursorPosition = text.length;
                }

                function recall(steps) {
                    const earlier = terminal.session.recall(steps);
                    if (steps < 0)
                        return;
                    historySteps = steps;
                    text = earlier;
                    cursorPosition = text.length;
                }
            }
        }
    }
}
