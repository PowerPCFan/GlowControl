#include "glowcontrol/ddc.h"

#include <algorithm>
#include <cstring>

#include <ddcutil_status_codes.h>

const QList<DdcFeature> ControlFeatures {
    {BrightnessFeature, "Brightness"},
    {ContrastFeature, "Contrast"},
};

QString ddcStatusMessage(DDCA_Status status) {
    const char *name = ddca_rc_name(status);
    const char *description = ddca_rc_desc(status);

    if (name && description) {
        return QString("%1: %2").arg(QString::fromUtf8(name), QString::fromUtf8(description));
    }

    if (description) {
        return QString::fromUtf8(description);
    }

    return QString("DDC/CI error %1").arg(status);
}

QList<Monitor> detectMonitors() {
    QList<Monitor> monitors;

    DDCA_Status status = ddca_redetect_displays();
    if (status != DDCRC_OK) {
        return monitors;
    }

    DDCA_Display_Info_List *displayList = nullptr;
    status = ddca_get_display_info_list2(false, &displayList);
    if (status != DDCRC_OK || !displayList) {
        return monitors;
    }

    for (int i = 0; i < displayList->ct; ++i) {
        const DDCA_Display_Info &info = displayList->info[i];

        Monitor monitor;
        monitor.displayNumber = info.dispno;
        monitor.displayRef = info.dref;

        const auto modelLength = strnlen(info.model_name, sizeof(info.model_name));
        monitor.name = QString::fromLatin1(info.model_name, static_cast<int>(modelLength)).trimmed();
        if (monitor.name.isEmpty()) {
            monitor.name = QString("Display %1").arg(monitor.displayNumber);
        }

        monitors.append(monitor);
    }

    ddca_free_display_info_list(displayList);
    return monitors;
}

QString controlKey(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode) {
    return QString("%1:%2").arg(monitor.displayNumber).arg(featureCode);
}

int getFeatureValue(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode, int *maximumValue) {
    DDCA_Display_Handle displayHandle = nullptr;
    DDCA_Status status = ddca_open_display2(monitor.displayRef, true, &displayHandle);
    if (status != DDCRC_OK) {
        return -1;
    }

    DDCA_Non_Table_Vcp_Value value {};
    status = ddca_get_non_table_vcp_value(displayHandle, featureCode, &value);
    ddca_close_display(displayHandle);

    if (status != DDCRC_OK) {
        return -1;
    }

    const int current = (static_cast<int>(value.sh) << 8) | value.sl;
    const int maximum = (static_cast<int>(value.mh) << 8) | value.ml;
    if (maximumValue) {
        *maximumValue = maximum > 0 ? maximum : 100;
    }

    if (maximum > 0 && maximum != 100) {
        return std::clamp((current * 100) / maximum, 0, 100);
    }

    return std::clamp(current, 0, 100);
}

bool setFeatureValue(const Monitor &monitor, DDCA_Vcp_Feature_Code featureCode, int value, int maximumValue) {
    DDCA_Display_Handle displayHandle = nullptr;
    DDCA_Status status = ddca_open_display2(monitor.displayRef, true, &displayHandle);
    if (status != DDCRC_OK) {
        return false;
    }

    const int clampedPercent = std::clamp(value, 0, 100);
    const int clampedMaximum = std::clamp(maximumValue, 1, 0xffff);
    const int clampedValue = std::clamp((clampedPercent * clampedMaximum) / 100, 0, clampedMaximum);
    status = ddca_set_non_table_vcp_value(
        displayHandle,
        featureCode,
        static_cast<uint8_t>(clampedValue >> 8),
        static_cast<uint8_t>(clampedValue & 0xff)
    );

    ddca_close_display(displayHandle);
    return status == DDCRC_OK;
}

QList<MonitorValue> adjustAllMonitors(DDCA_Vcp_Feature_Code featureCode, int delta) {
    QList<MonitorValue> values;
    const QList<Monitor> monitors = detectMonitors();

    for (const Monitor &monitor : monitors) {
        int maximum = 100;
        const int current = getFeatureValue(monitor, featureCode, &maximum);
        if (current < 0) {
            continue;
        }

        const int next = std::clamp(current + delta, 0, 100);
        if (next != current) {
            setFeatureValue(monitor, featureCode, next, maximum);
        }

        values.append({monitor, next});
    }

    return values;
}

QList<MonitorValue> setAllMonitors(DDCA_Vcp_Feature_Code featureCode, int value) {
    QList<MonitorValue> values;
    const QList<Monitor> monitors = detectMonitors();
    const int next = std::clamp(value, 0, 100);

    for (const Monitor &monitor : monitors) {
        int maximum = 100;
        const int current = getFeatureValue(monitor, featureCode, &maximum);
        if (current < 0) {
            continue;
        }

        if (next != current) {
            setFeatureValue(monitor, featureCode, next, maximum);
        }

        values.append({monitor, next});
    }

    return values;
}
