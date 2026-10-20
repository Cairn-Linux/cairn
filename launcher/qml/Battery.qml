// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// The battery on the bar above the tiles (ADR-0034): a drawn battery whose
// fill shows how full it is, a plug while it is on the charger, and one
// sentence when UPower says it is low. A child who cannot read sees the
// fill go down, as on a phone or a tablet; the words are for children who
// read and for grown-ups. Nothing moves, flashes or changes colour, and it
// never takes the focus: there is nothing to press.
Item {
    id: battery

    // From BatteryLook: how full, from 0 to 100.
    required property int level
    // BatteryLook.Supply and BatteryLook.Warning.
    required property int supply
    required property bool onCharger
    required property int warning

    Accessible.role: Accessible.Indicator
    Accessible.name: {
        switch (supply) {
        case BatteryLook.Charging:
            return qsTr("Battery charging, %1 percent").arg(level);
        case BatteryLook.OnCharger:
            return qsTr("Battery on the charger, %1 percent").arg(level);
        case BatteryLook.Full:
            return qsTr("Battery full");
        }
        return qsTr("Battery, %1 percent").arg(level);
    }

    Rectangle {
        id: body

        objectName: "batteryBody"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Tokens.headingSize * 2
        height: Tokens.headingSize
        radius: Tokens.radiusSm / 2
        color: Qt.alpha(Tokens.text, 0)
        border.width: Tokens.strokeHairline
        border.color: Tokens.text

        Rectangle {
            objectName: "batteryFill"
            // The inside of the outline, less a gap as wide as the outline.
            readonly property real room: body.width - 4 * Tokens.strokeHairline

            x: 2 * Tokens.strokeHairline
            y: 2 * Tokens.strokeHairline
            width: room * battery.level / 100
            height: body.height - 4 * Tokens.strokeHairline
            radius: Math.max(0, body.radius - 2 * Tokens.strokeHairline)
            color: Tokens.text
        }
    }

    // The battery's positive end.
    Rectangle {
        id: nub

        anchors.left: body.right
        anchors.verticalCenter: body.verticalCenter
        width: Tokens.strokeHairline * 2
        height: Tokens.headingSize / 2
        color: Tokens.text
    }

    // A plug with its prongs towards the battery and its cord away from it.
    // It shows while the charger is in, charging or not: a full battery, or
    // one a charge limit holds, is still on the charger.
    Item {
        id: plug

        objectName: "batteryPlug"
        readonly property real prongLength: Tokens.headingSize / 4

        anchors.left: nub.right
        anchors.leftMargin: Tokens.strokeHairline * 2
        anchors.verticalCenter: body.verticalCenter
        width: prongLength + plugBody.width + cord.width
        height: Tokens.headingSize / 2
        visible: battery.onCharger

        Repeater {
            model: 2

            Rectangle {
                required property int index

                x: 0
                y: plug.height / 4 + index * plug.height / 2 - height / 2
                width: plug.prongLength
                height: Tokens.strokeHairline
                color: Tokens.text
            }
        }

        Rectangle {
            id: plugBody

            x: plug.prongLength
            width: Tokens.headingSize / 3
            height: plug.height
            radius: Tokens.strokeHairline
            color: Tokens.text
        }

        Rectangle {
            id: cord

            anchors.left: plugBody.right
            anchors.verticalCenter: plugBody.verticalCenter
            width: Tokens.headingSize / 2
            height: Tokens.strokeHairline
            color: Tokens.text
        }
    }

    // One sentence, beside the battery, as large as the room allows and
    // never smaller than a child's smallest text, on two lines if it must.
    Text {
        id: sentence

        objectName: "batterySentence"
        anchors.left: plug.visible ? plug.right : nub.right
        anchors.leftMargin: Tokens.headingSize / 2
        anchors.right: parent.right
        height: parent.height
        visible: battery.warning !== BatteryLook.None
        text: battery.warning === BatteryLook.AlmostEmpty ? qsTr("The battery is almost empty. Ask a grown-up for the charger.") : qsTr("The battery is low. Ask a grown-up for the charger.")
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        fontSizeMode: Text.Fit
        minimumPixelSize: Tokens.minChild
        lineHeight: Tokens.headingLineHeight
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightRegular
        color: Tokens.text

        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
