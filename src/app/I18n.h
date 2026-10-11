// I18n.h - English and French texts, chosen from the system language as the
// Python version did, or from the Interface settings.
//
// C++ calls T("key"); the QML reads I18n.s.key, a map that is replaced when
// the language changes so every binding follows.  The catalogue is in
// I18n.cpp.  Placeholders are %1, %2... (QString::arg).
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class I18n : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lang READ lang NOTIFY changed)
    Q_PROPERTY(QVariantMap s READ strings NOTIFY changed)
public:
    static I18n &instance();

    // "auto" (system language), "en" or "fr".
    void setPreference(const QString &preference);
    QString lang() const { return m_lang; }
    QString t(const char *key) const;
    QVariantMap strings() const { return m_strings; }

    // "fr" when the system language is French, "en" otherwise.
    static QString systemLanguage();
    // Every key of the catalogue, for the tests.
    static QStringList keys();
    static bool hasKey(const QString &key);

signals:
    void changed();

private:
    I18n();
    void rebuild();
    QString m_lang;
    QVariantMap m_strings;
};

inline QString T(const char *key)
{
    return I18n::instance().t(key);
}
