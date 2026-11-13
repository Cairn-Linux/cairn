// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A heading on the About page, marked as a heading for a screen reader.
Text {
    wrapMode: Text.Wrap
    textFormat: Text.PlainText
    font.family: Tokens.fontFamily
    font.pixelSize: Tokens.headingSize
    font.weight: Tokens.weightBold
    lineHeight: Tokens.headingLineHeight
    color: Tokens.text
    Accessible.role: Accessible.Heading
    Accessible.name: text
}
