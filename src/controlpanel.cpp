#include "glowcontrol/controlpanel.h"

#include <algorithm>
#include <QCloseEvent>
#include <QLabel>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QTabBar>

namespace {
constexpr const char *SettingsOrganization = "GlowControl";
constexpr const char *SettingsApplication = "GlowControl";
constexpr const char *SyncAllKey = "syncAllMonitors";
constexpr const char *ShowOsdKey = "showOsd";
constexpr const char *StepSizeKey = "stepSize";
}

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent) {
    loadSettings();

    setWindowTitle("GlowControl");
    setWindowFlags(Qt::Window);

    QVBoxLayout *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(0);

    tabWidget = new QTabWidget(this);
    QWidget *controlsPage = new QWidget(tabWidget);
    mainLayout = new QVBoxLayout(controlsPage);
    mainLayout->setContentsMargins(12, 10, 12, 12);
    mainLayout->setSpacing(8);

    tabWidget->addTab(controlsPage, "Controls");
    addSettingsPage(tabWidget);
    windowLayout->addWidget(tabWidget);

    connect(tabWidget, &QTabWidget::currentChanged, this, [this]() {
        reflowToCurrentTab();
    });
}

void ControlPanel::loadMonitors() {
    rebuildControls(true);
}

bool ControlPanel::syncAllEnabled() const {
    return syncAllMonitors;
}

bool ControlPanel::osdEnabled() const {
    return showOsd;
}

int ControlPanel::adjustmentStep() const {
    return std::clamp(stepSize, 1, 25);
}

void ControlPanel::applyPendingChanges() {
    flushPendingValues();
}

void ControlPanel::showControlsTab() {
    if (tabWidget) {
        tabWidget->setCurrentIndex(0);
        reflowToCurrentTab();
    }
}

void ControlPanel::loadSettings() {
    QSettings settings(SettingsOrganization, SettingsApplication);

    syncAllMonitors = settings.value(SyncAllKey, syncAllMonitors).toBool();
    showOsd = settings.value(ShowOsdKey, showOsd).toBool();
    stepSize = std::clamp(settings.value(StepSizeKey, stepSize).toInt(), 1, 25);
}

void ControlPanel::saveSetting(const char *key, const QVariant &value) {
    QSettings settings(SettingsOrganization, SettingsApplication);
    settings.setValue(key, value);
}

void ControlPanel::rebuildControls(bool readMonitorValues) {
    flushPendingValues();

    qDeleteAll(timers);
    timers.clear();
    pendingValues.clear();
    if (readMonitorValues) {
        maximumValues.clear();
    }

    while (mainLayout->count() > 0) {
        QLayoutItem *item = mainLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (readMonitorValues || monitors.isEmpty()) {
        monitors = detectMonitors();
    }

    if (monitors.isEmpty()) {
        QLabel *emptyLabel = new QLabel("No DDC/CI monitors found.\n", this);
        mainLayout->addWidget(emptyLabel);
        adjustSize();
        reflowToCurrentTab();
        return;
    }

    if (syncAllMonitors) {
        addSyncedMonitorControl(readMonitorValues);
    } else {
        for (const Monitor &monitor : monitors) {
            addMonitorControl(monitor, readMonitorValues);
        }
    }

    adjustSize();
    reflowToCurrentTab();
}

void ControlPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        QTimer::singleShot(0, this, &QWidget::hide);
    }

    QWidget::changeEvent(event);
}

void ControlPanel::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}

