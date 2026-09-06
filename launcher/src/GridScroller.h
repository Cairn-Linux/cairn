// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>

// Decides which rows of the tile grid are on screen (ADR-0015). The grid is
// flat; when there are more rows than fit, the window slides by whole rows
// so the focused tile is never off screen, and never further than that. A
// wheel notch moves the focus one row, so wheel and keys scroll the same way.
class GridScroller : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
    // Whole rows the window shows at once.
    Q_PROPERTY(int visibleRows READ visibleRows WRITE setVisibleRows NOTIFY visibleRowsChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    // The decision: the row at the top of the window.
    Q_PROPERTY(int firstRow READ firstRow NOTIFY firstRowChanged)
    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged)
    // More rows than the window shows, so the grid scrolls and the next row peeks.
    Q_PROPERTY(bool scrolls READ scrolls NOTIFY rowsChanged)

public:
    explicit GridScroller(QObject* parent = nullptr);

    int columns() const;
    void setColumns(int columns);
    int visibleRows() const;
    void setVisibleRows(int rows);
    int count() const;
    void setCount(int count);
    int currentIndex() const;
    void setCurrentIndex(int index);
    int firstRow() const;
    int rows() const;
    bool scrolls() const;

    // The index the focus moves to for one wheel event: one row down for a
    // negative angle, one row up for a positive one, staying put at the ends.
    Q_INVOKABLE int indexAfterWheel(int angleDeltaY) const;

signals:
    void columnsChanged();
    void visibleRowsChanged();
    void countChanged();
    void currentIndexChanged();
    void firstRowChanged();
    void rowsChanged();

private:
    void follow();
    int rowOf(int index) const;

    int m_columns = 3;
    int m_visibleRows = 2;
    int m_count = 0;
    int m_currentIndex = 0;
    int m_firstRow = 0;
};
