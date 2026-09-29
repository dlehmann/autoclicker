#pragma once

#include <QPoint>
#include <QWidget>

#include <memory>
#include <optional>

class ClickWorker;
class KWinScript;
class MouseWatcher;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTimer;
class TargetPicker;
class VirtualPointer;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setTarget(QPoint pos);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class State { Idle, Countdown, Clicking };

    void selectTarget();
    void onStartButton();
    void beginClicking();
    void onWatcherReady();
    void stopClicking(const QString &reason);
    void poll();
    void setState(State state);
    void updateTargetLabel();
    void setClickCount(quint64 count);
    QString countdownText(int seconds) const;
    double clicksPerSecond() const;
    void loadSettings();
    void saveSettings() const;

    std::unique_ptr<VirtualPointer> m_pointer;
    std::unique_ptr<ClickWorker> m_worker;
    MouseWatcher *m_watcher;
    TargetPicker *m_picker;
    std::unique_ptr<KWinScript> m_keepAbove;

    QPushButton *m_selectButton;
    QLabel *m_targetLabel;
    QLabel *m_targetInline;
    QSpinBox *m_rateSpin;
    QRadioButton *m_perSecond;
    QRadioButton *m_perMinute;
    QSpinBox *m_maxClicks;
    QPushButton *m_startButton;
    QPushButton *m_banner;
    QStackedWidget *m_actionStack;
    QLabel *m_status;
    QLabel *m_clickCount;

    QTimer *m_pollTimer;
    QTimer *m_countdownTimer;
    QTimer *m_blinkTimer;
    QTimer *m_readyTimeout;
    int m_countdown = 0;

    State m_state = State::Idle;
    std::optional<QPoint> m_target;
};
