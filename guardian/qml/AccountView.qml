// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The "Your account" page: the grown-up's own picture and password
// (ADR-0043). Plasma's Users page is hidden on Cairn, so this is where a
// grown-up changes them. One change runs at a time, and every control
// waits while it does.
Column {
    id: view

    required property AccountPage page
    readonly property bool aboutPassword: view.page.step === AccountPage.Asking || view.page.step === AccountPage.PasswordChanged || view.page.step === AccountPage.PasswordNotChanged

    // An outcome has changed: the box that says it, for the window to
    // scroll to.
    signal outcomeShown(Item box)

    spacing: Tokens.bodySize

    function passwordOutcome() {
        switch (view.page.step) {
        case AccountPage.Asking:
            return qsTr("Type the password you use now in the box that opened.");
        case AccountPage.PasswordChanged:
            return qsTr("Your password is changed. Use the new one from now on: to log in, to unlock the screen and in every password box.");
        case AccountPage.PasswordNotChanged:
            return view.reason();
        }
        return "";
    }

    function reason() {
        switch (view.page.reason) {
        case AccountPage.Empty:
            return qsTr("Type the new password first.");
        case AccountPage.Different:
            return qsTr("The two new passwords are not the same. Type the new password again in the second box.");
        case AccountPage.TooLong:
            return qsTr("A password can be at most %1 characters long. Choose a shorter one.").arg(view.page.maxPasswordLength);
        case AccountPage.Unprintable:
        case AccountPage.BadPassword:
            return qsTr("A password cannot hold a tab, a line break or another character that does not show. Type it again without them.");
        case AccountPage.Dismissed:
        case AccountPage.NotAuthorized:
            return qsTr("Your password did not change. It needs the password you use now: the box was closed, or the password was not accepted.");
        case AccountPage.NotAGrownUp:
            return qsTr("Your password did not change. Only a grown-up's account can change its password here.");
        case AccountPage.NotRoot:
            return qsTr("Cairn's family command did not get the rights it needs, so your password did not change. Close Family and open it again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case AccountPage.Usage:
            return qsTr("This window asked Cairn's family command for something it does not do, so your password did not change. The window and the command may come from different versions of Cairn. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case AccountPage.Failed:
            return qsTr("Your password did not change. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        }
        return qsTr("Cairn's family command did not answer as expected, so it is not known whether your password changed. Try the new one the next time a box asks; if it is not accepted, the old one still works. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
    }

    // The picture section is a column of its own, so it is laid out
    // first, or the window would scroll to where its box used to be.
    function showOutcome() {
        pictureSection.forceLayout();
        view.outcomeShown(pictureSection.aboutPicture ? pictureSection.outcomeBox : passwordOutcomeBox);
    }

    Text {
        objectName: "accountHeading"
        width: view.width
        wrapMode: Text.WordWrap
        text: qsTr("Your account")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Row {
        spacing: Tokens.bodySize

        Rectangle {
            id: ownPicture

            readonly property bool hasPicture: view.page.picture.toString() !== ""

            objectName: "ownPicture"
            width: Tokens.headingSize * 4
            height: width
            radius: Tokens.radiusSm
            color: Tokens.card
            border.color: Tokens.line
            border.width: Tokens.strokeHairline
            Accessible.role: Accessible.Graphic
            Accessible.name: ownPicture.hasPicture ? qsTr("Your picture") : noPicture.text

            Image {
                anchors.fill: parent
                anchors.margins: Tokens.strokeHairline
                visible: ownPicture.hasPicture
                source: view.page.picture
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectCrop
                cache: false
                Accessible.ignored: true
            }

            // Words where the picture would be, so an empty frame never
            // looks like a picture that did not load.
            Text {
                id: noPicture

                objectName: "noPicture"
                anchors.fill: parent
                anchors.margins: Tokens.bodySize / 2
                visible: !ownPicture.hasPicture
                text: qsTr("No picture yet")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.bodySize
                color: Tokens.paperLabel
                Accessible.ignored: true
            }
        }

        Text {
            objectName: "ownName"
            anchors.verticalCenter: parent.verticalCenter
            text: view.page.name
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }

    PictureSection {
        id: pictureSection

        width: view.width
        page: view.page
    }

    PasswordForm {
        id: passwordForm

        objectName: "passwordForm"
        width: view.width
        topPadding: Tokens.bodySize
        page: view.page
        enabled: !view.page.busy
    }

    OutcomeBox {
        id: passwordOutcomeBox

        objectName: "passwordOutcome"
        width: view.width
        text: view.aboutPassword ? view.passwordOutcome() : ""
    }

    // Every control waits while a change runs, which takes focus away. A
    // new password is cleared once it is in place, and kept otherwise, so
    // a typing slip is mended without typing both again.
    Connections {
        target: view.page

        function onStepChanged() {
            if (view.page.step === AccountPage.PasswordChanged)
                passwordForm.clear();
            if (view.page.step === AccountPage.PasswordChanged || view.page.step === AccountPage.PasswordNotChanged)
                Qt.callLater(passwordForm.takeFocus);
            Qt.callLater(view.showOutcome);
        }
    }
}
