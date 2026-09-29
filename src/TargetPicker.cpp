#include "TargetPicker.h"

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

PickerOverlay::PickerOverlay(QScreen *screen, TargetPicker *picker)
    : m_screen(screen)
    , m_picker(picker)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    if (QGuiApplication::platformName() == QLatin1String("wayland")) {
        // As a layer surface above everything (including panels) on exactly this screen.
        winId();
        QWindow *win = windowHandle();
        win->setScreen(screen);
        if (auto *layer = LayerShellQt::Window::get(win)) {
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setAnchors(LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
            layer->setExclusiveZone(-1);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
            layer->setScope(QStringLiteral("autoclicker-picker"));
#ifdef LAYERSHELLQT_HAS_SETSCREEN
            layer->setScreen(screen);
#endif
        }
        resize(screen->geometry().size());
        show();
    } else {
        setGeometry(screen->geometry());
        showFullScreen();
    }
    activateWindow();
    setFocus();
}

void PickerOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // Source instead of SourceOver: fully replaces the old content so that no
    // remnants of a previous crosshair remain.
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), QColor(0, 0, 0, 90));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QString hint = tr("Click the spot that should be clicked\n"
                            "Esc = cancel");
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    p.setFont(f);
    const QRect box = p.fontMetrics()
                          .boundingRect(rect(), Qt::AlignCenter, hint)
                          .adjusted(-24, -16, 24, 16)
                          .translated(0, -height() / 4);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 210));
    p.drawRoundedRect(box, 10, 10);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, hint);

    if (rect().contains(m_cursor)) {
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(QColor(255, 80, 80, 200), 1, Qt::DashLine));
        p.drawLine(0, m_cursor.y(), width(), m_cursor.y());
        p.drawLine(m_cursor.x(), 0, m_cursor.x(), height());

        const QPoint g = m_screen->geometry().topLeft() + m_cursor;
        const QString coords = tr("X: %1 px   Y: %2 px").arg(g.x()).arg(g.y());
        QFont small = font();
        p.setFont(small);
        const QRect label = p.fontMetrics().boundingRect(coords).adjusted(-6, -3, 6, 3)
                                .translated(m_cursor + QPoint(16, 28));
        p.fillRect(label, QColor(20, 20, 20, 210));
        p.setPen(Qt::white);
        p.drawText(label, Qt::AlignCenter, coords);
    }
}

void PickerOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_cursor = event->position().toPoint();
    update();
    // Remove the crosshair from the other screens
    m_picker->cursorEntered(this);
}

void PickerOverlay::leaveEvent(QEvent *)
{
    clearCursor();
}

void PickerOverlay::clearCursor()
{
    if (m_cursor == QPoint(-1, -1))
        return;
    m_cursor = QPoint(-1, -1);
    update();
}

void PickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_picker->finish(true, m_screen->geometry().topLeft() + event->position().toPoint());
    else
        m_picker->finish(false);
}

void PickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        m_picker->finish(false);
}

TargetPicker::~TargetPicker()
{
    for (auto &o : std::as_const(m_overlays))
        if (o)
            o->close();
}

void TargetPicker::pick()
{
    if (!m_overlays.isEmpty())
        return;
    for (QScreen *screen : QGuiApplication::screens())
        m_overlays << new PickerOverlay(screen, this);
}

void TargetPicker::cursorEntered(PickerOverlay *active)
{
    for (auto &o : std::as_const(m_overlays))
        if (o && o != active)
            o->clearCursor();
}

void TargetPicker::finish(bool ok, QPoint pos)
{
    if (m_overlays.isEmpty())
        return;
    const auto overlays = std::exchange(m_overlays, {});
    // Do not delete the overlays from within their own event handler.
    QTimer::singleShot(0, this, [overlays] {
        for (auto &o : overlays)
            if (o)
                o->close();
    });
    if (ok)
        Q_EMIT targetSelected(pos);
    else
        Q_EMIT cancelled();
}
