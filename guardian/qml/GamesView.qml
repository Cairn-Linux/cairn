// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Games page (ADR-0041): pick a child, and see every game in the
// shared folder, with the games on that child's own list highlighted.
// Tapping a game puts it on the list or takes it off, for one child or
// several, and Save gives each of them their new list with one password.
// Each child also has a way to open their own Steam. While a Save runs,
// every control waits.
Column {
    id: view

    required property GamesPage page
    // The box with what the last Save did, for the window to scroll to.
    readonly property alias outcomeBox: outcome

    readonly property bool hasChildren: view.page.listedChildren.length > 0
    // Games to show: the shared folder's, and any left on the child's list.
    readonly property bool hasGames: view.page.games.length > 0
    readonly property string name: view.page.chosenName
    readonly property bool busy: view.page.busy

    // A Save has started or ended, and focus is where the parent can go on.
    signal outcomeShown

    spacing: Tokens.bodySize

    // A paragraph of the page's own words.
    component Words: Text {
        width: view.width
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // What the picked child's list holds, or why it holds nothing.
    function listSentence() {
        if (view.page.list === GamesPage.Unreadable)
            return qsTr("%1's list of games could not be read, so it counts as a list with no games, and %1 has no game tiles. The file is /var/lib/cairn/games/%2.json. Saving %1's games on this page writes it again.").arg(view.name).arg(view.page.chosen);
        if (view.page.onListCount === 0)
            return qsTr("%1 has no games on their list yet.").arg(view.name);
        const listed = qsTr("Games on %1's list: %2. They are highlighted.").arg(view.name).arg(view.page.onListCount);
        if (view.page.goneCount === 0)
            return listed;
        return listed + " " + qsTr("The ones no longer in the shared folder come last. %1's own copy may still play them.").arg(view.name);
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

    TextButton {
        objectName: "openSteam"
        visible: view.hasChildren
        quiet: true
        enabled: !view.busy
        text: qsTr("Open Steam for %1").arg(view.name)
        onActivated: view.page.openSteam(view.page.chosen)
    }

    Words {
        objectName: "steamNotOpened"
        visible: text !== ""
        text: view.page.steamNotOpened === "" ? "" : qsTr("The Steam window for %1 did not open. The reason is in the system log. In a terminal: journalctl -t cairn-guardian -t cairn-steam-for").arg(view.page.steamNotOpened)
        Accessible.role: Accessible.AlertMessage
    }

    Words {
        objectName: "listSentence"
        visible: view.hasChildren && view.hasGames
        text: view.listSentence()
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
            enabled: !view.busy && view.page.unsavedLogins.indexOf(view.page.chosen) >= 0
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

    SaveOutcome {
        id: outcome

        objectName: "saveOutcomeBox"
        width: view.width
        saving: view.page.saving
    }

    Flow {
        id: grid

        // As many columns as fit, each at least this wide.
        readonly property real narrowest: Tokens.headingSize * 6
        readonly property int columns: Math.max(1, Math.floor((grid.width + grid.spacing) / (grid.narrowest + grid.spacing)))

        objectName: "gameGrid"
        width: view.width
        spacing: Tokens.bodySize / 2
        visible: view.hasChildren && view.hasGames

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Games in the shared folder")

        Repeater {
            objectName: "gameCards"
            model: view.page.games

            GameCard {
                required property var modelData

                width: (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns
                game: modelData
                childName: view.name
                onList: view.page.wanted.indexOf(modelData.appId) >= 0
                enabled: !view.busy
                onTurned: view.page.turn(modelData.appId)
            }
        }
    }

    // Every control waits while a Save runs, which takes focus away. After
    // it, focus goes to Save when there is still something to save, and to
    // the child picked otherwise.
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
        view.outcomeShown();
    }

    Connections {
        target: view.page.saving

        function onEnded() {
            Qt.callLater(view.restoreFocus);
        }

        function onChanged() {
            if (view.page.saving.step === GamesSaving.Asking)
                Qt.callLater(view.outcomeShown);
        }
    }
}
