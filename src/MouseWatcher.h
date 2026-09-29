#pragma once

#include <QObject>
#include <QPoint>

#include <memory>

class KWinScript;

// Detects user mouse movement through a KWin script that reports back via
// D-Bus. Needs no special permissions, but requires KDE Plasma.
//
// Flow: the script announces itself with ready(). After that, every pointer
// position more than a few pixels away from the target is reported as user
// input – the simulated click always puts the pointer exactly on the target.
// homed() reports the first new pointer position (used for tests).
class MouseWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "local.autoclicker.Watcher")

public:
    explicit MouseWatcher(QObject *parent = nullptr);
    ~MouseWatcher() override;

    // Loads the script into KWin. The ready() signal follows asynchronously.
    bool start(QPoint target);
    void stop();
    QString errorString() const { return m_error; }

public Q_SLOTS:
    // Called by the KWin script via D-Bus; arguments as "x,y".
    Q_SCRIPTABLE void scriptReady(const QString &pos);
    Q_SCRIPTABLE void scriptHomed(const QString &pos);
    Q_SCRIPTABLE void scriptUserMoved(const QString &pos);

Q_SIGNALS:
    void ready(QPoint cursor);
    void homed(QPoint cursor);
    void userActivity(QPoint cursor);

private:
    std::unique_ptr<KWinScript> m_script;
    bool m_registered = false;
    QString m_error;
};
