#include "glowcontrol/shortcut.h"

#include <algorithm>

bool parseModeAction(const QString &mode, const QString &direction, const QString &valueText, ShortcutAction *action, QString *error) {
    if (mode == "brightness") {
        action->featureCode = BrightnessFeature;
        action->label = "Brightness";
    } else if (mode == "contrast") {
        action->featureCode = ContrastFeature;
        action->label = "Contrast";
    } else {
        *error = "Invalid --mode value. Use brightness or contrast.";
        return false;
    }

    const bool hasDirection = !direction.isEmpty();
    const bool hasValue = !valueText.isEmpty();
    if (hasDirection == hasValue) {
        *error = "Specify exactly one of --direction or --value.";
        return false;
    }

    if (hasDirection) {
        if (direction == "up") {
            action->delta = ShortcutStep;
            action->value = -1;
            return true;
        }

        if (direction == "down") {
            action->delta = -ShortcutStep;
            action->value = -1;
            return true;
        }

        *error = "Invalid --direction value. Use up or down.";
        return false;
    }

    bool ok = false;
    const int value = valueText.toInt(&ok);
    if (!ok || value < 0 || value > 100) {
        *error = "Invalid --value. Use an integer from 0 to 100.";
        return false;
    }

    action->delta = 0;
    action->value = std::clamp(value, 0, 100);
    return true;
}
