#pragma once

#include <QList>
#include <QString>

#include <ddcutil_c_api.h>

constexpr DDCA_Vcp_Feature_Code BrightnessFeature = 0x10;
constexpr DDCA_Vcp_Feature_Code ContrastFeature = 0x12;
constexpr int ShortcutStep = 5;
constexpr int TrayScrollDebounceMs = 1000;

struct DdcFeature {
    DDCA_Vcp_Feature_Code code;
    QString label;
};

struct Monitor {
    int displayNumber = 0;
    QString name;
    DDCA_Display_Ref displayRef = nullptr;
};

struct MonitorValue {
    Monitor monitor;
    int value = -1;
};

extern const QList<DdcFeature> ControlFeatures;

QString ddcStatusMessage(DDCA_Status status);
QList<Monitor> detectMonitors();
QString controlKey(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode);
int getFeatureValue(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode, int *maximumValue = nullptr);
bool setFeatureValue(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode, int value, int maximumValue);
QList<MonitorValue> adjustAllMonitors(DDCA_Vcp_Feature_Code featureCode, int delta);
QList<MonitorValue> setAllMonitors(DDCA_Vcp_Feature_Code featureCode, int value);
