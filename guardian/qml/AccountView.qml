// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The "Your account" page: the grown-up's own picture and password
// (ADR-0043). Plasma's Users page is hidden on Cairn, so this is where a
// grown-up changes them, and where a new password brings the offer to
// change KDE Wallet's too. One change runs at a time, and every control
// waits while it does.
Column {
    id: view

    required property AccountPage page

    // An outcome has changed: the box that says it, for the window to
    // scroll to.
    signal outcomeShown(Item box)

    spacing: Tokens.bodySize

    // The picture section and the password's answers are columns of their
    // own, so they are laid out first, or the window would scroll to where
    // a box used to be.
    function show(box) {
        pictureSection.forceLayout();
        passwordAnswers.forceLayout();
        view.outcomeShown(box);
    }

    function showOutcome() {
        view.show(pictureSection.aboutPicture ? pictureSection.outcomeBox : passwordAnswers.outcomeBox);
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

    PasswordAnswers {
        id: passwordAnswers

        width: view.width
        page: view.page
        // Focus goes back to the form's button, not to a field: a key held
        // down on the card repeats, and a field would type the repeats, or
        // take Enter and send an empty form.
        onWalletAnswered: Qt.callLater(passwordForm.takeFocusOnButton)
        onWalletShown: box => view.show(box)
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
