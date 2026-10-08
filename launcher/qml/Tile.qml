// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

Rectangle {
    id: tile

    required property string title
    required property int kind
    required property string accessibleName

    // Enter, Space or a click. The launcher decides what happens next.
    signal activated

    radius: Tokens.radiusTile
    color: {
        switch (kind) {
        case TileModel.Make:
            return Tokens.make;
        case TileModel.Practice:
            return Tokens.practice;
        case TileModel.Games:
            return Tokens.games;
        case TileModel.Machine:
            return Tokens.machine;
        }
        return Tokens.make;
    }
    // A games tile is a quiet card, so its edge keeps it a tile on the ground.
    border.width: kind === TileModel.Games ? Tokens.strokeHairline : 0
    border.color: Tokens.gamesEdge

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: true
    Accessible.onPressAction: tile.activated()

    Keys.onReturnPressed: tile.activated()
    Keys.onEnterPressed: tile.activated()
    Keys.onSpacePressed: tile.activated()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: tile.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: tile.activeFocus
    }

    // A game's title is the store's full name, so it wraps inside the tile
    // and ends in an ellipsis rather than running into the next tile. Only
    // the width is limited: Log out is a tile one line tall.
    Text {
        objectName: "tileTitle"
        anchors.centerIn: parent
        width: parent.width - 2 * Tokens.headingSize
        text: tile.title
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
        lineHeight: Tokens.headingLineHeight
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        color: {
            switch (tile.kind) {
            case TileModel.Make:
                return Tokens.makeLabel;
            case TileModel.Practice:
                return Tokens.practiceLabel;
            case TileModel.Games:
                return Tokens.gamesLabel;
            case TileModel.Machine:
                return Tokens.machineLabel;
            }
            return Tokens.makeLabel;
        }
    }

    TapHandler {
        onTapped: {
            tile.forceActiveFocus(Qt.MouseFocusReason);
            tile.activated();
        }
    }
}
