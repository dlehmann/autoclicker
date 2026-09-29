#include "MainWindow.h"
#include "ClickWorker.h"
#include "KWinScript.h"
#include "MouseWatcher.h"
#include "TargetPicker.h"
#include "VirtualPointer.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int CountdownSeconds = 2;
// Most applications do not reliably accept more than 50 clicks per second;
// anything faster than 600 per minute is better expressed per second.
constexpr int MaxPerSecond = 50;
constexpr int MaxPerMinute = 600;
constexpr int DefaultRate = 5; // clicks per second on every program start

// Keeps this process's windows above all others. On Wayland, KWin ignores
// Qt::WindowStaysOnTopHint, so this is done with a KWin script.
const char *KeepAboveScript = R"JS(
var pid = %1;
function apply(w) {
    if (w && w.pid === pid && (w.normalWindow || w.dialog))
        w.keepAbove = true;
}
workspace.windowList().forEach(apply);
workspace.windowAdded.connect(apply);
)JS";

const char *BannerStyle =
    "QPushButton { background:%1; color:white; font-weight:bold; font-size:12pt;"
    " border:none; border-radius:6px; padding:0 12px; }";

QRect desktopGeometry()
{
    QRect r;
    for (QScreen *s : QGuiApplication::screens())
        r = r.united(s->geometry());
    return r;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_pointer(std::make_unique<VirtualPointer>())
    , m_worker(std::make_unique<ClickWorker>(*m_pointer))
    , m_watcher(new MouseWatcher(this))
    , m_picker(new TargetPicker(this))
    , m_keepAbove(std::make_unique<KWinScript>(QStringLiteral("autoclicker-keepabove")))
{
    setWindowTitle(tr("Autoclicker"));
    setWindowFlag(Qt::WindowStaysOnTopHint); // effective on X11
    m_keepAbove->load(QString::fromUtf8(KeepAboveScript).arg(QApplication::applicationPid()));

    // Consistent spacing: outer margin, between related items, between groups
    const int margin = 12;
    const int gap = 8;
    const int groupGap = 16;

    m_selectButton = new QPushButton(tr("Select target"));

    m_rateSpin = new QSpinBox;
    m_rateSpin->setRange(1, MaxPerSecond);

    m_perSecond = new QRadioButton(tr("per second"));
    m_perMinute = new QRadioButton(tr("per minute"));
    auto *unitGroup = new QButtonGroup(this);
    unitGroup->addButton(m_perSecond);
    unitGroup->addButton(m_perMinute);
    // Convert the rate when switching units (e.g. 5/s -> 300/min)
    connect(m_perSecond, &QRadioButton::toggled, this, [this](bool perSecond) {
        const int v = m_rateSpin->value();
        m_rateSpin->setRange(1, perSecond ? MaxPerSecond : MaxPerMinute);
        m_rateSpin->setValue(perSecond ? qMax(1, (v + 30) / 60) : v * 60);
    });

    auto *rateRow = new QHBoxLayout;
    rateRow->setSpacing(gap);
    rateRow->addWidget(m_rateSpin);
    rateRow->addWidget(m_perSecond);
    rateRow->addSpacing(groupGap - gap);
    rateRow->addWidget(m_perMinute);
    rateRow->addStretch();

    m_maxClicks = new QSpinBox;
    m_maxClicks->setRange(0, 1'000'000'000);
    m_maxClicks->setMinimumWidth(m_maxClicks->fontMetrics().horizontalAdvance(QStringLiteral("0000000000")));
    auto *maxClicksRow = new QHBoxLayout;
    maxClicksRow->setSpacing(gap);
    maxClicksRow->addWidget(m_maxClicks);
    maxClicksRow->addWidget(new QLabel(tr("clicks (0 = unlimited)")));
    maxClicksRow->addStretch();

    // Give both spin boxes the same width so they line up
    const int spinWidth = m_maxClicks->sizeHint().width();
    m_rateSpin->setFixedWidth(spinWidth);
    m_maxClicks->setFixedWidth(spinWidth);

    m_targetInline = new QLabel;
    auto *targetRow = new QHBoxLayout;
    targetRow->setSpacing(gap);
    targetRow->addWidget(m_selectButton);
    targetRow->addWidget(m_targetInline);
    targetRow->addStretch();

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(groupGap);
    form->setVerticalSpacing(gap);
    form->addRow(tr("Target:"), targetRow);
    form->addRow(tr("Click rate:"), rateRow);
    form->addRow(tr("Stop after:"), maxClicksRow);

    m_startButton = new QPushButton(tr("Start clicking"));

    // The banner replaces the start button while the clicker is running. During
    // the countdown clicking it cancels; afterwards it is display only.
    // Longest text and final style are set here so the window size fits.
    m_banner = new QPushButton(countdownText(CountdownSeconds));
    m_banner->setStyleSheet(QString::fromUtf8(BannerStyle).arg(QStringLiteral("#b3261e")));
    m_banner->setCursor(Qt::PointingHandCursor);

    m_actionStack = new QStackedWidget;
    m_actionStack->addWidget(m_startButton);
    m_actionStack->addWidget(m_banner);
    const int actionHeight = m_banner->sizeHint().height() + 12;
    m_startButton->setFixedHeight(actionHeight);
    m_banner->setFixedHeight(actionHeight);

    // Status bar: message | target | click counter
    m_status = new QLabel;
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_targetLabel = new QLabel;
    m_targetLabel->setMinimumWidth(
        m_targetLabel->fontMetrics().horizontalAdvance(tr("Target: %1 px, %2 px").arg(88888).arg(88888)));
    m_clickCount = new QLabel;
    m_targetLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_clickCount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_clickCount->setMinimumWidth(
        m_clickCount->fontMetrics().horizontalAdvance(tr("Clicks: %1").arg(8888888)));
    auto *statusBar = new QStatusBar;
    statusBar->setSizeGripEnabled(false);
    statusBar->setContentsMargins(margin - 2, 0, margin - 2, 2);
    statusBar->setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));
    statusBar->addWidget(m_status, 1);
    statusBar->addPermanentWidget(m_targetLabel);
    statusBar->addPermanentWidget(m_clickCount);
    setClickCount(0);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(margin, margin, margin, margin);
    content->setSpacing(0);
    content->addLayout(form);
    content->addSpacing(groupGap);
    content->addWidget(m_actionStack);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(content);
    layout->addWidget(separator);
    layout->addWidget(statusBar);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    // Makes "Clicking …" blink in the status bar while clicking
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, [this] {
        m_status->setText(m_status->text().isEmpty() ? tr("Clicking …") : QString());
    });
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);

    connect(m_selectButton, &QPushButton::clicked, this, &MainWindow::selectTarget);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButton);
    connect(m_banner, &QPushButton::clicked, this, &MainWindow::onStartButton);
    connect(m_rateSpin, &QSpinBox::valueChanged, this, [this] {
        if (m_state == State::Clicking)
            m_worker->setRate(clicksPerSecond());
    });
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::poll);
    connect(m_countdownTimer, &QTimer::timeout, this, [this] {
        if (--m_countdown <= 0) {
            m_countdownTimer->stop();
            beginClicking();
        } else {
            m_banner->setText(countdownText(m_countdown));
        }
    });
    connect(m_watcher, &MouseWatcher::ready, this, &MainWindow::onWatcherReady);
    connect(m_watcher, &MouseWatcher::userActivity, this, [this] {
        if (m_state == State::Clicking)
            stopClicking(tr("Stopped by mouse movement"));
    });
    m_readyTimeout = new QTimer(this);
    m_readyTimeout->setSingleShot(true);
    m_readyTimeout->setInterval(3000);
    connect(m_readyTimeout, &QTimer::timeout, this, [this] {
        m_watcher->stop();
        setState(State::Idle);
        m_status->setText(tr("KWin did not respond – start cancelled"));
    });

    connect(m_picker, &TargetPicker::targetSelected, this, &MainWindow::setTarget);

    loadSettings();
    updateTargetLabel();
    setState(State::Idle);

    m_status->setText(m_pointer->isValid() ? tr("No target selected yet") : m_pointer->errorString());

    // Fix the size once so the layout never shifts later
    setFixedSize(sizeHint());
}

