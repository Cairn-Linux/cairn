// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

Rectangle {
    id: tile

    required property string title
    required property int kind
    required property string accessibleName

    radius: Tokens.radiusTile
    color: {
        switch (kind) {
        case TileModel.Make:
            return Tokens.make;
        case TileModel.Practice:
            return Tokens.practice;
        case TileModel.Machine:
            return Tokens.machine;
        }
        return Tokens.make;
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: tile.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: tile.activeFocus
    }

    Text {
        anchors.centerIn: parent
        text: tile.title
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        color: {
            switch (tile.kind) {
            case TileModel.Make:
                return Tokens.makeLabel;
            case TileModel.Practice:
                return Tokens.practiceLabel;
            case TileModel.Machine:
                return Tokens.machineLabel;
            }
            return Tokens.makeLabel;
        }
    }

    TapHandler {
        onTapped: tile.forceActiveFocus(Qt.MouseFocusReason)
    }
}
