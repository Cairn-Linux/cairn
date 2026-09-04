// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand

TestCase {
    name: "BrandTokens"

    function init() {
        Tokens.textScale = 1.0;
    }

    function cleanup() {
        Tokens.textScale = 1.0;
    }

    function test_labelColours() {
        // Palette properties carry the exact values generated from tokens.json.
        compare(Tokens.makeLabel, Tokens.ink);
        compare(Tokens.practiceLabel, Tokens.ink);
        compare(Tokens.machineLabel, Tokens.sand);
        compare(Tokens.inkLabel, Tokens.sand);
        compare(Tokens.sandLabel, Tokens.ink);
        compare(Tokens.paperLabel, Tokens.ink);
    }

    function test_textScale() {
        const bodyAtNormalScale = Tokens.bodySize;
        const displaySpacingAtNormalScale = Tokens.displayLetterSpacing;
        const kickerSpacingAtNormalScale = Tokens.kickerLetterSpacing;

        Tokens.textScale = 1.5;

        compare(Tokens.bodySize, bodyAtNormalScale * 1.5);
        compare(Tokens.displayLetterSpacing, displaySpacingAtNormalScale * 1.5);
        compare(Tokens.kickerLetterSpacing, kickerSpacingAtNormalScale * 1.5);
    }

    function test_focusDimensions() {
        verify(Tokens.focusWidth > 0);
        verify(Tokens.focusOffset > 0);
    }
}
