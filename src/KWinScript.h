#pragma once

#include <QCoreApplication>
#include <QString>

#include <memory>

class QTemporaryFile;

// Loads a JavaScript into KWin (via D-Bus, without special permissions) and
// removes it again. Only works on KDE Plasma.
class KWinScript
{
    Q_DECLARE_TR_FUNCTIONS(KWinScript)

public:
    explicit KWinScript(QString pluginName);
    ~KWinScript();

    KWinScript(const KWinScript &) = delete;
    KWinScript &operator=(const KWinScript &) = delete;

    bool load(const QString &source);
    void unload();
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_error; }

private:
    QString m_pluginName;
    std::unique_ptr<QTemporaryFile> m_file;
    bool m_loaded = false;
    QString m_error;
};
