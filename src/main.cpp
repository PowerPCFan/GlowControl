#include "glowcontrol/ddc.h"
#include "glowcontrol/nativeosd.h"
#include "glowcontrol/osdwindow.h"
#include "glowcontrol/shortcut.h"
#include "glowcontrol/trayapp.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#include <algorithm>
#include <cstdio>

#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

namespace {
QList<MonitorValue> adjustAllMonitorsSynced(DDCA_Vcp_Feature_Code featureCode, int delta) {
    QList<MonitorValue> values;
    const QList<Monitor> monitors = detectMonitors();
    int currentValue = -1;
    bool foundCurrentValue = false;

    for (const Monitor &monitor : monitors) {
        int maximum = 100;
        const int current = getFeatureValue(monitor, featureCode, &maximum);
        if (current < 0) {
            continue;
        }

        if (!foundCurrentValue) {
            currentValue = current;
            foundCurrentValue = true;
        }

        values.append({monitor, current});
    }

    if (!foundCurrentValue) {
        return values;
    }

    const int next = std::clamp(currentValue + delta, 0, 100);
    for (MonitorValue &value : values) {
        int maximum = 100;
        const int current = getFeatureValue(value.monitor, featureCode, &maximum);
        if (current >= 0 && next != current) {
            setFeatureValue(value.monitor, featureCode, next, maximum);
        }

        value.value = next;
    }

    return values;
}
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(":/icons/GlowControl.svg"));

    QCommandLineParser parser;
    parser.setApplicationDescription("DDC/CI brightness and contrast tray app");
    parser.addHelpOption();

    QCommandLineOption modeOption("mode", "Run a one-shot adjustment for brightness or contrast and exit.", "brightness|contrast");
    QCommandLineOption directionOption("direction", "Adjust the selected mode by 5 percentage points.", "up|down");
    QCommandLineOption valueOption("value", "Set the selected mode to an absolute value from 0 to 100.", "0-100");
    QCommandLineOption osdOption("osd", "Show GlowControl's custom OSD for shortcut-mode changes.");
    QCommandLineOption nativeOsdOption("native-osd", "Show KDE Plasma's native brightness OSD for shortcut-mode brightness changes.");
    parser.addOption(modeOption);
    parser.addOption(directionOption);
    parser.addOption(valueOption);
    parser.addOption(osdOption);
    parser.addOption(nativeOsdOption);
    parser.process(app);

    ShortcutAction shortcutAction {};
    const bool commandMode = parser.isSet(modeOption)
        || parser.isSet(directionOption)
        || parser.isSet(valueOption)
        || parser.isSet(osdOption)
        || parser.isSet(nativeOsdOption);
    if (commandMode) {
        QString error;
        if (!parser.isSet(modeOption)) {
            std::fprintf(stderr, "Specify --mode brightness or --mode contrast.\n");
            return 2;
        }

        if (!parseModeAction(
                parser.value(modeOption).toLower(),
                parser.value(directionOption).toLower(),
                parser.value(valueOption),
                &shortcutAction,
                &error
            )) {
            std::fprintf(stderr, "%s\n", qPrintable(error));
            return 2;
        }

        if (parser.isSet(osdOption) && parser.isSet(nativeOsdOption)) {
            std::fprintf(stderr, "Specify --osd or --native-osd, not both.\n");
            return 2;
        }

        if (parser.isSet(nativeOsdOption) && shortcutAction.featureCode != BrightnessFeature) {
            std::fprintf(stderr, "--native-osd is only supported for brightness.\n");
            return 2;
        }
    }

    DDCA_Status status = ddca_init2("", DDCA_SYSLOG_NEVER, DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE, nullptr);
    if (status != DDCRC_OK) {
        if (commandMode) {
            std::fprintf(stderr, "Could not initialize libddcutil: %s\n", qPrintable(ddcStatusMessage(status)));
        } else {
            QMessageBox::critical(nullptr, "GlowControl", "Could not initialize libddcutil:\n" + ddcStatusMessage(status));
        }
        return 1;
    }

    if (commandMode) {
        const bool useNativeOsd = parser.isSet(nativeOsdOption);
        const QList<MonitorValue> values = shortcutAction.value >= 0
            ? setAllMonitors(shortcutAction.featureCode, shortcutAction.value)
            : (useNativeOsd
                ? adjustAllMonitorsSynced(shortcutAction.featureCode, shortcutAction.delta)
                : adjustAllMonitors(shortcutAction.featureCode, shortcutAction.delta));
        if (values.isEmpty()) {
            std::fprintf(stderr, "No DDC/CI monitors were adjusted.\n");
            return 1;
        }

        if (useNativeOsd) {
            showNativeBrightnessOsd(values.constFirst().value);
            return 0;
        }

        if (parser.isSet(osdOption)) {
            QList<OsdValue> osdValues;
            for (const MonitorValue &value : values) {
                osdValues.append({
                    QString("[Display %1] %2").arg(value.monitor.displayNumber).arg(value.monitor.name),
                    value.value
                });
            }

            OsdWindow osd;
            osd.showValues(shortcutAction.label, osdValues);
            QTimer::singleShot(1400, &app, &QApplication::quit);
            return app.exec();
        }

        return 0;
    }

    QApplication::setQuitOnLastWindowClosed(false);

    TrayApp trayApp;

    return app.exec();
}