void ControlPanel::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ControlPanel::addSettingsPage(QTabWidget *tabs) {
    QWidget *settingsPage = new QWidget(tabs);
    QVBoxLayout *layout = new QVBoxLayout(settingsPage);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    syncAllCheckBox = new QCheckBox("Sync All Monitors", settingsPage);
    syncAllCheckBox->setChecked(syncAllMonitors);
    connect(syncAllCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        syncAllMonitors = checked;
        saveSetting(SyncAllKey, syncAllMonitors);
        rebuildControls(false);
        QTimer::singleShot(50, this, &ControlPanel::loadMonitors);
    });
    layout->addWidget(syncAllCheckBox);

    osdCheckBox = new QCheckBox("Show OSD", settingsPage);
    osdCheckBox->setChecked(showOsd);
    connect(osdCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        showOsd = checked;
        saveSetting(ShowOsdKey, showOsd);
    });
    layout->addWidget(osdCheckBox);

    QWidget *stepRow = new QWidget(settingsPage);
    QHBoxLayout *stepLayout = new QHBoxLayout(stepRow);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLayout->setSpacing(8);

    QLabel *stepLabel = new QLabel("Adjustment Step", stepRow);
    adjustmentStepSpinBox = new QSpinBox(stepRow);
    adjustmentStepSpinBox->setRange(1, 25);
    adjustmentStepSpinBox->setSingleStep(1);
    adjustmentStepSpinBox->setSuffix("%");
    adjustmentStepSpinBox->setValue(stepSize);
    connect(adjustmentStepSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        stepSize = std::clamp(value, 1, 25);
        saveSetting(StepSizeKey, stepSize);
        updateSliderSteps();
    });

    stepLayout->addWidget(stepLabel);
    stepLayout->addWidget(adjustmentStepSpinBox);
    stepLayout->addStretch();
    layout->addWidget(stepRow);
    layout->addStretch();

    tabs->addTab(settingsPage, "Settings");
}

void ControlPanel::updateSliderSteps() {
    const int step = adjustmentStep();
    const QList<QSlider*> sliders = findChildren<QSlider*>();
    for (QSlider *slider : sliders) {
        slider->setSingleStep(step);
        slider->setPageStep(step);
    }
}

void ControlPanel::reflowToCurrentTab() {
    if (!tabWidget || !tabWidget->currentWidget()) {
        adjustSize();
        return;
    }

    const QSize pageSize = tabWidget->currentWidget()->sizeHint()
        .expandedTo(tabWidget->currentWidget()->minimumSizeHint());
    const QSize tabBarSize = tabWidget->tabBar()->sizeHint();
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, tabWidget);

    const QSize tabWidgetSize(
        std::max(pageSize.width(), tabBarSize.width()) + (frameWidth * 2),
        pageSize.height() + tabBarSize.height() + (frameWidth * 2)
    );

    resize(tabWidgetSize.expandedTo(minimumSizeHint()));
}

void ControlPanel::addMonitorControl(const Monitor &monitor, bool readMonitorValues) {
    QWidget *box = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    QLabel *name = new QLabel(QString("[Display %1] %2").arg(monitor.displayNumber).arg(monitor.name));
    QFont monitorFont = name->font();
    monitorFont.setBold(true);
    name->setFont(monitorFont);
    layout->addWidget(name);

    for (const DdcFeature &feature : ControlFeatures) {
        addFeatureControl(layout, monitor, feature, readMonitorValues);
    }

    mainLayout->addWidget(box);
}

void ControlPanel::addFeatureControl(QVBoxLayout *layout, const Monitor &monitor, const DdcFeature &feature, bool readMonitorValues) {
    QWidget *row = new QWidget(this);
    QVBoxLayout *rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    QLabel *valueLabel = new QLabel(QString("%1: reading...").arg(feature.label), row);

    QSlider *slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 100);
    slider->setSingleStep(adjustmentStep());
    slider->setPageStep(adjustmentStep());
    slider->setMinimumWidth(220);

    const QString key = controlKey(monitor, feature.code);
    int maximum = 100;
    int current = currentValues.value(key, -1);
    if (readMonitorValues) {
        current = getFeatureValue(monitor, feature.code, &maximum);
        maximumValues[key] = maximum;
        if (current >= 0) {
            currentValues[key] = current;
        } else {
            currentValues.remove(key);
        }
    } else {
        maximum = maximumValues.value(key, 100);
    }

    if (current >= 0) {
        slider->setValue(current);
        valueLabel->setText(QString("%1: %2%").arg(feature.label).arg(current));
    } else {
        valueLabel->setText(QString("%1: unavailable").arg(feature.label));
        slider->setEnabled(false);
        slider->setValue(50);
    }

    rowLayout->addWidget(valueLabel);
    rowLayout->addWidget(slider);
    layout->addWidget(row);

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(TrayScrollDebounceMs);
    timers[key] = timer;

    connect(slider, &QSlider::valueChanged, this, [=](int value) {
        valueLabel->setText(QString("%1: %2%").arg(feature.label).arg(value));
        currentValues[key] = value;
        pendingValues[key] = value;
        timer->start();
    });

    connect(timer, &QTimer::timeout, this, [=]() {
        int value = pendingValues.value(key, slider->value());
        setFeatureValue(monitor, feature.code, value, maximumValues.value(key, 100));
        pendingValues.remove(key);
    });
}

