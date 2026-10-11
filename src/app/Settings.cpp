// Settings.cpp - configparser-compatible configuration file.
#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>

namespace {

const QString kSection = QStringLiteral("CRYPTARA");

// configparser's BasicInterpolation: "%%" stands for "%".  Python refuses
// to store a lone "%", so it is doubled on writing and undoubled on reading.
QString unescapePercent(const QString &value)
{
    QString out = value;
    out.replace(QStringLiteral("%%"), QStringLiteral("%"));
    return out;
}

QString escapePercent(const QString &value)
{
    QString out = value;
    out.replace(QStringLiteral("%"), QStringLiteral("%%"));
    return out;
}

} // namespace

const QList<Settings::Key> &Settings::schema()
{
    // Same keys, order and defaults as load_settings() in the Python version,
    // then the two this version adds.
    static const QList<Key> keys = {
        {"modem_type", Type::String, "VARA HF"},
        {"host", Type::String, "localhost"},
        {"cmd_port", Type::Int, "8300"},
        {"data_port", Type::Int, "8301"},
        {"mycall", Type::String, ""},
        {"operator_name", Type::String, ""},
        {"operator_firstname", Type::String, ""},
        {"qth", Type::String, ""},
        {"grid_square", Type::String, ""},
        {"rig", Type::String, ""},
        {"antenna", Type::String, ""},
        {"listen", Type::Bool, "True"},
        {"compression", Type::Bool, "True"},
        {"vara_hf_path", Type::String, ""},
        {"vara_fm_path", Type::String, ""},
        {"vara_sat_path", Type::String, ""},
        {"auto_start_vara", Type::Bool, "True"},
        {"auto_enabled", Type::Bool, "False"},
        {"auto_message", Type::String, ""},
        {"auto_disconnect", Type::Bool, "True"},
        {"auto_delay", Type::Int, "3"},
        {"timestamp", Type::Bool, "True"},
        {"sound", Type::Bool, "False"},
        {"save_log", Type::Bool, "True"},
        {"tooltips_enabled", Type::Bool, "True"},
        {"file_rx_path", Type::String, ""},
        {"bbs_enabled", Type::Bool, "False"},
        {"bbs_folder", Type::String, ""},
        {"macro_f1", Type::String, ""},
        {"macro_f2", Type::String, ""},
        {"macro_f3", Type::String, ""},
        {"macro_f4", Type::String, ""},
        {"macro_f5", Type::String, ""},
        {"macro_f6", Type::String, ""},
        {"macro_f7", Type::String, ""},
        {"macro_f8", Type::String, ""},
        {"macro_f9", Type::String, ""},
        {"macro_f10", Type::String, ""},
        {"macro_f11", Type::String, ""},
        {"macro_f12", Type::String, ""},
        // New in the C++ version; the Python version ignores them.
        {"theme", Type::String, "light"},
        {"language", Type::String, "auto"},
    };
    return keys;
}

Settings::Settings()
{
    for (const Key &key : schema())
        m_values.insert(QString::fromLatin1(key.name),
                        convert(key.type, QString::fromLatin1(key.defaultValue),
                                QString::fromLatin1(key.defaultValue)));
}

QString Settings::configDir()
{
#ifdef Q_OS_WIN
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty())
        base = QDir::homePath();
#else
    const QString base = QDir::homePath() + QStringLiteral("/.config");
#endif
    const QString path = QDir(base).filePath(QStringLiteral("CRYPTARA"));
    QDir().mkpath(path);
    return path;
}

QString Settings::defaultPath()
{
    return QDir(configDir()).filePath(QStringLiteral("cryptara_settings.conf"));
}

QVariant Settings::convert(Type type, const QString &text, const QString &fallback)
{
    switch (type) {
    case Type::Int: {
        bool ok = false;
        const int value = text.trimmed().toInt(&ok);
        return ok ? value : fallback.toInt();
    }
    case Type::Bool:
        // Python: value.lower() == 'true'
        return text.trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    case Type::String:
        break;
    }
    return text;
}