MainWindow::~MainWindow()
{
    m_worker->stop();
    saveSettings();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_worker->stop();
    m_watcher->stop();
    event->accept();
}

double MainWindow::clicksPerSecond() const
{
    const double v = m_rateSpin->value();
    return m_perSecond->isChecked() ? v : v / 60.0;
}

void MainWindow::setTarget(QPoint pos)
{
    m_target = pos;
    updateTargetLabel();
    if (m_pointer->isValid())
        m_status->setText(tr("Ready"));
    setState(State::Idle);
}

void MainWindow::updateTargetLabel()
{
    // Without a target the hint is shown on the left of the status bar instead.
    m_targetLabel->setVisible(m_target.has_value());
    m_targetInline->setVisible(m_target.has_value());
    if (m_target) {
        m_targetLabel->setText(tr("Target: %1 px, %2 px").arg(m_target->x()).arg(m_target->y()));
        m_targetInline->setText(tr("X: %1 px   Y: %2 px").arg(m_target->x()).arg(m_target->y()));
    }
}

void MainWindow::selectTarget()
{
    // The window stays visible: the overlay is above all windows anyway.
    // Hiding and showing it again would make KWin place the window anew.
    m_picker->pick();
}

void MainWindow::onStartButton()
{
    if (m_state == State::Countdown) {
        m_countdownTimer->stop();
        m_readyTimeout->stop();
        m_watcher->stop();
        m_status->setText(tr("Cancelled"));
        setState(State::Idle);
        return;
    }
    if (!m_target || !m_pointer->isValid())
        return;

    m_countdown = CountdownSeconds;
    m_banner->setText(countdownText(m_countdown));
    setState(State::Countdown);
    m_countdownTimer->start();
}

