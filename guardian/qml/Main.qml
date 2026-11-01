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
    // The room the buttons take under the words, when the step has any.
    readonly property real buttonsRoom: buttons.visible ? buttons.height + Tokens.bodySize : 0
    // The size follows the words, and only the words, for every step: it
    // grows and shrinks with each message (#172). Rounded up, as a window's
    // height is whole pixels and the words' height may not be.
    readonly property int wordsHeight: Math.ceil(content.implicitHeight + buttonsRoom) + margin * 2
    // The screen's height free of panels, as Qt gives it.
    property int screenHeight: Screen.desktopAvailableHeight
    // But never taller than the screen has room for, such as at 200% on a
    // small screen: then the words scroll above the buttons (#231). Room is
    // kept for KWin's title bar, which is not part of the window's height,
    // and for Plasma's panel, which Qt under Wayland counts as free. Bound
    // as the most and the least the window may be, so KWin keeps to it.
    readonly property int fitHeight: Math.min(wordsHeight, screenHeight - Tokens.headingSize * 3)

    width: Tokens.headingSize * 22
    height: fitHeight
    minimumWidth: width
    maximumWidth: width
    minimumHeight: fitHeight
    maximumHeight: fitHeight
    color: Tokens.ground
    visible: true

    // KWin can still give the window a height it worked out for the words
    // before, such as when the password box closes as the message changes.
    // Nothing in the window would then change to put it right, and the
    // buttons stay cut off (#172), so any other height is put back.
    onHeightChanged: Qt.callLater(window.fitWords)
    onFitHeightChanged: Qt.callLater(window.fitWords)

    function fitWords() {
        if (height !== fitHeight)
            height = fitHeight;
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

    // The words. When they are taller than the window has room for, they
    // scroll, and the buttons stay in sight under them (#231).
    Flickable {
        id: scroller

        objectName: "wordsScroller"
        x: window.margin
        y: window.margin
        width: window.width - window.margin * 2
        height: window.height - window.margin * 2 - window.buttonsRoom
        contentHeight: content.implicitHeight
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: content

            objectName: "content"
            width: scroller.width
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
        }
    }

    // Where the words are, when there are more than fit, as on the Family
    // window's page.
    ScrollMark {
        objectName: "scrollIndicator"
        flickable: scroller
        x: window.width - width - Tokens.focusOffset
    }

    Row {
        id: buttons

        objectName: "buttons"
        anchors.left: parent.left
        anchors.leftMargin: window.margin
        anchors.bottom: parent.bottom
        anchors.bottomMargin: window.margin
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
