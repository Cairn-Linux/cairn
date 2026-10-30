// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Games page (ADR-0041): pick a child, and see every game in the
// shared folder, with the games on that child's own list highlighted.
// Tapping a game puts it on the list or takes it off, for one child or
// several, and Save gives each of them their new list with one password.
// Each child also has a way to open their own Steam. A child who is logged
// in has their games greyed and blurred under a card that offers to log
// them out first. While a Save or a logout runs, every control waits; the
// Save's own window shows how it goes.
Column {
    id: view

    required property GamesPage page

    readonly property bool hasChildren: view.page.listedChildren.length > 0
    // Games to show: the shared folder's, and any left on the child's list.
    readonly property bool hasGames: view.page.games.length > 0
    readonly property string name: view.page.chosenName
    readonly property bool busy: view.page.busy

    spacing: Tokens.bodySize

    // A paragraph of the page's own words.
    component Words: Text {
        width: view.width
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        // Plain text, so a game name from Steam shows exactly as written.
        textFormat: Text.PlainText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Text {
        objectName: "gamesHeading"
        width: view.width
        wrapMode: Text.WordWrap
        text: qsTr("Games")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Words {
        text: qsTr("Each child has their own list of the games in the shared folder. A new child starts with none. Tap a game to put it on the list or take it off, then press Save. A child sees their games the next time they log in.")
    }

    Words {
        objectName: "noChildrenForGames"
        visible: !view.hasChildren
        text: qsTr("No children yet. Add one on the Children page, then come back here for their games.")
    }

    Words {
        objectName: "noGames"
        visible: view.page.sharedCount === 0
        text: qsTr("No games in the shared folder yet. To add one, install it from your own Steam into the folder /var/lib/cairn/steam-library. If Steam's install dialog does not offer that folder, add it first in Steam's Settings, under Storage. A game shows here after the next Save, which looks at the shared folder again. With nothing to save, Look for new games does the same.")
    }

    // Games installed the usual way go to the Guardian's own Steam library,
    // where no child can have them, so the page names them and says how
    // to move them.
    Words {
        objectName: "ownSteamGames"
        visible: view.page.ownSteamGames !== ""
        text: qsTr("In your own Steam library, not the shared folder: %1. Children can have only the games in the shared folder. To move them, open Steam's Settings and choose Storage. If the folder /var/lib/cairn/steam-library is not listed there, add it as a drive. Then pick your own library in the list, tick the games, press Move and choose the shared folder. A moved game shows here after the next Save, or after Look for new games. To send future downloads to the shared folder too, make it the default drive in Storage.").arg(view.page.ownSteamGames)
    }

    Flow {
        objectName: "childChoices"
        width: view.width
        spacing: Tokens.bodySize / 2
        visible: view.hasChildren

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Whose games to show")

        Repeater {
            id: childChoiceRepeater

            objectName: "childChoiceRepeater"
            model: view.page.listedChildren

            ChildChoice {
                required property var modelData

                objectName: "choose_" + modelData.login
                name: modelData.name
                checked: view.page.chosen === modelData.login
                unsaved: view.page.unsavedLogins.indexOf(modelData.login) >= 0
                enabled: !view.busy
                onChosen: view.page.choose(modelData.login)
            }
        }
    }

    Flow {
        objectName: "saveRow"
        width: view.width
        spacing: Tokens.bodySize / 2
        visible: view.hasChildren

        TextButton {
            id: saveButton

            objectName: "saveButton"
            text: qsTr("Save")
            enabled: view.page.canSave
            onActivated: view.page.save()
        }

        TextButton {
            objectName: "putBack"
            quiet: true
            text: qsTr("Put back as saved")
            enabled: view.page.canPutBack
            onActivated: view.page.putBack()
        }

        TextButton {
            objectName: "lookForGames"
            quiet: true
            text: qsTr("Look for new games")
            enabled: !view.busy
            onActivated: view.page.lookForGames()
        }
    }

    Words {
        objectName: "unsavedSentence"
        visible: view.hasChildren && view.page.unsavedNames !== "" && !view.busy
        text: qsTr("Not saved yet: the changes for %1. Save gives each child their new list, with one password.").arg(view.page.unsavedNames)
    }

    Words {
        objectName: "loggedInUnsavedSentence"
        visible: view.hasChildren && view.page.loggedInUnsavedNames !== "" && !view.busy
        text: qsTr("Save waits: changes for a child who is logged in cannot be saved yet. Logged in: %1. Log them out first.").arg(view.page.loggedInUnsavedNames)
    }

    // The child picked: their Steam, their list and the games, greyed and
    // blurred while the child is logged in.
    ChildGames {
        id: childArea

        objectName: "childArea"
        width: view.width
        visible: view.hasChildren
        page: view.page
    }

    // Every control waits while a Save runs, which takes focus away. After
    // the Save's window is closed, focus goes to Save when there is still
    // something to save, and to the child picked otherwise.
    function restoreFocus() {
        if (saveButton.enabled) {
            saveButton.forceActiveFocus();
        } else {
            const choices = childChoiceRepeater;
            for (let i = 0; i < choices.count; ++i) {
                const choice = choices.itemAt(i) as ChildChoice;
                if (choice && choice.checked)
                    choice.forceActiveFocus();
            }
        }
    }

    // Each step of a logout moves focus to where the parent goes on: within
    // the card while it shows, back to the page once the child is out.
    function followLogOut() {
        // Picking another child also ends a warning; focus stays on the
        // choice then.
        const focused = view.Window.activeFocusItem;
        if (view.page.loggingOut.step === ChildLogOut.NotAsking && focused && focused.visible && focused.enabled)
            return;
        if (childArea.cover.visible)
            childArea.cover.takeFocus();
        else if (view.page.loggingOut.step === ChildLogOut.NotAsking)
            view.restoreFocus();
    }

    Connections {
        target: view.page.loggingOut

        function onChanged() {
            Qt.callLater(view.followLogOut);
        }
    }
}
