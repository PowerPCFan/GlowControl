#include "glowcontrol/trayapp.h"

#include "glowcontrol/nativeosd.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QIcon>
#include <QRandomGenerator>
#include <QScreen>

#include <algorithm>

TrayApp::TrayApp(QObject *parent)
    : QObject(parent) {
    panel = new ControlPanel;
    osd = new OsdWindow;
    brightnessScrollTimer = new QTimer(this);
    brightnessScrollTimer->setSingleShot(true);
    brightnessScrollTimer->setInterval(TrayScrollDebounceMs);

    tray = new KStatusNotifierItem("glowcontrol", this);
    tray->setCategory(KStatusNotifierItem::Hardware);
    tray->setStatus(KStatusNotifierItem::Active);
    tray->setTitle("GlowControl");
    tray->setStandardActionsEnabled(false);

    const QIcon appIcon(":/icons/GlowControl.svg");
    tray->setIconByPixmap(appIcon);
    tray->setToolTip(appIcon, "GlowControl", "Monitor brightness and contrast");

    contextMenu = new QMenu;
    contextMenu->addAction("Open", this, [=]() {
        showPanel();
    });
    contextMenu->addAction("I'm Feeling Lucky", this, &TrayApp::feelLuckyBrightness);
    contextMenu->addSeparator();
    contextMenu->addAction("Quit", qApp, &QApplication::quit);
    tray->setContextMenu(contextMenu);

    connect(tray, &KStatusNotifierItem::activateRequested, this, [=](bool, const QPoint &pos) {
        showPanel(pos);
    });

    connect(tray, &KStatusNotifierItem::scrollRequested, this, [=](int delta, Qt::Orientation orientation) {
        if (orientation == Qt::Vertical) {
            adjustAllBrightness(delta > 0 ? 1 : -1);
        }
    });

    connect(brightnessScrollTimer, &QTimer::timeout, this, &TrayApp::applyPendingBrightness);

    QTimer::singleShot(500, this, &TrayApp::refreshBrightnessCache);
}

void TrayApp::showPanel(const QPoint &pos) {
    if (brightnessScrollTimer->isActive()) {
        brightnessScrollTimer->stop();
        applyPendingBrightness();
    }

    brightnessValues.clear();
    brightnessMaximumValues.clear();
    brightnessMonitors.clear();
    panel->loadMonitors();
    panel->showControlsTab();
    panel->move(panelPosition(pos));
    panel->setWindowState(panel->windowState() & ~Qt::WindowMinimized);
    panel->show();
    panel->raise();
    panel->activateWindow();
}

bool TrayApp::refreshBrightnessCache() {
    const QList<Monitor> monitors = detectMonitors();
    brightnessValues.clear();
    brightnessMaximumValues.clear();
    brightnessMonitors.clear();

    for (const Monitor &monitor : monitors) {
        int maximum = 100;
        const int current = getFeatureValue(monitor, BrightnessFeature, &maximum);
        if (current < 0) {
            continue;
        }

        brightnessMonitors[monitor.displayNumber] = monitor;
        brightnessMaximumValues[monitor.displayNumber] = maximum;
        brightnessValues[monitor.displayNumber] = current;
    }

    return !brightnessValues.isEmpty();
}

void TrayApp::adjustAllBrightness(int direction) {
    if (!brightnessScrollTimer->isActive()) {
        panel->applyPendingChanges();
        if (!refreshBrightnessCache()) {
            return;
        }
    } else if (brightnessValues.isEmpty() && !refreshBrightnessCache()) {
        return;
    }

    const int delta = (direction > 0 ? 1 : -1) * panel->adjustmentStep();
    bool changed = false;

    if (panel->syncAllEnabled()) {
        int total = 0;
        for (auto it = brightnessValues.constBegin(); it != brightnessValues.constEnd(); ++it) {
            total += it.value();
        }

        const int current = brightnessValues.isEmpty() ? 0 : total / brightnessValues.size();
        const int next = std::clamp(current + delta, 0, 100);
        for (auto it = brightnessValues.begin(); it != brightnessValues.end(); ++it) {
            if (it.value() != next) {
                changed = true;
            }
            it.value() = next;
        }
    } else {
        for (auto it = brightnessValues.begin(); it != brightnessValues.end(); ++it) {
            const int current = it.value();
            const int next = std::clamp(current + delta, 0, 100);
            it.value() = next;
            if (next != current) {
                changed = true;
            }
        }
    }

    if (!changed) {
        return;
    }

    if (panel->osdEnabled()) {
        showBrightnessOsd(pendingBrightnessOsdValues());
    }
    brightnessScrollTimer->start();
}

void TrayApp::applyPendingBrightness() {
    for (auto it = brightnessValues.constBegin(); it != brightnessValues.constEnd(); ++it) {
        const Monitor monitor = brightnessMonitors.value(it.key());
        if (!monitor.displayRef) {
            continue;
        }

        setFeatureValue(monitor, BrightnessFeature, it.value(), brightnessMaximumValues.value(it.key(), 100));
    }

    if (panel->isVisible()) {
        panel->loadMonitors();
    }
}

void TrayApp::feelLuckyBrightness() {
    brightnessScrollTimer->stop();

    const int value = static_cast<int>(QRandomGenerator::global()->bounded(101));
    const QList<MonitorValue> values = setAllMonitors(BrightnessFeature, value);
    if (values.isEmpty()) {
        return;
    }

    brightnessValues.clear();
    brightnessMaximumValues.clear();
    brightnessMonitors.clear();

    QList<OsdValue> osdValues;
    for (const MonitorValue &monitorValue : values) {
        brightnessMonitors[monitorValue.monitor.displayNumber] = monitorValue.monitor;
        brightnessMaximumValues[monitorValue.monitor.displayNumber] = 100;
        brightnessValues[monitorValue.monitor.displayNumber] = monitorValue.value;
        osdValues.append({
            QString("[Display %1] %2").arg(monitorValue.monitor.displayNumber).arg(monitorValue.monitor.name),
            monitorValue.value
        });
    }

    if (panel->osdEnabled()) {
        showBrightnessOsd(osdValues);
    }

    if (panel->isVisible()) {
        panel->loadMonitors();
    }
}

void TrayApp::showBrightnessOsd(const QList<OsdValue> &values) {
    if (panel->nativeOsdEnabled()) {
        showNativeBrightnessOsd(values.isEmpty() ? 0 : values.constFirst().value);
        return;
    }

    osd->showValues("Brightness", values);
}

QList<OsdValue> TrayApp::pendingBrightnessOsdValues() const {
    QList<OsdValue> values;
    for (auto it = brightnessValues.constBegin(); it != brightnessValues.constEnd(); ++it) {
        const Monitor monitor = brightnessMonitors.value(it.key());
        const QString label = monitor.name.isEmpty()
            ? QString("Display %1").arg(it.key())
            : QString("[Display %1] %2").arg(it.key()).arg(monitor.name);
        values.append({label, it.value()});
    }

    return values;
}

QPoint TrayApp::panelPosition(const QPoint &requestedPosition) const {
    const QPoint anchor = requestedPosition.isNull() ? QCursor::pos() : requestedPosition;
    QScreen *screen = QGuiApplication::screenAt(anchor);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    if (!screen) {
        return anchor;
    }

    const QRect available = screen->availableGeometry();
    const QSize panelSize = panel->sizeHint();

    const int x = available.left() + (available.width() - panelSize.width()) / 2;
    const int y = available.top() + (available.height() - panelSize.height()) / 2;

    return QPoint(x, y);
}
