#pragma once

#include "glowcontrol/ddc.h"

#include <QString>

struct ShortcutAction {
    DDCA_Vcp_Feature_Code featureCode = BrightnessFeature;
    QString label;
    int delta = 0;
    int value = -1;
};

bool parseModeAction(const QString &mode, const QString &direction, const QString &valueText, ShortcutAction *action, QString *error);