void ControlPanel::addSyncedMonitorControl(bool readMonitorValues) {
    QWidget *box = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    QLabel *name = new QLabel("All Monitors");
    QFont monitorFont = name->font();
    monitorFont.setBold(true);
    name->setFont(monitorFont);
    layout->addWidget(name);

    for (const DdcFeature &feature : ControlFeatures) {
        addSyncedFeatureControl(layout, feature, readMonitorValues);
    }

    mainLayout->addWidget(box);
}

void ControlPanel::addSyncedFeatureControl(QVBoxLayout *layout, const DdcFeature &feature, bool readMonitorValues) {
    QWidget *row = new QWidget(this);
    QVBoxLayout *rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    QLabel *valueLabel = new QLabel(QString("%1: reading...").arg(feature.label), row);

    QSlider *slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 100);
    slider->setSingleStep(adjustmentStep());
    slider->setPageStep(adjustmentStep());
    slider->setMinimumWidth(220);

    const QString key = QString("all:%1").arg(feature.code);
    int total = 0;
    int count = 0;
    for (const Monitor &monitor : monitors) {
        int maximum = 100;
        const QString monitorKey = controlKey(monitor, feature.code);
        int current = currentValues.value(monitorKey, -1);
        if (readMonitorValues) {
            current = getFeatureValue(monitor, feature.code, &maximum);
            if (current >= 0) {
                maximumValues[monitorKey] = maximum;
                currentValues[monitorKey] = current;
            } else {
                currentValues.remove(monitorKey);
            }
        }

        if (current < 0) {
            continue;
        }

        total += current;
        ++count;
    }

    if (count > 0) {
        const int average = std::clamp(total / count, 0, 100);
        slider->setValue(average);
        valueLabel->setText(QString("%1: %2%").arg(feature.label).arg(average));
    } else {
        valueLabel->setText(QString("%1: unavailable").arg(feature.label));
        slider->setEnabled(false);
        slider->setValue(50);
    }

    rowLayout->addWidget(valueLabel);
    rowLayout->addWidget(slider);
    layout->addWidget(row);

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(TrayScrollDebounceMs);
    timers[key] = timer;

    connect(slider, &QSlider::valueChanged, this, [=](int value) {
        valueLabel->setText(QString("%1: %2%").arg(feature.label).arg(value));
        pendingValues[key] = value;
        for (const Monitor &monitor : monitors) {
            currentValues[controlKey(monitor, feature.code)] = value;
        }
        timer->start();
    });

    connect(timer, &QTimer::timeout, this, [=]() {
        const int value = pendingValues.value(key, slider->value());
        for (const Monitor &monitor : monitors) {
            const QString monitorKey = controlKey(monitor, feature.code);
            setFeatureValue(monitor, feature.code, value, maximumValues.value(monitorKey, 100));
        }
        pendingValues.remove(key);
    });
}

void ControlPanel::flushPendingValues() {
    if (pendingValues.isEmpty()) {
        return;
    }

    for (auto it = pendingValues.constBegin(); it != pendingValues.constEnd(); ++it) {
        const QStringList parts = it.key().split(':');
        if (parts.size() != 2) {
            continue;
        }

        bool featureOk = false;
        const int feature = parts.at(1).toInt(&featureOk);
        if (!featureOk) {
            continue;
        }

        const auto featureCode = static_cast<DDCA_Vcp_Feature_Code>(feature);
        const int value = it.value();
        if (parts.at(0) == "all") {
            for (const Monitor &monitor : monitors) {
                const QString monitorKey = controlKey(monitor, featureCode);
                setFeatureValue(monitor, featureCode, value, maximumValues.value(monitorKey, 100));
            }
            continue;
        }

        bool displayOk = false;
        const int displayNumber = parts.at(0).toInt(&displayOk);
        if (!displayOk) {
            continue;
        }

        for (const Monitor &monitor : monitors) {
            if (monitor.displayNumber != displayNumber) {
                continue;
            }

            setFeatureValue(monitor, featureCode, value, maximumValues.value(it.key(), 100));
            break;
        }
    }

    pendingValues.clear();
}
