// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

Rectangle {
    id: tile

    required property string title
    required property int kind
    required property string accessibleName
    // A picture of what the tile opens, such as a game's own box art, so a
    // child who cannot read yet can tell one game from another (#138,
    // ADR-0035). Empty shows the title instead.
    property url picture
    // A game that starts in Steam, while Steam updates itself (ADR-0061):
    // its picture or title fades and the mark builds over the middle of
    // the tile. A press then opens nothing; the launcher decides that.
    property bool waitsForSteam: false
    // Where the building stones are, from the one clock that steps every
    // waiting tile, so they all build in step.
    property int stoneStep: 0

    // Enter, Space or a click. The launcher decides what happens next.
    signal activated

    // A held key repeats. Only the first press counts, so a child holding
    // Enter on Music does not flip between it and Back many times a second.
    function keyPressed(event) {
        if (!event.isAutoRepeat)
            tile.activated();
    }

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
    Accessible.description: waitsForSteam ? qsTr("Waits for a Steam update. It opens when Steam is done.") : ""
    Accessible.focusable: true
    Accessible.onPressAction: tile.activated()

    Keys.onReturnPressed: event => tile.keyPressed(event)
    Keys.onEnterPressed: event => tile.keyPressed(event)
    Keys.onSpacePressed: event => tile.keyPressed(event)

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: tile.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: tile.activeFocus
    }

    // Inside the rounded corners on every side, so the picture's square
    // corners never cross the card's. Decoded at the size it is drawn.
    Image {
        id: pictureImage

        objectName: "tilePicture"
        anchors.fill: parent
        anchors.margins: Tokens.radiusTile
        source: tile.picture
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        asynchronous: true
        sourceSize.width: width
        sourceSize.height: height
        smooth: true
        mipmap: true
        opacity: tile.waitsForSteam ? Tokens.coveredOpacity : 1
        Accessible.ignored: true

        Behavior on opacity {
            NumberAnimation {
                duration: Tokens.motionStone
            }
        }
    }

    // A game's title is the store's full name, so it wraps inside the tile
    // and ends in an ellipsis rather than running into the next tile. Only
    // the width is limited: Log out is a tile one line tall. A picture takes
    // its place; a picture that cannot be drawn gives it back, so a tile is
    // never blank. A title comes from a Steam game's own name, so it is plain
    // text: markup in it is shown as written and never loads a picture from
    // the network.
    Text {
        objectName: "tileTitle"
        visible: tile.picture.toString() === "" || pictureImage.status === Image.Error
        anchors.centerIn: parent
        width: parent.width - 2 * Tokens.headingSize
        text: tile.title
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
        lineHeight: Tokens.headingLineHeight
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        opacity: tile.waitsForSteam ? Tokens.coveredOpacity : 1
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

        Behavior on opacity {
            NumberAnimation {
                duration: Tokens.motionStone
            }
        }
    }

    // The mark with its clear space, at half the starting screen's size,
    // centred on the tile. On the laptop's partly shown row its bottom is
    // cut off; the faded picture still marks the tile there.
    Rectangle {
        id: chip

        objectName: "steamChip"
        // Pixels per unit of the mark's own box: half the starting screen's.
        readonly property real unit: Tokens.displaySize * 3 / 2 / Tokens.markHeight

        anchors.centerIn: parent
        width: (stones.stonesRight - stones.stonesLeft + 2 * stones.clearSpace) * unit
        height: (stones.stonesBottom - stones.stonesTop + 2 * stones.clearSpace) * unit
        radius: Tokens.radiusSm
        color: Tokens.pebble
        // No edge: on Pebble one would add nothing a child could see.
        border.width: 0
        opacity: tile.waitsForSteam ? 1 : 0
        visible: opacity > 0
        Accessible.ignored: true

        Behavior on opacity {
            NumberAnimation {
                duration: Tokens.motionStone
            }
        }

        BuildingStones {
            id: stones

            objectName: "steamChipStones"
            x: (clearSpace - stonesLeft) * unit
            y: (clearSpace - stonesTop) * unit
            unit: chip.unit
            step: tile.stoneStep
        }
    }

    TapHandler {
        onTapped: {
            tile.forceActiveFocus(Qt.MouseFocusReason);
            tile.activated();
        }
    }
}
