// SPDX-License-Identifier: Apache-2.0
#include "GridScroller.h"

#include <algorithm>

GridScroller::GridScroller(QObject* parent) : QObject(parent) {}

int GridScroller::columns() const {
    return m_columns;
}

void GridScroller::setColumns(int columns) {
    columns = std::max(1, columns);
    if (columns == m_columns) {
        return;
    }
    m_columns = columns;
    emit columnsChanged();
    emit rowsChanged();
    follow();
}

int GridScroller::visibleRows() const {
    return m_visibleRows;
}

void GridScroller::setVisibleRows(int rows) {
    rows = std::max(1, rows);
    if (rows == m_visibleRows) {
        return;
    }
    m_visibleRows = rows;
    emit visibleRowsChanged();
    emit rowsChanged();
    follow();
}

int GridScroller::count() const {
    return m_count;
}

void GridScroller::setCount(int count) {
    count = std::max(0, count);
    if (count == m_count) {
        return;
    }
    m_count = count;
    emit countChanged();
    emit rowsChanged();
    follow();
}

int GridScroller::currentIndex() const {
    return m_currentIndex;
}

void GridScroller::setCurrentIndex(int index) {
    if (index == m_currentIndex) {
        return;
    }
    m_currentIndex = index;
    emit currentIndexChanged();
    follow();
}

int GridScroller::firstRow() const {
    return m_firstRow;
}

int GridScroller::rows() const {
    return (m_count + m_columns - 1) / m_columns;
}

bool GridScroller::scrolls() const {
    return rows() > m_visibleRows;
}

int GridScroller::indexAfterWheel(int angleDeltaY) const {
    if (angleDeltaY == 0 || m_count == 0) {
        return m_currentIndex;
    }
    const int next = angleDeltaY < 0 ? std::min(m_currentIndex + m_columns, m_count - 1)
                                     : m_currentIndex - m_columns;
    if (next < 0 || rowOf(next) == rowOf(m_currentIndex)) {
        return m_currentIndex;
    }
    return next;
}

void GridScroller::follow() {
    // Slide only as far as it takes to bring the focused row into the window.
    const int row = rowOf(std::clamp(m_currentIndex, 0, std::max(0, m_count - 1)));
    int first = m_firstRow;
    if (row < first) {
        first = row;
    } else if (row >= first + m_visibleRows) {
        first = row - m_visibleRows + 1;
    }
    first = std::clamp(first, 0, std::max(0, rows() - m_visibleRows));
    if (first == m_firstRow) {
        return;
    }
    m_firstRow = first;
    emit firstRowChanged();
}

int GridScroller::rowOf(int index) const {
    return index / m_columns;
}
