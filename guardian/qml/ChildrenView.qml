// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Children page: each child with their level, a way to move a child to
// another level or change their name or language, and a way to add one. Raising and
// lowering a level look and read the same (DESIGN §3). One change runs at
// a time, and every control waits while it does.
Column {
    id: view

    required property ChildrenPage page
    // The box with what the last change did, for the window to scroll to.
    readonly property alias outcomeBox: outcomeBox
    // The login of the child whose level, name or language the parent
    // changed last, or "" when the last change came from the Add form.
    // Focus goes back there: to the new name's field or Change name after a
    // name, to the languages or Change language after a language.
    property string changing: ""
    property bool changingName: false
    property bool changingLanguage: false
    // The login of the child whose new name is being typed, or "", and
    // what is typed so far. Kept here, not in the row, because the list is
    // made again after every change, and a refused name stays to mend.
    property string renaming: ""
    property string typedName: ""
    // The login of the child whose languages are open, or "". One field or
    // the other is open at a time.
    property string choosingLanguage: ""

    // The outcome has changed, and focus is back where the parent was.
    signal outcomeShown

    spacing: Tokens.bodySize

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
            renaming: view.renaming === modelData.login
            typedName: view.typedName
            choosingLanguage: view.choosingLanguage === modelData.login
            onLevelChosen: level => {
                // A level chosen leaves the name and language as they are.
                view.renaming = "";
                view.choosingLanguage = "";
                view.changing = modelData.login;
                view.changingName = false;
                view.changingLanguage = false;
                view.page.changeLevel(modelData.login, level);
            }
            onRenameOpened: {
                if (view.renaming !== modelData.login)
                    view.typedName = modelData.name;
                view.renaming = modelData.login;
                view.choosingLanguage = "";
            }
            onTypedNameEdited: name => view.typedName = name
            onNameChosen: name => {
                view.changing = modelData.login;
                view.changingName = true;
                view.changingLanguage = false;
                view.page.changeName(modelData.login, name);
            }
            onRenameClosed: {
                view.renaming = "";
                view.page.forgetRefusal();
            }
            onLanguageOpened: {
                view.renaming = "";
                view.choosingLanguage = modelData.login;
            }
            onLanguageChosen: locale => {
                view.changing = modelData.login;
                view.changingName = false;
                view.changingLanguage = true;
                view.page.changeLanguage(modelData.login, locale);
            }
            onLanguageClosed: {
                view.choosingLanguage = "";
                view.page.forgetRefusal();
            }
        }
    }

    // The outcome sits between the list and the form, near both.
    ChildrenOutcome {
        id: outcomeBox

        width: view.width
        page: view.page
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
                    if (view.changingName && row.renaming)
                        row.focusNewName();
                    else if (view.changingName)
                        row.focusChangeName();
                    else if (view.changingLanguage && row.choosingLanguage)
                        row.focusLanguages();
                    else if (view.changingLanguage)
                        row.focusChangeLanguage();
                    else
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
            // A name in place closes its field.
            if (view.page.step === ChildrenPage.NameChanged)
                view.renaming = "";
            if (view.page.step === ChildrenPage.LanguageChanged)
                view.choosingLanguage = "";
            // Later, once the controls are enabled again.
            if (view.page.step === ChildrenPage.Asking)
                Qt.callLater(view.outcomeShown);
            else
                Qt.callLater(view.restoreFocus);
        }
    }
}
