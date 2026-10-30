// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Dialogs
import Cairn.Brand
import Cairn.Family

// The grown-up's own picture: one of Plasma's, or a file of their own,
// and the box that says how the change went.
Column {
    id: section

    required property AccountPage page
    readonly property bool aboutPicture: section.page.step === AccountPage.SettingPicture || section.page.step === AccountPage.PictureChanged || section.page.step === AccountPage.PictureNotChanged
    readonly property Item outcomeBox: pictureOutcomeBox

    spacing: Tokens.bodySize

    function outcome() {
        switch (section.page.step) {
        case AccountPage.SettingPicture:
            return qsTr("Saving the picture.");
        case AccountPage.PictureChanged:
            return qsTr("Your picture is changed.");
        case AccountPage.PictureNotChanged:
            if (section.page.reason === AccountPage.PictureUnreadable)
                return qsTr("That file is not a picture this window can read, so your picture stays as it was. Choose a PNG or JPEG picture.");
            return qsTr("Your picture stays as it was: the computer's account service did not take it. Trying again is safe. The reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        }
        return "";
    }

    Text {
        width: section.width
        wrapMode: Text.WordWrap
        text: qsTr("Picture")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Text {
        width: section.width
        wrapMode: Text.WordWrap
        text: qsTr("Pick one of these, or a picture of your own. Plasma shows it on the lock screen.")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Flow {
        objectName: "pictureChoices"
        width: section.width
        spacing: Tokens.bodySize / 2
        enabled: !section.page.busy

        Repeater {
            objectName: "pictureTiles"
            model: section.page.pictures

            PictureTile {
                required property var modelData

                source: modelData.url
                name: modelData.name
                onChosen: section.page.choosePicture(modelData.url)
            }
        }
    }

    TextButton {
        objectName: "ownPictureButton"
        text: qsTr("Choose a picture of your own")
        enabled: !section.page.busy
        onActivated: fileDialog.open()
    }

    OutcomeBox {
        id: pictureOutcomeBox

        objectName: "pictureOutcome"
        width: section.width
        text: section.aboutPicture ? section.outcome() : ""
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Choose a picture of your own")
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp *.gif)")]
        onAccepted: section.page.choosePicture(fileDialog.selectedFile)
    }
}
