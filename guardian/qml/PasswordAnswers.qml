// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// Under the password form: the sentence about the last password change,
// and under it the offer to change KDE Wallet's password too (ADR-0043).
// When the offer comes, the window shows both, and the sentence's top wins
// when they do not fit; after a button, what the card says wins. With
// nothing to say it has no height, and the page's column leaves no gap
// for an item with none.
Column {
    id: answers

    required property AccountPage page
    readonly property bool aboutPassword: answers.page.step === AccountPage.Asking || answers.page.step === AccountPage.PasswordChanged || answers.page.step === AccountPage.PasswordNotChanged
    readonly property Item outcomeBox: passwordOutcomeBox

    // The Guardian answered the wallet's offer, with either button.
    signal walletAnswered
    // The card came, or says something new: what the window scrolls to.
    signal walletShown(Item box)

    spacing: Tokens.bodySize

    function outcome() {
        switch (answers.page.step) {
        case AccountPage.Asking:
            return qsTr("Type the password you use now in the box that opened.");
        case AccountPage.PasswordChanged:
            return qsTr("Your password is changed. Use the new one from now on: to log in, to unlock the screen and in every password box.");
        case AccountPage.PasswordNotChanged:
            return answers.reason();
        }
        return "";
    }

    function reason() {
        switch (answers.page.reason) {
        case AccountPage.Empty:
            return qsTr("Type the new password first.");
        case AccountPage.Different:
            return qsTr("The two new passwords are not the same. Type the new password again in the second box.");
        case AccountPage.TooLong:
            return qsTr("A password can be at most %1 characters long. Choose a shorter one.").arg(answers.page.maxPasswordLength);
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

    OutcomeBox {
        id: passwordOutcomeBox

        objectName: "passwordOutcome"
        width: answers.width
        text: answers.aboutPassword ? answers.outcome() : ""
    }

    WalletOffer {
        id: walletOffer

        objectName: "walletOffer"
        width: answers.width
        wallet: answers.page.wallet
        onAnswered: answers.walletAnswered()
        onShown: answers.walletShown(walletOffer.step === WalletPassword.Offered ? answers : walletOffer)
    }
}
