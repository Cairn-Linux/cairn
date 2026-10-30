// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Guardian

// The Guardian's window for one child's Steam (ADR-0032). It names the
// child, says in plain words what is happening, and has at most two
// buttons. The child's Steam itself is in a window of its own beside it.
Window {
    id: window

    required property SteamForChild visit

    readonly property string child: visit.childName
    readonly property int margin: Tokens.headingSize
    readonly property bool asksFirst: visit.step === SteamForChild.Ready || visit.step === SteamForChild.NotOpened

    title: qsTr("Steam for %1").arg(child)
    // The size follows the words, and only the words, for every step: it
    // grows and shrinks with each message (#172). Bound as the most and the
    // least the window may be, so KWin keeps to it. Rounded up, as a
    // window's height is whole pixels and the words' height may not be.
    readonly property int wordsHeight: Math.ceil(content.implicitHeight) + margin * 2

    width: Tokens.headingSize * 22
    height: wordsHeight
    minimumWidth: width
    maximumWidth: width
    minimumHeight: wordsHeight
    maximumHeight: wordsHeight
    color: Tokens.ground
    visible: true

    // KWin can still give the window a height it worked out for the words
    // before, such as when the password box closes as the message changes.
    // Nothing in the window would then change to put it right, and the
    // buttons stay cut off (#172), so any other height is put back.
    onHeightChanged: Qt.callLater(window.fitWords)
    onWordsHeightChanged: Qt.callLater(window.fitWords)

    function fitWords() {
        if (height !== wordsHeight)
            height = wordsHeight;
    }

    onClosing: close => {
        close.accepted = visit.leave();
    }

    // A step with a button is one the Guardian needs to see, so the window
    // comes in front, such as of Steam's own window when it updates itself.
    // While the password box is up there is no button, and the box stays
    // in front.
    Connections {
        target: window.visit

        function onStepChanged() {
            if (words.mainButtonText() === "")
                return;
            window.raise();
            window.requestActivate();
        }
    }

    SteamForWords {
        id: words

        visit: window.visit
    }

    function pressMainButton() {
        switch (visit.step) {
        case SteamForChild.Ready:
        case SteamForChild.NotOpened:
            visit.open();
            break;
        case SteamForChild.Open:
            visit.done();
            break;
        case SteamForChild.Closed:
        case SteamForChild.Stuck:
            window.close();
            break;
        }
    }

    Column {
        id: content

        objectName: "content"
        x: window.margin
        y: window.margin
        width: window.width - window.margin * 2
        spacing: Tokens.bodySize

        Text {
            objectName: "heading"
            width: parent.width
            wrapMode: Text.WordWrap
            text: words.heading()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            font.weight: Tokens.weightBold
            lineHeight: Tokens.headingLineHeight
            color: Tokens.text
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Repeater {
            model: words.paragraphs()

            Text {
                required property string modelData

                objectName: "paragraph"
                width: content.width
                wrapMode: Text.WordWrap
                text: modelData
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.bodySize
                lineHeight: Tokens.bodyLineHeight
                color: Tokens.text
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }

        Row {
            objectName: "buttons"
            spacing: Tokens.bodySize
            topPadding: Tokens.bodySize
            // From the words, not from the button: a hidden row hides the
            // button too, which would then never show the row again.
            visible: words.mainButtonText() !== ""

            TextButton {
                id: mainButton

                objectName: "mainButton"
                text: words.mainButtonText()
                visible: text !== ""
                focus: true
                onActivated: window.pressMainButton()
                onVisibleChanged: {
                    if (visible)
                        forceActiveFocus();
                }
            }

            TextButton {
                objectName: "wayOutButton"
                quiet: true
                text: window.visit.step === SteamForChild.Ready ? qsTr("Cancel") : qsTr("Close")
                visible: window.asksFirst
                onActivated: window.close()
            }
        }
    }
}
