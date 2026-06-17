#pragma once

#include <QLabel>
#include <QList>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

struct OsdValue {
    QString label;
    int value = 0;
};

class OsdWindow : public QWidget {
    Q_OBJECT

public:
    explicit OsdWindow(QWidget *parent = nullptr);

    void showValue(const QString &title, int value);
    void showValues(const QString &title, const QList<OsdValue> &values);

private:
    QWidget *surface;
    QVBoxLayout *surfaceLayout;
    QLabel *titleLabel;
    QTimer *hideTimer;

    QPoint osdPosition() const;
    void clearRows();
    void addRow(const QString &label, int value);
};
