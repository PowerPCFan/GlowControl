#pragma once

#include "glowcontrol/controlpanel.h"
#include "glowcontrol/osdwindow.h"

#include <KStatusNotifierItem>
#include <QMap>
#include <QMenu>
#include <QObject>
#include <QTimer>

class TrayApp : public QObject {
    Q_OBJECT

public:
    explicit TrayApp(QObject *parent = nullptr);

private slots:
    void showPanel(const QPoint &pos = QPoint());

private:
    KStatusNotifierItem *tray;
    QMenu *contextMenu;
    ControlPanel *panel;
    OsdWindow *osd;
    QTimer *brightnessScrollTimer;
    QMap<int, int> brightnessValues;
    QMap<int, int> brightnessMaximumValues;
    QMap<int, Monitor> brightnessMonitors;

    bool refreshBrightnessCache();
    void adjustAllBrightness(int direction);
    void applyPendingBrightness();
    void feelLuckyBrightness();
    QList<OsdValue> pendingBrightnessOsdValues() const;
    QPoint panelPosition(const QPoint &requestedPosition) const;
};
