#include "glowcontrol/nativeosd.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include <algorithm>

void showNativeBrightnessOsd(int value) {
    const int clampedValue = std::clamp(value, 0, 100);
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.kde.plasmashell",
        "/org/kde/osdService",
        "org.kde.osdService",
        "brightnessChanged"
    );
    message << clampedValue;
    QDBusConnection::sessionBus().send(message);
}
