// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Greeter

// One person on the login screen: a circle in their colour with the first
// letter of their name, and the name beneath (ADR-0020).
Item {
    id: tile

    required property string displayName
    required property string initial
    required property int tint
    // Its password row is open: the ring stays, so it is clear whose it is.
    property bool chosen: false

    signal activated

    width: Tokens.displaySize * 3
    height: circle.height + Tokens.focusOffset * 2 + Tokens.headingSize / 2 + name.height

    Accessible.role: Accessible.Button
    Accessible.name: tile.displayName
    Accessible.onPressAction: tile.activated()

    Keys.onReturnPressed: tile.activated()
    Keys.onEnterPressed: tile.activated()
    Keys.onSpacePressed: tile.activated()

    function fill() {
        switch (tile.tint) {
        case FamilyModel.Make:
            return Tokens.make;
        case FamilyModel.Practice:
            return Tokens.practice;
        case FamilyModel.Machine:
            return Tokens.machine;
        }
        return Tokens.sand;
    }

    function label() {
        switch (tile.tint) {
        case FamilyModel.Make:
            return Tokens.makeLabel;
        case FamilyModel.Practice:
            return Tokens.practiceLabel;
        case FamilyModel.Machine:
            return Tokens.machineLabel;
        }
        return Tokens.sandLabel;
    }

    // The focus ring, outside the circle so it never covers the letter. The
    // login screen is Ink, so it is the ring for Ink.
    Rectangle {
        objectName: "focusRing"
        anchors.centerIn: circle
        width: circle.width + (Tokens.focusOffset + Tokens.focusWidth) * 2
        height: width
        radius: width / 2
        color: "transparent"
        border.color: Tokens.focusOnInk
        border.width: Tokens.focusWidth
        visible: tile.activeFocus || tile.chosen
    }

    Rectangle {
        id: circle

        y: Tokens.focusOffset + Tokens.focusWidth
        anchors.horizontalCenter: parent.horizontalCenter
        width: Tokens.displaySize * 2
        height: width
        radius: width / 2
        color: tile.fill()

        Text {
            anchors.centerIn: parent
            text: tile.initial
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: tile.label()
        }
    }

    // A first and last name are often wider than the tile, so the name
    // wraps onto a second line before it ends in an ellipsis. It breaks only
    // between words, as a word split with a stray letter is hard for an
    // early reader to recognise. So one word too wide for the tile ends in
    // an ellipsis, and a first word too wide runs a little into the gap
    // beside the tile.
    Text {
        id: name

        objectName: "tileName"
        anchors.top: circle.bottom
        anchors.topMargin: Tokens.headingSize / 2
        width: tile.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        text: tile.displayName
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        color: Tokens.inkLabel
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            tile.forceActiveFocus();
            tile.activated();
        }
    }
}
