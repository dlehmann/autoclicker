#pragma once

#include <QCoreApplication>
#include <QPoint>
#include <QRect>
#include <QString>

#include <chrono>

// Virtual pointer device via /dev/uinput with absolute coordinates.
// KWin maps absolute pointer devices onto the whole desktop area.
class VirtualPointer
{
    Q_DECLARE_TR_FUNCTIONS(VirtualPointer)

public:
    static constexpr const char *DeviceName = "Autoclicker Virtual Pointer";

    VirtualPointer();
    ~VirtualPointer();

    VirtualPointer(const VirtualPointer &) = delete;
    VirtualPointer &operator=(const VirtualPointer &) = delete;

    bool isValid() const { return m_fd >= 0; }
    QString errorString() const { return m_error; }

    // desktop: bounding box of all screens in logical coordinates.
    void moveTo(QPoint pos, const QRect &desktop);
    void click(std::chrono::microseconds hold);

private:
    void sendEvent(int type, int code, int value);

    int m_fd = -1;
    QString m_error;
};
