#include "glowcontrol/osdwindow.h"

#include <QBoxLayout>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

OsdWindow::OsdWindow(QWidget *parent)
    : QWidget(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(8);

    surface = new QWidget(this);
    surface->setStyleSheet(
        "QWidget { background: rgba(32, 36, 40, 230); border-radius: 10px; }"
        "QLabel { color: white; font-weight: 600; }"
        "QProgressBar { background: rgba(255, 255, 255, 40); border: 0; border-radius: 2px; height: 4px; }"
        "QProgressBar::chunk { background: white; border-radius: 2px; }"
    );

    surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(18, 14, 18, 14);
    surfaceLayout->setSpacing(8);

    titleLabel = new QLabel(surface);
    surfaceLayout->addWidget(titleLabel);
    layout->addWidget(surface);

    hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    hideTimer->setInterval(1200);
    connect(hideTimer, &QTimer::timeout, this, &QWidget::hide);
}

void OsdWindow::showValue(const QString &title, int value) {
    showValues(title, {{title, value}});
}

void OsdWindow::showValues(const QString &title, const QList<OsdValue> &values) {
    titleLabel->setText(title);
    clearRows();

    for (const OsdValue &value : values) {
        QString newLabel = value.label;
        qsizetype pos = newLabel.indexOf("] ");
        if (pos != -1) newLabel = newLabel.mid(pos + 2);
        addRow(newLabel, value.value);
    }

    adjustSize();
    move(osdPosition());
    show();
    raise();
    hideTimer->start();
}

QPoint OsdWindow::osdPosition() const {
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    if (!screen) {
        return QPoint(0, 0);
    }

    const QRect available = screen->availableGeometry();
    return QPoint(
        available.left() + (available.width() - width()) / 2,
        available.top() + available.height() / 8
    );
}

void OsdWindow::clearRows() {
    while (surfaceLayout->count() > 1) {
        QLayoutItem *item = surfaceLayout->takeAt(1);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void OsdWindow::addRow(const QString &label, int value) {
    QWidget *row = new QWidget(surface);
    QVBoxLayout *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    QLabel *labelWidget = new QLabel(QString("%1: %2%").arg(label).arg(std::clamp(value, 0, 100)), row);

    QProgressBar *progress = new QProgressBar(row);
    progress->setRange(0, 100);
    progress->setValue(std::clamp(value, 0, 100));
    progress->setTextVisible(false);
    progress->setFixedSize(220, 4);

    layout->addWidget(labelWidget);
    layout->addWidget(progress);
    surfaceLayout->addWidget(row);
}
