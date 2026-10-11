// Settings.h - the configuration, in the very file the Python version uses.
//
// %APPDATA%\CRYPTARA\cryptara_settings.conf, one [CRYPTARA] section in
// Python configparser syntax.  An existing configuration is therefore taken
// over as it is, and the Python version can still read what this one
// writes.  QSettings is not used: its INI reader would split values at
// commas and eat the backslashes of Windows paths.
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

class Settings
{
public:
    enum class Type { String, Int, Bool };
    struct Key
    {
        const char *name;
        Type type;
        const char *defaultValue;
    };

    // Every key, in the order the file is written.
    static const QList<Key> &schema();

    Settings();

    // Default location: %APPDATA%\CRYPTARA (Windows), ~/.config/CRYPTARA.
    static QString configDir();
    static QString defaultPath();

    // Reads path; missing file or keys keep their defaults.  False with
    // *error if the file exists but cannot be read.
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool load() { return load(defaultPath()); }
    bool save(QString *error = nullptr) const { return save(defaultPath(), error); }

    QString string(const QString &key) const;
    int integer(const QString &key) const;
    bool flag(const QString &key) const;
    QVariant value(const QString &key) const { return m_values.value(key); }
    void set(const QString &key, const QVariant &value);

    // Typed values for QML, and back.  Unknown keys are ignored.
    QVariantMap toMap() const { return m_values; }
    void merge(const QVariantMap &values);

    // configparser's syntax, exposed for the tests.
    static QMap<QString, QString> parseIni(const QString &text, const QString &section);
    static QString writeIni(const QString &section, const QList<QPair<QString, QString>> &items);

private:
    static QVariant convert(Type type, const QString &text, const QString &fallback);
    QVariantMap m_values;
};
