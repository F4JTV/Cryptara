// Models.cpp
#include "Models.h"

#include <algorithm>

// ----------------------------------------------------------------- LineModel

LineModel::LineModel(int maxLines, QObject *parent)
    : QAbstractListModel(parent), m_maxLines(maxLines)
{
}

int LineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_lines.size());
}

QVariant LineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return QVariant();
    const Line &line = m_lines.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return line.text;
    case KindRole:
        return line.kind;
    }
    return QVariant();
}

QHash<int, QByteArray> LineModel::roleNames() const
{
    return {{TextRole, "text"}, {KindRole, "kind"}};
}

void LineModel::append(const QString &text, const QString &kind)
{
    // A long session must not grow without bound: the oldest lines go.
    if (m_maxLines > 0 && m_lines.size() >= m_maxLines) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_lines.removeFirst();
        endRemoveRows();
    }
    const int row = int(m_lines.size());
    beginInsertRows(QModelIndex(), row, row);
    m_lines.append({text, kind});
    endInsertRows();
    emit countChanged();
}

void LineModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
}

QString LineModel::plainText() const
{
    QStringList lines;
    lines.reserve(m_lines.size());
    for (const Line &line : m_lines)
        lines.append(line.text);
    return lines.join(QLatin1Char('\n'));
}

// ------------------------------------------------------------------- CqModel

CqModel::CqModel(QObject *parent) : QAbstractListModel(parent) {}

int CqModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant CqModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return QVariant();
    const CqEntry &e = m_entries.at(index.row());
    switch (role) {
    case CallsignRole:
        return e.callsign;
    case BandwidthRole:
        return e.bandwidth.isEmpty() ? QStringLiteral("-") : e.bandwidth;
    case ViaRole: {
        QStringList via;
        if (!e.via1.isEmpty())
            via << e.via1;
        if (!e.via2.isEmpty())
            via << e.via2;
        return via.isEmpty() ? QStringLiteral("-") : via.join(QLatin1Char(' '));
    }
    case TimeRole:
        return e.time.toString(QStringLiteral("HH:mm:ss"));
    }
    return QVariant();
}

QHash<int, QByteArray> CqModel::roleNames() const
{
    return {{CallsignRole, "callsign"}, {BandwidthRole, "bandwidth"},
            {ViaRole, "via"}, {TimeRole, "time"}};
}

void CqModel::sort()
{
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const CqEntry &a, const CqEntry &b) { return a.time > b.time; });
}

bool CqModel::addOrUpdate(const CqEntry &entry)
{
    beginResetModel();
    bool added = true;
    for (CqEntry &e : m_entries) {
        if (e.callsign == entry.callsign) {
            e = entry;
            added = false;
            break;
        }
    }
    if (added) {
        m_entries.append(entry);
        sort();
        // At most 20: the oldest goes.
        while (m_entries.size() > 20)
            m_entries.removeLast();
    } else {
        sort();
    }
    endResetModel();
    emit countChanged();
    return added;
}

void CqModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

const CqEntry *CqModel::at(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries.at(row);
}

// ---------------------------------------------------------------- TableModel

TableModel::TableModel(const QStringList &roles, QObject *parent)
    : QAbstractListModel(parent), m_roles(roles)
{
}

int TableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return QVariant();
    const int column = role - (Qt::UserRole + 1);
    const QStringList &row = m_rows.at(index.row());
    if (column < 0 || column >= row.size())
        return QVariant();
    return row.at(column);
}

QHash<int, QByteArray> TableModel::roleNames() const
{
    QHash<int, QByteArray> names;
    for (int i = 0; i < m_roles.size(); ++i)
        names.insert(Qt::UserRole + 1 + i, m_roles.at(i).toUtf8());
    return names;
}

void TableModel::setRows(const QVector<QStringList> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}

QString TableModel::cell(int row, int column) const
{
    if (row < 0 || row >= m_rows.size() || column < 0 || column >= m_rows.at(row).size())
        return QString();
    return m_rows.at(row).at(column);
}
