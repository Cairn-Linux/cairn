// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One game on the Games page: its picture, its title, and whether it is on
// the picked child's list. A game on the list is highlighted and says so
// in words, so the colour is never the only sign.
Rectangle {
    id: card

    // appId, title, picture and onList, from GamesPage.games.
    required property var game
    // The name of the child whose list this is.
    required property string childName

    readonly property bool onList: card.game.onList === true

    height: picture.height + words.implicitHeight + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: card.onList ? Tokens.selection : Tokens.card
    border.color: card.onList ? Tokens.machine : Tokens.line
    border.width: card.onList ? Tokens.focusWidth : Tokens.strokeHairline

    Accessible.role: Accessible.ListItem
    Accessible.name: card.game.title
    Accessible.description: card.onList ? qsTr("On %1's list").arg(card.childName) : qsTr("Not on %1's list").arg(card.childName)

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
            elide: Text.ElideRight
            text: card.onList ? qsTr("On the list") : qsTr("Not on the list")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.minGuardian
            color: Tokens.paperLabel
            Accessible.ignored: true
        }
    }
}
