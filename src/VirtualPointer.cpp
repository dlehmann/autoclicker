#include "VirtualPointer.h"

#include <linux/uinput.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace {
constexpr int AbsMax = 65535;
}

VirtualPointer::VirtualPointer()
{
    m_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        m_error = tr("Could not open /dev/uinput: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return;
    }

    ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(m_fd, UI_SET_EVBIT, EV_ABS);
    ioctl(m_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(m_fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(m_fd, UI_SET_PROPBIT, INPUT_PROP_POINTER);

    for (int axis : {ABS_X, ABS_Y}) {
        uinput_abs_setup abs {};
        abs.code = axis;
        abs.absinfo.minimum = 0;
        abs.absinfo.maximum = AbsMax;
        ioctl(m_fd, UI_ABS_SETUP, &abs);
    }

    uinput_setup setup {};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1209;
    setup.id.product = 0xac11;
    setup.id.version = 1;
    std::strncpy(setup.name, DeviceName, UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(m_fd, UI_DEV_SETUP, &setup) < 0 || ioctl(m_fd, UI_DEV_CREATE) < 0) {
        m_error = tr("Could not create the uinput device: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)));
        ::close(m_fd);
        m_fd = -1;
    }
}

VirtualPointer::~VirtualPointer()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        ::close(m_fd);
    }
}

void VirtualPointer::sendEvent(int type, int code, int value)
{
    input_event ev {};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    [[maybe_unused]] auto n = ::write(m_fd, &ev, sizeof(ev));
}

void VirtualPointer::moveTo(QPoint pos, const QRect &desktop)
{
    if (m_fd < 0 || desktop.isEmpty())
        return;

    auto scale = [](int v, int origin, int size) {
        // +0.5: aim at the pixel centre
        const double rel = (v - origin + 0.5) / size;
        return std::clamp(static_cast<int>(rel * (AbsMax + 1)), 0, AbsMax);
    };
    const int x = scale(pos.x(), desktop.x(), desktop.width());
    const int y = scale(pos.y(), desktop.y(), desktop.height());

    // The kernel drops ABS events whose value has not changed. If the real
    // pointer has moved in the meantime, the position would not be set again.
    // So first move slightly off (less than a pixel), then exactly onto it.
    // Both axes together, so no intermediate position far from the target occurs.
    sendEvent(EV_ABS, ABS_X, x > 0 ? x - 1 : x + 1);
    sendEvent(EV_ABS, ABS_Y, y > 0 ? y - 1 : y + 1);
    sendEvent(EV_SYN, SYN_REPORT, 0);
    sendEvent(EV_ABS, ABS_X, x);
    sendEvent(EV_ABS, ABS_Y, y);
    sendEvent(EV_SYN, SYN_REPORT, 0);
}

void VirtualPointer::click(std::chrono::microseconds hold)
{
    if (m_fd < 0)
        return;
    sendEvent(EV_KEY, BTN_LEFT, 1);
    sendEvent(EV_SYN, SYN_REPORT, 0);
    // Some applications ignore clicks without any hold time.
    std::this_thread::sleep_for(hold);
    sendEvent(EV_KEY, BTN_LEFT, 0);
    sendEvent(EV_SYN, SYN_REPORT, 0);
}
