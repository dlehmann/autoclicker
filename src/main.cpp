#include "MainWindow.h"
#include "MouseWatcher.h"
#include "VirtualPointer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QScreen>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QTranslator>

namespace {

// Time KWin needs to pick up a freshly created uinput device
constexpr int DeviceSettleMs = 1500;

QRect desktopGeometry()
{
    QRect desktop;
    for (QScreen *s : QGuiApplication::screens())
        desktop = desktop.united(s->geometry());
    return desktop;
}

// Installs the translation matching the system's preferred UI languages.
// English is the source language and the fallback: if English comes before
// any available translation, no translator is installed.
void installTranslations(QApplication &app)
{
    for (QString lang : QLocale().uiLanguages()) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (lang.startsWith(QLatin1String("en")))
            return;

        auto *appTranslator = new QTranslator(&app);
        if (!appTranslator->load(QStringLiteral("autoclicker_") + lang, QStringLiteral(":/i18n"))) {
            delete appTranslator;
            continue;
        }
        QApplication::installTranslator(appTranslator);

        // Qt's own texts (OK button, context menus, …) in the same language
        auto *qtTranslator = new QTranslator(&app);
        if (qtTranslator->load(QStringLiteral("qtbase_") + lang,
                               QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            QApplication::installTranslator(qtTranslator);
        else
            delete qtTranslator;
        return;
    }
}

// Console test: clicks once at the given position.
int testClick(const QStringList &args)
{
    QTextStream out(stdout);
    if (args.size() < 4) {
        out << "Usage: autoclicker --test-click X Y\n";
        return 2;
    }
    VirtualPointer p;
    if (!p.isValid()) {
        out << p.errorString() << "\n";
        return 1;
    }
    QThread::msleep(DeviceSettleMs); // KWin needs a moment to pick up the new device
    const QRect desktop = desktopGeometry();
    const QPoint target(args[2].toInt(), args[3].toInt());
    out << "Desktop " << desktop.width() << "x" << desktop.height() << ", clicking at "
        << target.x() << "," << target.y() << "\n";
    out.flush();
    p.moveTo(target, desktop);
    p.click(std::chrono::milliseconds(50));
    QThread::msleep(200);
    return 0;
}

// Console test without clicking: moves the pointer 10 pixels to the right and
// checks via KWin that it lands there. This verifies the coordinate mapping.
// The watcher is only started once the device is ready, right before the move,
// so that mouse movement while waiting does not interfere.
int testMove()
{
    QTextStream out(stdout);
    auto *pointer = new VirtualPointer;
    if (!pointer->isValid()) {
        out << pointer->errorString() << "\n";
        return 1;
    }
    const QRect desktop = desktopGeometry();

    auto *watcher = new MouseWatcher;
    QObject::connect(watcher, &MouseWatcher::ready, [=](QPoint cursor) {
        const QPoint target = cursor + QPoint(10, 0);
        QTextStream(stdout) << "Pointer is at " << cursor.x() << "," << cursor.y()
                            << ", moving it to " << target.x() << "," << target.y() << "\n";
        QObject::connect(watcher, &MouseWatcher::homed, [=](QPoint landed) {
            const int err = (landed - target).manhattanLength();
            QTextStream(stdout) << "Landed at " << landed.x() << "," << landed.y()
                                << (err <= 4 ? "  -> OK\n" : "  -> WRONG (mouse moved?)\n");
            QCoreApplication::exit(err <= 4 ? 0 : 1);
        });
        pointer->moveTo(target, desktop);
    });
    // KWin needs a moment to pick up the new device
    QTimer::singleShot(DeviceSettleMs, [=] {
        if (!watcher->start({-1000000, -1000000})) {
            QTextStream(stdout) << watcher->errorString() << "\n";
            QCoreApplication::exit(1);
        }
    });
    QTimer::singleShot(DeviceSettleMs + 3000, [] {
        QTextStream(stdout) << "No response from KWin\n";
        QCoreApplication::exit(1);
    });
    const int rc = QCoreApplication::exec();
    delete watcher;
    delete pointer;
    return rc;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("autoclicker"));
    QApplication::setApplicationName(QStringLiteral("autoclicker"));
    installTranslations(app);
    QApplication::setApplicationDisplayName(MainWindow::tr("Autoclicker"));

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--test-click")))
        return testClick(args);
    if (args.contains(QStringLiteral("--test-move")))
        return testMove();

    MainWindow w;
    w.show();

    // Developer aid: save the window as an image and exit (optionally with target X Y)
    if (const int i = args.indexOf(QStringLiteral("--screenshot")); i > 0 && i + 1 < args.size()) {
        if (i + 3 < args.size())
            w.setTarget(QPoint(args[i + 2].toInt(), args[i + 3].toInt()));
        QApplication::processEvents();
        return w.grab().save(args[i + 1]) ? 0 : 1;
    }
    return app.exec();
}
