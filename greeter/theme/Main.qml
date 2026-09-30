// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Greeter

// The Cairn login screen (ADR-0020), after the brand guide's "Who's playing
// today?" sketch. SDDM gives it `sddm`, `userModel` and `sessionModel`. A tile
// that needs no password logs in at once; any other opens the password row.
// PAM decides who gets in; this only decides what to draw.
Rectangle {
    id: root

    // The accounts as tiles. Tests give one with a table of accounts.
    property FamilyModel family: FamilyModel {
        sourceModel: userModel
    }
    // The tile whose password row is open, or -1.
    property int askingFor: -1
    // Waiting for SDDM to answer a login.
    property bool busy: false
    property string message: ""

    width: Tokens.displaySize * 20
    height: Tokens.displaySize * 14
    color: Tokens.darkGround

    Accessible.role: Accessible.Pane
    Accessible.name: heading.text

    Connections {
        target: sddm

        function onLoginFailed() {
            root.busy = false;
            if (root.askingFor >= 0) {
                root.message = qsTr("That password did not work. After a few wrong tries, the account waits a few minutes before it lets anyone try again.");
                passwordRow.clear();
                passwordRow.takeFocus();
            } else {
                root.message = qsTr("Something needs a grown-up.");
                tiles.forceActiveFocus();
            }
        }
    }

    function logIn(loginName, password) {
        root.busy = true;
        root.message = "";
        sddm.login(loginName, password, sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0);
    }

    function choose(row) {
        if (root.busy)
            return;
        const index = root.family.index(row, 0);
        const loginName = root.family.data(index, FamilyModel.LoginNameRole);
        if (root.family.data(index, FamilyModel.AsksForPasswordRole)) {
            root.message = "";
            root.askingFor = row;
            passwordRow.clear();
            passwordRow.takeFocus();
        } else {
            root.logIn(loginName, "");
        }
    }

    function stopAsking() {
        root.askingFor = -1;
        root.message = "";
        tiles.forceActiveFocus();
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - Tokens.headingSize * 2
        spacing: Tokens.headingSize

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.headingSize / 2

            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "mark-on-ink.svg"
                sourceSize.height: Tokens.headingSize * 1.5
                Accessible.ignored: true
            }

            // The name, not a sentence: never translated.
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "cairn"
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.headingSize
                font.weight: Tokens.weightBold
                color: Tokens.inkLabel
                Accessible.ignored: true
            }
        }

        Text {
            id: heading

            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Who's playing today?")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.linkOnInk
        }

        ListView {
            id: tiles

            objectName: "familyTiles"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, contentWidth)
            height: Tokens.displaySize * 4
            orientation: ListView.Horizontal
            spacing: Tokens.headingSize
            interactive: false
            keyNavigationEnabled: true
            keyNavigationWraps: true
            focus: true
            model: root.family
            enabled: root.askingFor < 0 && !root.busy

            delegate: FamilyTile {
                required property int index

                focus: ListView.isCurrentItem
                chosen: index === root.askingFor
                onActivated: {
                    tiles.currentIndex = index;
                    root.choose(index);
                }
            }
        }

        PasswordRow {
            id: passwordRow

            objectName: "passwordRow"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.askingFor >= 0
            enabled: !root.busy
            displayName: root.askingFor >= 0 ? root.family.data(root.family.index(root.askingFor, 0), FamilyModel.DisplayNameRole) : ""
            onSubmitted: password => root.logIn(root.family.data(root.family.index(root.askingFor, 0), FamilyModel.LoginNameRole), password)
            onCancelled: root.stopAsking()
        }

        Text {
            objectName: "message"
            width: Math.min(parent.width, Tokens.displaySize * 12)
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.message
            visible: text.length > 0
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.minChild
            color: Tokens.inkLabel
        }
    }
}
