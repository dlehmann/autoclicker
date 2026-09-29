#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QWidget>

class TargetPicker;

// Semi-transparent full-screen overlay on one screen with a crosshair cursor.
class PickerOverlay : public QWidget
{
    Q_OBJECT

public:
    PickerOverlay(QScreen *screen, TargetPicker *picker);

    void clearCursor();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QScreen *m_screen;
    TargetPicker *m_picker;
    QPoint m_cursor {-1, -1};
};

// Shows an overlay on every screen; a click yields the global position in
// logical desktop coordinates.
class TargetPicker : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~TargetPicker() override;

    void pick();

Q_SIGNALS:
    void targetSelected(QPoint globalPos);
    void cancelled();

private:
    friend class PickerOverlay;
    void cursorEntered(PickerOverlay *active);
    void finish(bool ok, QPoint pos = {});

    QList<QPointer<PickerOverlay>> m_overlays;
};
