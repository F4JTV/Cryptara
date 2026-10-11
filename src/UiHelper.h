// UiHelper.h - what the QML needs from Qt Gui and the core does not link.
#pragma once

#include <QImage>
#include <QObject>
#include <QQuickWindow>

class UiHelper : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // Saves the whole window (menu bar and status bar included) as a PNG.
    Q_INVOKABLE bool grabWindow(QObject *window, const QString &path) const
    {
        auto *quickWindow = qobject_cast<QQuickWindow *>(window);
        return quickWindow && quickWindow->grabWindow().save(path);
    }
};
