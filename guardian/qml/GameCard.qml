// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One game on the Games page: its picture, its title, and whether it is on
// the picked child's list. Tapping it, or Space or Enter, puts it on the
// list or takes it off until Save. A game on the list is highlighted and
// says so in words, so the colour is never the only sign, and a change not
// saved yet says that too.
Rectangle {
    id: card

    // appId, title, picture, saved, waiting and shared, from
    // GamesPage.games.
    required property var game
    // The name of the child whose list this is.
    required property string childName
    // Whether the game is on the list as the parent wants it now.
    required property bool onList

    // Whether the child's list as last saved has it.
    readonly property bool saved: card.game.saved === true
    // Saved, and copied the next time the child logs out.
    readonly property bool waiting: card.game.waiting === true
    // False for a game on the saved list that the shared folder no longer
    // has.
    readonly property bool shared: card.game.shared !== false

    signal turned

    // The words under the title, and the screen reader's, for each state.
    function stateWords() {
        if (!card.shared)
            return card.onList ? qsTr("On the list, no longer shared") : qsTr("Taken off, not saved yet");
        if (card.onList && card.saved && card.waiting)
            return qsTr("Waits for %1 to log out").arg(card.childName);
        if (card.onList)
            return card.saved ? qsTr("On the list") : qsTr("Added, not saved yet");
        return card.saved ? qsTr("Taken off, not saved yet") : qsTr("Not on the list");
    }

    function stateDescription() {
        if (!card.shared && card.onList)
            return qsTr("On %1's list, no longer in the shared folder").arg(card.childName);
        if (card.onList && card.saved && card.waiting)
            return qsTr("On %1's list, copied the next time %1 logs out").arg(card.childName);
        if (card.onList)
            return card.saved ? qsTr("On %1's list").arg(card.childName) : qsTr("Added to %1's list, not saved yet").arg(card.childName);
        return card.saved ? qsTr("Taken off %1's list, not saved yet").arg(card.childName) : qsTr("Not on %1's list").arg(card.childName);
    }

    height: picture.height + words.implicitHeight + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: card.onList ? Tokens.selection : Tokens.card
    border.color: card.onList ? Tokens.machine : Tokens.line
    border.width: card.onList ? Tokens.focusWidth : Tokens.strokeHairline
    activeFocusOnTab: true

    Accessible.role: Accessible.CheckBox
    Accessible.name: card.game.title
    Accessible.description: card.stateDescription()
    Accessible.checkable: true
    Accessible.checked: card.onList
    Accessible.focusable: true
    Accessible.onPressAction: card.turned()
    Accessible.onToggleAction: card.turned()

    Keys.onReturnPressed: card.turned()
    Keys.onEnterPressed: card.turned()
    Keys.onSpacePressed: card.turned()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: card.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: card.activeFocus
    }

    // A Steam header is about twice as wide as it is tall. A game without a
    // picture, or one that cannot be drawn, shows a plain pebble instead,
    // and its title below says which game it is.
    Rectangle {
        id: picture

        x: Tokens.bodySize / 2
        y: Tokens.bodySize / 2
        width: card.width - Tokens.bodySize
        height: Math.round(width * 215 / 460)
        radius: Tokens.radiusSm / 2
        color: Tokens.pebble

        Image {
            objectName: "gamePicture"
            anchors.fill: parent
            source: card.game.picture
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            sourceSize.width: width
            sourceSize.height: height
            smooth: true
            mipmap: true
            opacity: card.onList ? 1 : 0.55
            Accessible.ignored: true
        }
    }

    Column {
        id: words

        x: Tokens.bodySize / 2
        y: picture.y + picture.height + Tokens.bodySize / 2
        width: card.width - Tokens.bodySize

        // A Steam game's own name, so plain text: markup in it is shown as
        // written and never loads a picture from the network.
        Text {
            objectName: "gameTitle"
            width: parent.width
            text: card.game.title
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            font.weight: Tokens.weightBold
            color: Tokens.paperLabel
            Accessible.ignored: true
        }

        Text {
            objectName: "onListLabel"
            width: parent.width
            wrapMode: Text.WordWrap
            text: card.stateWords()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.minGuardian
            font.weight: card.onList === card.saved ? Tokens.weightRegular : Tokens.weightBold
            color: Tokens.paperLabel
            Accessible.ignored: true
        }
    }

    TapHandler {
        onTapped: {
            card.forceActiveFocus(Qt.MouseFocusReason);
            card.turned();
        }
    }
}
