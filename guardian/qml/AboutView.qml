// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The About page (ADR-0040): what Cairn Linux is, which version this
// computer runs when its image says, what it is built on, with Fedora's
// notice in full, and where the licences are. Every block is plain text.
// The version, Fedora's release, Fedora's address and the folder's path
// are put into their sentences from the page object, so nothing read from
// a file is ever markup and no translation can change the notice's address
// or the folder's path.
//
// The keyboard reads it too. Tab stops on each block of words and each
// button in turn, and the window scrolls to show it; Up, Down, Page Up
// and Page Down scroll the page from any of them.
Column {
    id: view

    required property AboutPage page
    // The page's scroller, for the four keys that scroll.
    required property Flickable flickable

    // Asks the window to scroll just enough that the item shows whole.
    signal showRequested(Item item)

    spacing: Tokens.bodySize

    // One of the four keys scrolls the page. Any other is left alone.
    function scrollBy(event) {
        event.accepted = scrollKeys.scroll(event.key);
    }

    // The words under a button after a press that the desktop answered
    // opened nothing. They are laid out before the window is asked to
    // scroll to them.
    component NotOpened: Text {
        id: words

        width: view.width
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
        onVisibleChanged: {
            if (!words.visible)
                return;
            view.forceLayout();
            view.showRequested(words);
        }
    }

    ScrollKeys {
        id: scrollKeys

        flickable: view.flickable
    }

    AboutHeading {
        objectName: "aboutHeading"
        width: view.width
        text: qsTr("About Cairn Linux")
    }

    AboutWords {
        objectName: "aboutWhatItIs"
        width: view.width
        text: qsTr("Cairn Linux is the system on this computer. It is made for children, with a grown-up in charge.")
        onKeyPressed: event => view.scrollBy(event)
    }

    // Cairn's own version, never Fedora's number.
    AboutWords {
        objectName: "aboutVersion"
        width: view.width
        text: view.page.version === "" ? qsTr("This copy of Cairn Linux does not say which version it is.") : qsTr("Version: %1").arg(view.page.version)
        onKeyPressed: event => view.scrollBy(event)
    }

    AboutHeading {
        objectName: "builtOnHeading"
        width: view.width
        text: qsTr("What it is built on")
    }

    AboutWords {
        objectName: "aboutLineage"
        width: view.width
        //: Says what Cairn Linux is built on, and that it is not theirs. Keep its meaning, every name in it and the ® after the first "Fedora", which Fedora's trademark guidelines ask for.
        text: qsTr("Cairn Linux is built on Bazzite, a Universal Blue image derived from Fedora® Linux. Cairn Linux is not affiliated with, endorsed by, or supported by Bazzite, Universal Blue, Red Hat or the Fedora Project.")
        onKeyPressed: event => view.scrollBy(event)
    }

    AboutWords {
        objectName: "aboutNotice"
        width: view.width
        //: Fedora's trademark guidelines ask for this notice. Keep its meaning and every name in it. %1 is the address of Fedora's website.
        text: qsTr("The software Cairn Linux provides is not provided or supported by the Fedora Project. Official Fedora Materials are available through the Fedora Project website at %1.").arg(view.page.fedoraAddress)
        onKeyPressed: event => view.scrollBy(event)
    }

    // The address is written out above, so the notice is whole without
    // this button. Nothing is looked up or fetched until it is pressed.
    TextButton {
        objectName: "openFedora"
        quiet: true
        //: Keep fedoraproject.org as it is: it is the address the button opens.
        text: qsTr("Open fedoraproject.org in your web browser")
        Keys.onPressed: event => view.scrollBy(event)
        onActivated: view.page.open(AboutPage.FedoraWebsite)
    }

    NotOpened {
        objectName: "fedoraNotOpened"
        visible: view.page.notOpened === AboutPage.FedoraWebsite
        text: qsTr("No web browser opened. The address above works in any browser, on this computer or on a phone.")
    }

    AboutWords {
        objectName: "aboutFedoraRelease"
        width: view.width
        visible: view.page.fedoraRelease !== ""
        text: qsTr("This copy is derived from Fedora Linux %1.").arg(view.page.fedoraRelease)
        onKeyPressed: event => view.scrollBy(event)
    }

    AboutWords {
        objectName: "aboutAttribution"
        width: view.width
        //: Fedora's trademark guidelines ask for this statement. Keep its meaning and every name in it.
        text: qsTr("Fedora is a trademark or registered trademark of Red Hat, Inc., or its subsidiaries in the United States and other countries.")
        onKeyPressed: event => view.scrollBy(event)
    }

    AboutHeading {
        objectName: "licencesHeading"
        width: view.width
        text: qsTr("Licences")
    }

    AboutWords {
        objectName: "aboutLicences"
        width: view.width
        text: qsTr("Cairn Linux's own code is licensed under the Apache License 2.0. Its documentation and brand assets, including the stacked-stones artwork, are licensed under CC BY-SA 4.0. Those copyright licences do not grant trademark rights in the name \"Cairn Linux\" or the stacked-stones mark.")
        onKeyPressed: event => view.scrollBy(event)
    }

    AboutWords {
        objectName: "aboutFullTexts"
        width: view.width
        text: view.page.licenceFolder === "" ? qsTr("The full texts could not be found on this computer.") : qsTr("The full texts are on this computer, in the folder %1. LICENSE is the Apache License 2.0. LICENSE-CC-BY-SA-4.0 is the CC BY-SA 4.0 licence. NOTICE says whose work Cairn Linux is built on. TRADEMARKS.md says how the name and the mark may be used.").arg(view.page.licenceFolder)
        onKeyPressed: event => view.scrollBy(event)
    }

    TextButton {
        objectName: "openFolder"
        quiet: true
        visible: view.page.licenceFolder !== ""
        text: qsTr("Open the folder with the full texts")
        Keys.onPressed: event => view.scrollBy(event)
        onActivated: view.page.open(AboutPage.LicenceFolder)
    }

    NotOpened {
        objectName: "folderNotOpened"
        visible: view.page.notOpened === AboutPage.LicenceFolder
        text: qsTr("The folder did not open. Its path is written above.")
    }

    AboutWords {
        objectName: "aboutOtherSoftware"
        width: view.width
        //: Keep the path /usr/share/licenses as it is.
        text: qsTr("The rest of the system is software from other projects, each under its own licence. The licences of its packages are in the folder /usr/share/licenses.")
        onKeyPressed: event => view.scrollBy(event)
    }
}
