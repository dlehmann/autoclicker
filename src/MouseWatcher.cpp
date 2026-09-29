#include "MouseWatcher.h"

#include "KWinScript.h"

#include <QDBusConnection>

namespace {

const QString ObjectPath = QStringLiteral("/autoclicker");

// Distance in pixels from which a movement counts as user input.
constexpr int Tolerance = 3;

const char *ScriptTemplate = R"JS(
var svc = "%1", path = "%2", iface = "local.autoclicker.Watcher";
var tx = %3, ty = %4, tol = %5;
var first = true, reported = false;
function fmt(p) { return String(Math.round(p.x)) + "," + String(Math.round(p.y)); }
workspace.cursorPosChanged.connect(function () {
    var p = workspace.cursorPos;
    if (first) {
        first = false;
        callDBus(svc, path, iface, "scriptHomed", fmt(p));
    }
    if (!reported && (Math.abs(p.x - tx) > tol || Math.abs(p.y - ty) > tol)) {
        reported = true;
        callDBus(svc, path, iface, "scriptUserMoved", fmt(p));
    }
});
callDBus(svc, path, iface, "scriptReady", fmt(workspace.cursorPos));
)JS";

QPoint parsePoint(const QString &s)
{
    const auto parts = s.split(QLatin1Char(','));
    return parts.size() == 2 ? QPoint(parts[0].toInt(), parts[1].toInt()) : QPoint();
}

} // namespace

MouseWatcher::MouseWatcher(QObject *parent)
    : QObject(parent)
    , m_script(std::make_unique<KWinScript>(QStringLiteral("autoclicker-watcher")))
{
}

MouseWatcher::~MouseWatcher()
{
    stop();
}

bool MouseWatcher::start(QPoint target)
{
    stop();
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_error = tr("No connection to D-Bus.");
        return false;
    }
    if (!m_registered) {
        m_registered = bus.registerObject(ObjectPath, this, QDBusConnection::ExportScriptableSlots);
        if (!m_registered) {
            m_error = tr("Could not register the D-Bus object.");
            return false;
        }
    }

    const QString source = QString::fromUtf8(ScriptTemplate)
                               .arg(bus.baseService(), ObjectPath)
                               .arg(target.x())
                               .arg(target.y())
                               .arg(Tolerance);
    if (!m_script->load(source)) {
        m_error = m_script->errorString();
        return false;
    }
    return true;
}

void MouseWatcher::stop()
{
    m_script->unload();
}

void MouseWatcher::scriptReady(const QString &pos)
{
    if (m_script->isLoaded())
        Q_EMIT ready(parsePoint(pos));
}

void MouseWatcher::scriptHomed(const QString &pos)
{
    if (m_script->isLoaded())
        Q_EMIT homed(parsePoint(pos));
}

void MouseWatcher::scriptUserMoved(const QString &pos)
{
    if (m_script->isLoaded())
        Q_EMIT userActivity(parsePoint(pos));
}
