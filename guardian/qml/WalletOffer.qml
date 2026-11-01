// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// Under the sentence about a new password: the offer to change KDE
// Wallet's password too (ADR-0043), then what kwalletd's own box asks, or
// that kwalletd did not answer. WalletPassword decides every step; this
// only picks the words for it.
Rectangle {
    id: offer

    required property WalletPassword wallet
    readonly property int step: offer.wallet.step
    readonly property bool offering: offer.step === WalletPassword.Offered || offer.step === WalletPassword.NoAnswer

    // The Guardian answered, with either button.
    signal answered
    // It came, or says something new, so the window may scroll to it.
    signal shown

    function sentence() {
        switch (offer.step) {
        case WalletPassword.WalletAsks:
            return qsTr("KDE Wallet asks in a box of its own. If it first asks for the wallet's own password, type that: usually your old one. Then type your new password twice.");
        case WalletPassword.NoAnswer:
            return qsTr("KDE Wallet did not answer, so its password stays as it was. Trying again is safe. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        }
        return qsTr("KDE Wallet keeps passwords that programs save, such as Wi-Fi ones, behind a password of its own. It opens by itself at login only if that password is the same as your login password. Change the wallet's password to your new one too? If not, the wallet asks for its own password, usually your old one, the next time a program needs it.");
    }

    // A hidden card is not told about: the window keeps one scroll waiting,
    // the latest, and a scroll to a hidden card would take the place of
    // the one to the sentence about the password. The buttons may have
    // just come, so they are laid out first, or the window would scroll to
    // a card shorter than it is.
    function tell() {
        if (!offer.visible)
            return;
        buttons.forceLayout();
        column.forceLayout();
        offer.shown();
    }

    visible: offer.offering || offer.step === WalletPassword.WalletAsks
    height: column.implicitHeight + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: words.text

    // After the step has changed and the card has its new size.
    Connections {
        target: offer.wallet

        function onStepChanged() {
            Qt.callLater(offer.tell);
        }
    }

    Column {
        id: column

        x: Tokens.bodySize
        y: Tokens.bodySize
        width: offer.width - Tokens.bodySize * 2
        spacing: Tokens.bodySize

        Text {
            id: words

            objectName: "walletSentence"
            width: column.width
            wrapMode: Text.WordWrap
            text: offer.sentence()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.text
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flow {
            id: buttons

            width: column.width
            spacing: Tokens.bodySize / 2
            visible: offer.offering

            TextButton {
                objectName: "changeWalletPassword"
                text: qsTr("Change the wallet's password")
                onActivated: {
                    offer.wallet.change();
                    offer.answered();
                }
            }

            // Not "Not now": nothing offers again until the next new
            // password.
            TextButton {
                objectName: "leaveWalletPassword"
                quiet: true
                text: qsTr("Leave it as it is")
                onActivated: {
                    offer.wallet.leaveAsItIs();
                    offer.answered();
                }
            }
        }
    }
}