QMap<QString, QString> Settings::parseIni(const QString &text, const QString &section)
{
    // A subset of configparser: sections, "key = value" or "key: value",
    // keys lower-cased, full-line comments (# or ;), indented continuation
    // lines and blank lines inside values, trailing blank lines dropped.
    QMap<QString, QStringList> lines;
    QString current;
    QString key;
    bool inSection = false;
    static const QRegularExpression sectionRe(QStringLiteral("^\\[(?<name>.+)\\]"));

    const QStringList rows = text.split(QLatin1Char('\n'));
    for (QString row : rows) {
        if (row.endsWith(QLatin1Char('\r')))
            row.chop(1);
        const QString stripped = row.trimmed();
        if (stripped.startsWith(QLatin1Char('#')) || stripped.startsWith(QLatin1Char(';')))
            continue;
        if (stripped.isEmpty()) {
            if (inSection && !key.isEmpty())
                lines[key].append(QString());
            continue;
        }
        const bool indented = row.at(0).isSpace();
        if (indented && inSection && !key.isEmpty()) {
            lines[key].append(stripped);
            continue;
        }
        const auto match = sectionRe.match(stripped);
        if (match.hasMatch()) {
            current = match.captured(QStringLiteral("name"));
            inSection = current == section;
            key.clear();
            continue;
        }
        if (!inSection)
            continue;
        const int eq = row.indexOf(QLatin1Char('='));
        const int colon = row.indexOf(QLatin1Char(':'));
        int split = -1;
        if (eq >= 0 && colon >= 0)
            split = qMin(eq, colon);
        else
            split = qMax(eq, colon);
        if (split <= 0) {
            key.clear();
            continue;
        }
        key = row.left(split).trimmed().toLower();
        lines[key] = QStringList{row.mid(split + 1).trimmed()};
    }

    QMap<QString, QString> values;
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        QString value = it.value().join(QLatin1Char('\n'));
        // configparser: '\n'.join(lines).rstrip()
        while (!value.isEmpty() && value.at(value.size() - 1).isSpace())
            value.chop(1);
        values.insert(it.key(), unescapePercent(value));
    }
    return values;
}

QString Settings::writeIni(const QString &section, const QList<QPair<QString, QString>> &items)
{
    // As configparser writes it: "key = value", continuation lines
    // indented with a tab, a blank line after the section.
    QString out = QStringLiteral("[%1]\n").arg(section);
    for (const auto &item : items) {
        QString value = escapePercent(item.second);
        value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        value.replace(QLatin1Char('\n'), QStringLiteral("\n\t"));
        out += item.first + QStringLiteral(" = ") + value + QLatin1Char('\n');
    }
    out += QLatin1Char('\n');
    return out;
}

bool Settings::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QByteArray data = file.readAll();
    if (data.startsWith("\xef\xbb\xbf"))
        data.remove(0, 3);
    const QMap<QString, QString> values = parseIni(QString::fromUtf8(data), kSection);
    for (const Key &key : schema()) {
        const QString name = QString::fromLatin1(key.name);
        if (values.contains(name))
            m_values.insert(name, convert(key.type, values.value(name),
                                          QString::fromLatin1(key.defaultValue)));
    }
    return true;
}

bool Settings::save(const QString &path, QString *error) const
{
    QList<QPair<QString, QString>> items;
    for (const Key &key : schema()) {
        const QString name = QString::fromLatin1(key.name);
        const QVariant value = m_values.value(name);
        QString text;
        switch (key.type) {
        case Type::Bool:
            text = value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
            break;
        case Type::Int:
            text = QString::number(value.toInt());
            break;
        case Type::String:
            text = value.toString();
            break;
        }
        items.append({name, text});
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(writeIni(kSection, items).toUtf8());
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString Settings::string(const QString &key) const
{
    return m_values.value(key).toString();
}

int Settings::integer(const QString &key) const
{
    return m_values.value(key).toInt();
}

bool Settings::flag(const QString &key) const
{
    return m_values.value(key).toBool();
}

void Settings::set(const QString &key, const QVariant &value)
{
    for (const Key &k : schema()) {
        if (key == QLatin1String(k.name)) {
            switch (k.type) {
            case Type::Int:
                m_values.insert(key, value.toInt());
                return;
            case Type::Bool:
                m_values.insert(key, value.toBool());
                return;
            case Type::String:
                m_values.insert(key, value.toString());
                return;
            }
        }
    }
}

void Settings::merge(const QVariantMap &values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        set(it.key(), it.value());
}
