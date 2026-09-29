#include "KWinScript.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QTemporaryFile>

namespace {

const QString KWinService = QStringLiteral("org.kde.KWin");

QDBusInterface scripting()
{
    return QDBusInterface(KWinService, QStringLiteral("/Scripting"),
                          QStringLiteral("org.kde.kwin.Scripting"));
}

} // namespace

KWinScript::KWinScript(QString pluginName)
    : m_pluginName(std::move(pluginName))
{
}

KWinScript::~KWinScript()
{
    unload();
}

bool KWinScript::load(const QString &source)
{
    unload();

    m_file = std::make_unique<QTemporaryFile>(QDir::tempPath() + QStringLiteral("/autoclicker-XXXXXX.js"));
    if (!m_file->open()) {
        m_error = tr("Could not create a temporary script file.");
        return false;
    }
    m_file->write(source.toUtf8());
    m_file->flush();

    auto iface = scripting();
    if (!iface.isValid()) {
        m_error = tr("KWin is not reachable. The autoclicker only works on KDE Plasma.");
        return false;
    }
    // Remove leftovers of a crashed run
    iface.call(QStringLiteral("unloadScript"), m_pluginName);

    QDBusReply<int> id = iface.call(QStringLiteral("loadScript"), m_file->fileName(), m_pluginName);
    if (!id.isValid() || id.value() < 0) {
        m_error = tr("Could not load the KWin script: %1").arg(id.error().message());
        return false;
    }
    m_loaded = true;

    QDBusInterface script(KWinService, QStringLiteral("/Scripting/Script%1").arg(id.value()),
                          QStringLiteral("org.kde.kwin.Script"));
    script.call(QStringLiteral("run"));
    return true;
}

void KWinScript::unload()
{
    if (m_loaded) {
        scripting().call(QDBus::NoBlock, QStringLiteral("unloadScript"), m_pluginName);
        m_loaded = false;
    }
    m_file.reset();
}
