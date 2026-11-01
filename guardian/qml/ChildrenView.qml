// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Children page: each child with their level, a way to move a child to
// another level, and a way to add one. Raising and lowering a level look
// and read the same (DESIGN §3). One change runs at a time, and every
// control waits while it does.
Column {
    id: view

    required property ChildrenPage page
    // The box with what the last change did, for the window to scroll to.
    readonly property alias outcomeBox: outcomeBox
    // The login of the child whose level the parent changed last, or ""
    // when the last change came from the Add form. Focus goes back there.
    property string changing: ""

    // The outcome has changed, and focus is back where the parent was.
    signal outcomeShown

    spacing: Tokens.bodySize

    // What the last change did, or why nothing changed, in one or two
    // sentences. The list below always shows what is in place now.
    function outcome() {
        const name = view.page.subject;
        const level = view.page.level;
        switch (view.page.step) {
        case ChildrenPage.Asking:
            return qsTr("Confirm with your password, or your fingerprint, in the box that opened.");
        case ChildrenPage.Added:
            return qsTr("%1 can log in now, at level %2. %1 starts with no games: choose them on the Games page.").arg(name).arg(level);
        case ChildrenPage.LevelChanged:
            return qsTr("%1 is at level %2 now. It starts the next time %1 logs in.").arg(name).arg(level);
        case ChildrenPage.NotChanged:
            return view.reason();
        }
        return "";
    }

    function reason() {
        const name = view.page.subject;
        switch (view.page.reason) {
        case ChildrenPage.NoName:
            return qsTr("Type the child's name first.");
        case ChildrenPage.NoShortName:
            return qsTr("Type a short name for the account as well, with letters a to z.");
        case ChildrenPage.SameName:
            return qsTr("%1 already has an account on this computer. To add another child, give them a different name, such as one with an initial.").arg(name);
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
        case ChildrenPage.Failed:
            return qsTr("The change did not finish. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        }
        return qsTr("Cairn's family command did not answer as expected. The list shows what is in place now. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
    }

    Text {
        objectName: "heading"
        width: view.width
        wrapMode: Text.WordWrap
        text: qsTr("Children")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Text {
        width: view.width
        wrapMode: Text.WordWrap
        text: qsTr("Each child has their own login and their own level. A level can go up or down at any time, whatever fits now. Every change asks for your password.")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Text {
        objectName: "noChildren"
        width: view.width
        wrapMode: Text.WordWrap
        visible: view.page.listedChildren.length === 0
        text: qsTr("No children yet. Add one below.")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Repeater {
        id: rows

        objectName: "childRows"
        model: view.page.listedChildren

        ChildRow {
            required property var modelData

            width: view.width
            child: modelData
            enabled: !view.page.busy
            onLevelChosen: level => {
                view.changing = modelData.login;
                view.page.changeLevel(modelData.login, level);
            }
        }
    }

    // The outcome sits between the list and the form, near both.
    Rectangle {
        id: outcomeBox

        width: view.width
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
            text: view.outcome()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.paperLabel
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    AddChildForm {
        id: form

        objectName: "addChildForm"
        width: view.width
        page: view.page
        enabled: !view.page.busy
        onSubmitting: view.changing = ""
    }

    // Every control waits while a change runs, which takes focus away, and
    // the list is made again after it. So focus goes back to where the
    // parent was: the child's row, or the form, at the field to change
    // when the form was refused. A refusal leaves the form as it was, so
    // the parent can try again.
    function restoreFocus() {
        if (view.changing !== "") {
            for (let i = 0; i < rows.count; ++i) {
                const row = rows.itemAt(i) as ChildRow;
                if (row && row.child.login === view.changing) {
                    row.takeFocus();
                    view.outcomeShown();
                    return;
                }
            }
        }
        if (view.changing === "" && view.page.reason === ChildrenPage.NoShortName)
            form.focusShortName();
        else
            form.focusName();
        view.outcomeShown();
    }

    Connections {
        target: view.page

        function onStepChanged() {
            if (view.page.step === ChildrenPage.Added)
                form.clear();
            // Later, once the controls are enabled again.
            if (view.page.step === ChildrenPage.Asking)
                Qt.callLater(view.outcomeShown);
            else
                Qt.callLater(view.restoreFocus);
        }
    }
}
