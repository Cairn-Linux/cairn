// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Cairn.Brand
import Cairn.Family

// The Games page's part for the child picked (ADR-0041): their Steam,
// their list in a sentence, and the grid of games. While the child is
// logged in, or a logout is about them, it is greyed, blurred and cannot be
// used, under the card that says why and offers to log them out.
Item {
    id: area

    required property GamesPage page
    // The card, for the page to move focus into.
    readonly property alias cover: cover

    readonly property bool covered: area.page.chosenCovered
    readonly property bool hasGames: area.page.games.length > 0
    readonly property string name: area.page.chosenName
    readonly property bool busy: area.page.busy

    height: Math.max(childGames.implicitHeight, cover.visible ? cover.height : 0)

    // A paragraph of the page's own words.
    component Words: Text {
        width: area.width
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
        if (area.page.list === GamesPage.Unreadable)
            return qsTr("%1's list of games could not be read, so it counts as a list with no games, and %1 has no game tiles. The file is /var/lib/cairn/games/%2.json. Saving %1's games on this page writes it again.").arg(area.name).arg(area.page.chosen);
        if (area.page.onListCount === 0)
            return qsTr("%1 has no games on their list yet.").arg(area.name);
        const listed = qsTr("Games on %1's list: %2. They are highlighted.").arg(area.name).arg(area.page.onListCount);
        if (area.page.goneCount === 0)
            return listed;
        return listed + " " + qsTr("The ones no longer in the shared folder come last. %1's own copy may still play them.").arg(area.name);
    }

    Column {
        id: childGames

        objectName: "childGames"
        width: area.width
        spacing: Tokens.bodySize
        enabled: !area.covered
        // The fade says it on its own where blur cannot be drawn.
        opacity: area.covered ? Tokens.coveredOpacity : 1
        layer.enabled: area.covered
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 1
            blurMax: Tokens.blurMax
        }
        Accessible.ignored: area.covered

        TextButton {
            objectName: "openSteam"
            quiet: true
            enabled: !area.busy
            text: qsTr("Open Steam for %1").arg(area.name)
            onActivated: area.page.openSteam(area.page.chosen)
        }

        Words {
            objectName: "steamNotOpened"
            visible: text !== ""
            text: area.page.steamNotOpened === "" ? "" : qsTr("The Steam window for %1 did not open. The reason is in the system log. In a terminal: journalctl -t cairn-guardian -t cairn-steam-for").arg(area.page.steamNotOpened)
            Accessible.role: Accessible.AlertMessage
        }

        Words {
            objectName: "listSentence"
            visible: area.hasGames
            text: area.listSentence()
        }

        Flow {
            id: grid

            // As many columns as fit, each at least this wide.
            readonly property real narrowest: Tokens.headingSize * 6
            readonly property int columns: Math.max(1, Math.floor((grid.width + grid.spacing) / (grid.narrowest + grid.spacing)))

            objectName: "gameGrid"
            width: area.width
            spacing: Tokens.bodySize / 2
            visible: area.hasGames

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Games in the shared folder")

            Repeater {
                objectName: "gameCards"
                model: area.page.games

                GameCard {
                    required property var modelData

                    width: (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns
                    game: modelData
                    childName: area.name
                    onList: area.page.wanted.indexOf(modelData.appId) >= 0
                    enabled: !area.busy
                    onTurned: area.page.turn(modelData.appId)
                }
            }
        }
    }

    LoggedInCover {
        id: cover

        objectName: "loggedInCover"
        width: area.width
        visible: area.covered
        page: area.page
    }
}
