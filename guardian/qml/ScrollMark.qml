// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A thin bar beside a part that scrolls, when what it holds is taller than
// it: the bar is as long as the share in sight, and as far down as the part
// is scrolled. It only shows the place; the wheel and a drag scroll the
// part itself. It shares the part's parent, and whoever uses it sets its x.
Rectangle {
    id: mark

    required property Flickable flickable

    visible: mark.flickable.visibleArea.heightRatio < 1
    y: mark.flickable.y + mark.flickable.visibleArea.yPosition * mark.flickable.height
    width: Tokens.focusWidth * 2
    height: mark.flickable.visibleArea.heightRatio * mark.flickable.height
    radius: Tokens.radiusPill
    color: Tokens.machine
    Accessible.ignored: true
}
