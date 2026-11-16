// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The box on the Children page that says what the last change did, or why
// nothing changed. The list above it always shows what is in place now.
// Hidden while there is nothing to say.
Rectangle {
    id: box

    required property ChildrenPage page

    // In one or two sentences.
    function outcome() {
        const name = box.page.subject;
        const level = box.page.level;
        switch (box.page.step) {
        case ChildrenPage.Asking:
            return qsTr("Confirm with your password, or your fingerprint, in the box that opened.");
        case ChildrenPage.Added:
            return qsTr("%1 can log in now, at level %2. %1 starts with no games: choose them on the Games page.").arg(name).arg(level);
        case ChildrenPage.LevelChanged:
            return qsTr("%1 is at level %2 now. It starts the next time %1 logs in.").arg(name).arg(level);
        case ChildrenPage.NameChanged:
            return qsTr("%1 is called %2 now, on the login screen and in Family.").arg(name).arg(box.page.newName);
        case ChildrenPage.LanguageChanged:
            return qsTr("%1's language is %2 now. It starts the next time %1 logs in.").arg(name).arg(box.page.newLanguage);
        case ChildrenPage.NotChanged:
            return box.reason();
        }
        return "";
    }

    // A refused name names the child or grown-up who has it, as the list
    // and the login screen show them.
    function reason() {
        const name = box.page.subject;
        switch (box.page.reason) {
        case ChildrenPage.NoName:
            return qsTr("Type the child's name first.");
        case ChildrenPage.NoShortName:
            return qsTr("Type a short name for the account as well, with letters a to z.");
        case ChildrenPage.SameName:
            return qsTr("%1 already has an account on this computer. To add another child, give them a different name, such as one with an initial.").arg(box.page.clash);
        case ChildrenPage.AGrownUpsName:
            return qsTr("%1 is the name of a grown-up on this computer. A child with a grown-up's name looks like them on every screen, so choose a different one.").arg(box.page.clash);
        case ChildrenPage.AnotherChilds:
            return qsTr("Another child is called %1 already. Two children with one name look the same on every screen, so choose a different one, such as one with an initial.").arg(box.page.clash);
        case ChildrenPage.NameAsBefore:
            return qsTr("%1 is the name already. Type a different one, or press Cancel.").arg(box.page.newName);
        case ChildrenPage.LanguageAsBefore:
            return qsTr("%1 is the language already. Choose a different one, or press Cancel.").arg(box.page.newLanguage);
        case ChildrenPage.BadLanguage:
            return qsTr("Cairn's family command does not offer that language, so nothing changed. The window and the command may come from different versions of Cairn. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case ChildrenPage.BadFullName:
            return qsTr("A name cannot hold a colon, a comma, a backslash or a line break. Change the name and try again.");
        case ChildrenPage.BadName:
            return qsTr("The short name can hold only letters a to z, digits, - and _, and starts with a letter. Change it and try again.");
        case ChildrenPage.BadLevel:
            return qsTr("Only levels 1 and 2 can be given for now. Levels 3 and 4 have no session yet.");
        case ChildrenPage.Taken:
            return qsTr("Another account on this computer has that short name. Press Add again to use a different one.");
        case ChildrenPage.NotAChild:
            return qsTr("%1 is not a child's account on this computer. The list shows the children there are now.").arg(name);
        case ChildrenPage.NotRoot:
            return qsTr("Cairn's family command did not get the rights it needs, so it changed nothing. Close Family and open it again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case ChildrenPage.Usage:
            return qsTr("This window asked Cairn's family command for something it does not do, so nothing changed. The window and the command may come from different versions of Cairn. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case ChildrenPage.NotAGrownUp:
            return qsTr("This account may not change the family. A grown-up's account can.");
        case ChildrenPage.Dismissed:
        case ChildrenPage.NotAuthorized:
            return qsTr("Nothing changed. It needs your password: the box was closed, or the password was not accepted.");
        case ChildrenPage.Busy:
            return qsTr("Another change to the family was still under way, so nothing changed. Try again in a moment.");
        case ChildrenPage.Failed:
            return qsTr("The change did not finish. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        }
        return qsTr("Cairn's family command did not answer as expected. The list shows what is in place now. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
    }

    height: outcomeText.implicitHeight + Tokens.bodySize * 2
    visible: outcomeText.text !== ""
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    Text {
        id: outcomeText

        objectName: "outcome"
        x: Tokens.bodySize
        y: Tokens.bodySize
        width: parent.width - Tokens.bodySize * 2
        wrapMode: Text.WordWrap
        text: box.outcome()
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}
