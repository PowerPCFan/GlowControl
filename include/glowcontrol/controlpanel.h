#pragma once

#include "glowcontrol/ddc.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QMap>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVariant>
#include <QWidget>

class ControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);

    bool syncAllEnabled() const;
    bool osdEnabled() const;
    int adjustmentStep() const;
    void applyPendingChanges();
    void showControlsTab();

public slots:
    void loadMonitors();

private:
    QTabWidget *tabWidget = nullptr;
    QVBoxLayout *mainLayout;
    QCheckBox *syncAllCheckBox = nullptr;
    QCheckBox *osdCheckBox = nullptr;
    QSpinBox *adjustmentStepSpinBox = nullptr;
    bool syncAllMonitors = false;
    bool showOsd = true;
    int stepSize = 5;
    QList<Monitor> monitors;
    QMap<QString, QTimer*> timers;
    QMap<QString, int> currentValues;
    QMap<QString, int> pendingValues;
    QMap<QString, int> maximumValues;

    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    void addSettingsPage(QTabWidget *tabs);
    void updateSliderSteps();
    void reflowToCurrentTab();
    void loadSettings();
    void saveSetting(const char *key, const QVariant &value);

    void addMonitorControl(const Monitor &monitor, bool readMonitorValues);
    void addFeatureControl(QVBoxLayout *layout, const Monitor &monitor, const DdcFeature &feature, bool readMonitorValues);
    void addSyncedMonitorControl(bool readMonitorValues);
    void addSyncedFeatureControl(QVBoxLayout *layout, const DdcFeature &feature, bool readMonitorValues);
    void flushPendingValues();
    void rebuildControls(bool readMonitorValues);
};
