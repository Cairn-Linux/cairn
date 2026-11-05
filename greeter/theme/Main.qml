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
    // Whether faillock has locked the account whose password row is open
    // (ADR-0052). Tests give it their own settings, folder and time.
    property AccountLock accountLock: AccountLock {}
    // Why a login failed. Tests give it another list of children on a visit.
    property FailedLogin failedLogin: FailedLogin {
        accountLock: root.accountLock
    }
    // The tile whose password row is open, or -1.
    property int askingFor: -1
    // Waiting for SDDM to answer a login, and whose.
    property bool busy: false
    property string loggingIn: ""
    property string message: ""

    width: Tokens.displaySize * 20
    height: Tokens.displaySize * 14
    color: Tokens.darkGround

    Accessible.role: Accessible.Pane
    Accessible.name: heading.text

    Connections {
        target: sddm

        // SDDM sends this twice for each try; both give the same words.
        function onLoginFailed() {
            root.busy = false;
            switch (root.failedLogin.reasonFor(root.loggingIn, root.askingFor >= 0)) {
            case FailedLogin.Locked:
                root.askAgain(root.lockNotice());
                break;
            case FailedLogin.SteamVisit:
                root.askingFor = -1;
                root.accountLock.loginName = "";
                root.message = qsTr("A grown-up is setting something up for you. Try again soon.");
                tiles.forceActiveFocus();
                break;
            case FailedLogin.TryAgain:
                root.askAgain(qsTr("That password did not work. Try again."));
                break;
            case FailedLogin.OneTryLeft:
                // Always 2 or more: the theme loads no translation, so the
                // English must be right as written.
                root.askAgain(qsTr("That password did not work. One more wrong try locks this account for %n minutes.", "", root.accountLock.lockMinutes));
                break;
            case FailedLogin.OneTryLeftNoTime:
                root.askAgain(qsTr("That password did not work. One more wrong try locks this account."));
                break;
            case FailedLogin.WrongPassword:
                root.askAgain(qsTr("That password did not work. After a few wrong tries, the account waits a few minutes before it lets anyone try again."));
                break;
            case FailedLogin.NeedsGrownUp:
                root.message = qsTr("Something needs a grown-up.");
                tiles.forceActiveFocus();
                break;
            }
        }
    }

    // While a locked account's password row is open, the words count down
    // by the minute and say when a try is allowed again.
    Connections {
        target: root.accountLock

        function onStateChanged() {
            if (root.askingFor < 0 || root.busy)
                return;
            const notice = root.lockNotice();
            if (notice.length > 0)
                root.message = notice;
            if (root.accountLock.state === AccountLock.OpenAgain)
                passwordRow.takeFocus();
        }
    }

    // What the screen says about the lock, or nothing. The password row stays
    // open throughout: PAM is the gate, and a try while locked costs nothing.
    // The minutes are always 2 or more, so "%n minutes" is right as written.
    function lockNotice() {
        switch (root.accountLock.state) {
        case AccountLock.Locked:
            return qsTr("This account is locked after too many wrong passwords. Try again in %n minutes.", "", root.accountLock.minutesLeft);
        case AccountLock.LockedUnderAMinute:
            return qsTr("This account is locked after too many wrong passwords. Try again in less than a minute.");
        case AccountLock.LockedUntilReset:
            return qsTr("This account is locked after too many wrong passwords. A grown-up can unlock it.");
        case AccountLock.LockedForUnknownTime:
            return qsTr("This account is locked after too many wrong passwords. Try again later.");
        case AccountLock.OpenAgain:
            return qsTr("You can try again now.");
        default:
            return "";
        }
    }

    function askAgain(text) {
        root.message = text;
        passwordRow.clear();
        passwordRow.takeFocus();
    }

    function logIn(loginName, password) {
        root.busy = true;
        root.loggingIn = loginName;
        root.message = "";
        sddm.login(loginName, password, sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0);
    }

    function choose(row) {
        if (root.busy)
            return;
        const index = root.family.index(row, 0);
        const loginName = root.family.data(index, FamilyModel.LoginNameRole);
        if (root.family.data(index, FamilyModel.AsksForPasswordRole)) {
            root.askingFor = row;
            root.accountLock.loginName = loginName;
            // Said before anyone types; the row stays open all the same.
            root.message = root.lockNotice();
            passwordRow.clear();
            passwordRow.takeFocus();
        } else {
            root.logIn(loginName, "");
        }
    }

    function stopAsking() {
        root.askingFor = -1;
        root.accountLock.loginName = "";
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
            // Never shorter than its tallest tile: in some fonts a name on
            // two lines needs more than the usual height.
            height: Math.max(Tokens.displaySize * 4, contentItem.childrenRect.height)
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
            Accessible.role: Accessible.StaticText
            Accessible.name: text
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.minChild
            color: Tokens.inkLabel
        }
    }
}
