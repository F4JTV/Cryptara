// Models.h - list models behind the chat, the system log, the CQ table,
// the QSO log and the BBS file list.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QStringList>
#include <QVector>

// Lines of text with a kind (chat: tx, rx, info; log: debug, info,
// success, warning, error).  The QML colours them from the theme.
class LineModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { TextRole = Qt::UserRole + 1, KindRole };

    explicit LineModel(int maxLines, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const QString &text, const QString &kind);
    void clear();
    int count() const { return int(m_lines.size()); }
    // All lines, newline separated, as QTextEdit::toPlainText() gave them.
    Q_INVOKABLE QString plainText() const;

signals:
    void countChanged();

private:
    struct Line { QString text; QString kind; };
    QVector<Line> m_lines;
    int m_maxLines;
};

struct CqEntry
{
    QString callsign;
    QString bandwidth;
    QString via1;
    QString via2;
    QDateTime time;
};

// Received CQ frames, most recent first, at most 20.
class CqModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { CallsignRole = Qt::UserRole + 1, BandwidthRole, ViaRole, TimeRole };

    explicit CqModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds or refreshes the station; true if it was new.
    bool addOrUpdate(const CqEntry &entry);
    void clear();
    int count() const { return int(m_entries.size()); }
    const CqEntry *at(int row) const;

signals:
    void countChanged();

private:
    void sort();
    QVector<CqEntry> m_entries;
};

// A generic table of string columns, for the QSO log and BBS dialogs.
class TableModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit TableModel(const QStringList &roles, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QVector<QStringList> &rows);
    int count() const { return int(m_rows.size()); }
    Q_INVOKABLE QString cell(int row, int column) const;

signals:
    void countChanged();

private:
    QStringList m_roles;
    QVector<QStringList> m_rows;
};
