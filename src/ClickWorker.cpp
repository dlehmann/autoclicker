#include "ClickWorker.h"
#include "VirtualPointer.h"

using Clock = std::chrono::steady_clock;

ClickWorker::ClickWorker(VirtualPointer &pointer)
    : m_pointer(pointer)
{
}

ClickWorker::~ClickWorker()
{
    stop();
}

void ClickWorker::start(QPoint target, QRect desktop, double clicksPerSecond, quint64 maxClicks)
{
    stop();
    m_rate = clicksPerSecond;
    m_count = 0;
    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread(&ClickWorker::run, this, target, desktop, maxClicks);
}

void ClickWorker::stop()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    m_running = false;
}

void ClickWorker::setRate(double clicksPerSecond)
{
    m_rate = clicksPerSecond;
    m_cv.notify_all();
}

void ClickWorker::run(QPoint target, QRect desktop, quint64 maxClicks)
{
    auto interval = [this] {
        return std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / m_rate));
    };

    // Scheduled time of the last click; the first click happens immediately.
    Clock::time_point scheduled {};

    std::unique_lock lock(m_mutex);
    while (!m_stopRequested) {
        Clock::time_point next = Clock::now();
        if (m_count > 0) {
            // Recalculate on every wake-up, the rate may change.
            for (next = scheduled + interval(); !m_stopRequested && Clock::now() < next;
                 next = scheduled + interval())
                m_cv.wait_until(lock, next);
        }
        if (m_stopRequested)
            break;

        // Hold time like a real click (50 ms), but at high rates at most a
        // quarter of the click interval.
        const auto hold = std::min<std::chrono::microseconds>(
            std::chrono::milliseconds(50),
            std::chrono::duration_cast<std::chrono::microseconds>(interval()) / 4);

        lock.unlock();
        m_pointer.moveTo(target, desktop);
        m_pointer.click(hold);
        lock.lock();

        // If the click was more than one interval late, do not catch up.
        const auto now = Clock::now();
        scheduled = (now - next > interval()) ? now : next;

        if (++m_count >= maxClicks && maxClicks > 0)
            break;
    }
    m_running = false;
}
