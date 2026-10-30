// SPDX-License-Identifier: Apache-2.0
#include "PictureFile.h"

#include <QImage>
#include <QImageReader>
#include <QRect>

#include <algorithm>

bool PictureFile::prepare(const QString& source, const QString& target) {
    QImageReader reader(source);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return false;
    }
    const int shorter = std::min(image.width(), image.height());
    const QRect middle((image.width() - shorter) / 2, (image.height() - shorter) / 2, shorter,
                       shorter);
    const int finalSide = std::min(shorter, side);
    const QImage square = image.copy(middle).scaled(finalSide, finalSide, Qt::IgnoreAspectRatio,
                                                    Qt::SmoothTransformation);
    return square.save(target, "PNG");
}
