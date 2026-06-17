#include "glowcontrol/ddc.h"
#include "glowcontrol/osdwindow.h"
#include "glowcontrol/shortcut.h"
#include "glowcontrol/trayapp.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#include <cstdio>

#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(":/icons/GlowControl.svg"));

    QCommandLineParser parser;
    parser.setApplicationDescription("DDC/CI brightness and contrast tray app");
    parser.addHelpOption();

    QCommandLineOption modeOption("mode", "Run a one-shot adjustment for brightness or contrast and exit.", "brightness|contrast");
    QCommandLineOption directionOption("direction", "Adjust the selected mode by 5 percentage points.", "up|down");
    QCommandLineOption valueOption("value", "Set the selected mode to an absolute value from 0 to 100.", "0-100");
    QCommandLineOption osdOption("osd", "Show an OSD for shortcut-mode changes.");
    parser.addOption(modeOption);
    parser.addOption(directionOption);
    parser.addOption(valueOption);
    parser.addOption(osdOption);
    parser.process(app);

    ShortcutAction shortcutAction {};
    const bool commandMode = parser.isSet(modeOption) || parser.isSet(directionOption) || parser.isSet(valueOption);
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
        const QList<MonitorValue> values = shortcutAction.value >= 0
            ? setAllMonitors(shortcutAction.featureCode, shortcutAction.value)
            : adjustAllMonitors(shortcutAction.featureCode, shortcutAction.delta);
        if (values.isEmpty()) {
            std::fprintf(stderr, "No DDC/CI monitors were adjusted.\n");
            return 1;
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
