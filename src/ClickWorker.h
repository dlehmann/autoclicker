#pragma once

#include <QPoint>
#include <QRect>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class VirtualPointer;

// Clicks on the target at a fixed rate in a dedicated thread.
class ClickWorker
{
public:
    explicit ClickWorker(VirtualPointer &pointer);
    ~ClickWorker();

    // maxClicks: 0 = unlimited
    void start(QPoint target, QRect desktop, double clicksPerSecond, quint64 maxClicks);
    void stop();

    void setRate(double clicksPerSecond);
    bool isRunning() const { return m_running; }
    quint64 clickCount() const { return m_count; }

private:
    void run(QPoint target, QRect desktop, quint64 maxClicks);

    VirtualPointer &m_pointer;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running {false};
    std::atomic<bool> m_stopRequested {false};
    std::atomic<double> m_rate {10.0};
    std::atomic<quint64> m_count {0};
};