void MainWindow::beginClicking()
{
    // Watch first, then click: never click without a way to stop.
    if (!m_watcher->start(*m_target)) {
        setState(State::Idle);
        QMessageBox::warning(this, tr("Mouse monitoring not available"),
                             m_watcher->errorString());
        return;
    }
    m_readyTimeout->start();
}

void MainWindow::onWatcherReady()
{
    if (!m_readyTimeout->isActive())
        return;
    m_readyTimeout->stop();
    m_status->setText(tr("Clicking …"));
    m_blinkTimer->start();
    setClickCount(0);
    setState(State::Clicking);
    m_worker->start(*m_target, desktopGeometry(), clicksPerSecond(),
                    static_cast<quint64>(m_maxClicks->value()));
    m_pollTimer->start();
}

void MainWindow::stopClicking(const QString &reason)
{
    m_blinkTimer->stop();
    m_worker->stop();
    m_watcher->stop();
    m_pollTimer->stop();
    m_status->setText(reason);
    setClickCount(m_worker->clickCount());
    setState(State::Idle);
}

void MainWindow::poll()
{
    if (!m_worker->isRunning()) {
        stopClicking(tr("Click limit reached"));
        return;
    }
    setClickCount(m_worker->clickCount());
}

QString MainWindow::countdownText(int seconds) const
{
    return tr("Starting in %1 … keep the mouse still (click = cancel)").arg(seconds);
}

void MainWindow::setClickCount(quint64 count)
{
    m_clickCount->setText(tr("Clicks: %1").arg(count));
}

void MainWindow::setState(State state)
{
    m_state = state;
    const bool idle = state == State::Idle;

    m_selectButton->setEnabled(idle);
    m_perSecond->setEnabled(idle);
    m_perMinute->setEnabled(idle);
    m_maxClicks->setEnabled(idle);

    const QString bannerStyle = QString::fromUtf8(BannerStyle);
    switch (state) {
    case State::Idle:
        m_startButton->setEnabled(m_target.has_value() && m_pointer->isValid());
        m_actionStack->setCurrentWidget(m_startButton);
        break;
    case State::Countdown:
        m_banner->setEnabled(true);
        m_banner->setStyleSheet(bannerStyle.arg(QStringLiteral("#8a6d00")));
        m_actionStack->setCurrentWidget(m_banner);
        break;
    case State::Clicking:
        // Display only: the pointer keeps jumping to the target, so it cannot be clicked.
        m_banner->setEnabled(false);
        m_banner->setStyleSheet(bannerStyle.arg(QStringLiteral("#b3261e")));
        m_banner->setText(tr("● Clicking – move the mouse to stop"));
        m_actionStack->setCurrentWidget(m_banner);
        break;
    }
}

void MainWindow::loadSettings()
{
    QSettings s;
    // The click rate is not saved: every start begins with 5 per second.
    m_perSecond->setChecked(true);
    m_rateSpin->setValue(DefaultRate);
    m_maxClicks->setValue(s.value(QStringLiteral("maxClicks"), 0).toInt());
}

void MainWindow::saveSettings() const
{
    // The target and click rate are intentionally not saved
    QSettings s;
    s.setValue(QStringLiteral("maxClicks"), m_maxClicks->value());
}
