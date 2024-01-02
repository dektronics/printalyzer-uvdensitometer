#include "diagnosticstab.h"
#include "ui_diagnosticstab.h"

#include <QFileDialog>
#include <QDebug>

#include "densinterface.h"
#include "remotecontroldialog.h"

DiagnosticsTab::DiagnosticsTab(DensInterface *densInterface, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DiagnosticsTab)
    , densInterface_(densInterface)
{
    ui->setupUi(this);

    ui->refreshSensorsPushButton->setEnabled(false);
    ui->screenshotButton->setEnabled(false);

    // Diagnostics UI signals
    connect(ui->refreshSensorsPushButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetSystemInternalSensors);
    connect(ui->screenshotButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetDiagDisplayScreenshot);
    connect(ui->remotePushButton, &QPushButton::clicked, this, &DiagnosticsTab::onRemoteControl);

    // Densitometer interface update signals
    connect(densInterface_, &DensInterface::connectionOpened, this, &DiagnosticsTab::onConnectionOpened);
    connect(densInterface_, &DensInterface::connectionClosed, this, &DiagnosticsTab::onConnectionClosed);
    connect(densInterface_, &DensInterface::systemVersionResponse, this, &DiagnosticsTab::onSystemVersionResponse);
    connect(densInterface_, &DensInterface::systemBuildResponse, this, &DiagnosticsTab::onSystemBuildResponse);
    connect(densInterface_, &DensInterface::systemDeviceResponse, this, &DiagnosticsTab::onSystemDeviceResponse);
    connect(densInterface_, &DensInterface::systemUniqueId, this, &DiagnosticsTab::onSystemUniqueId);
    connect(densInterface_, &DensInterface::systemInternalSensors, this, &DiagnosticsTab::onSystemInternalSensors);
    connect(densInterface_, &DensInterface::diagDisplayScreenshot, this, &DiagnosticsTab::onDiagDisplayScreenshot);

    configureForDeviceType();

    // Initialize all fields with blank values
    onSystemVersionResponse();
    onSystemBuildResponse();
    onSystemDeviceResponse();
    onSystemUniqueId();
    onSystemInternalSensors();

    refreshButtonState();
}

DiagnosticsTab::~DiagnosticsTab()
{
    delete ui;
}

bool DiagnosticsTab::isRemoteOpen() const
{
    return remoteDialog_ != nullptr;
}

void DiagnosticsTab::onConnectionOpened()
{
    configureForDeviceType();

    refreshButtonState();
}

void DiagnosticsTab::onConnectionClosed()
{
    refreshButtonState();

    if (remoteDialog_) {
        remoteDialog_->close();
    }
}

void DiagnosticsTab::onSystemVersionResponse()
{
    if (densInterface_->projectName().isEmpty()) {
        ui->nameLabel->setText("Printalyzer Densitometer");
    } else {
        ui->nameLabel->setText(QString("<b>%1</b>").arg(densInterface_->projectName()));
    }
    ui->versionLabel->setText(tr("Version: %1").arg(densInterface_->version()));
}

void DiagnosticsTab::onSystemBuildResponse()
{
    ui->buildDateLabel->setText(tr("Date: %1").arg(densInterface_->buildDate().toString("yyyy-MM-dd hh:mm")));
    ui->buildDescribeLabel->setText(tr("Commit: %1").arg(densInterface_->buildDescribe()));
    if (densInterface_->buildChecksum() == 0) {
        ui->checksumLabel->setText(tr("Checksum: %1").arg(""));
    } else {
        ui->checksumLabel->setText(tr("Checksum: %1").arg(densInterface_->buildChecksum(), 0, 16));
    }
}

void DiagnosticsTab::onSystemDeviceResponse()
{
    ui->halVersionLabel->setText(tr("HAL Version: %1").arg(densInterface_->halVersion()));
    ui->mcuDevIdLabel->setText(tr("MCU Device ID: %1").arg(densInterface_->mcuDeviceId()));
    ui->mcuRevIdLabel->setText(tr("MCU Revision ID: %1").arg(densInterface_->mcuRevisionId()));
    ui->mcuSysClockLabel->setText(tr("MCU SysClock: %1").arg(densInterface_->mcuSysClock()));
}

void DiagnosticsTab::onSystemUniqueId()
{
    ui->uniqueIdLabel->setText(tr("UID: %1").arg(densInterface_->uniqueId()));
}

void DiagnosticsTab::onSystemInternalSensors()
{
    ui->mcuVddaLabel->setText(tr("Vdda: %1").arg(densInterface_->mcuVdda()));
    if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
        ui->mcuTempLabel->setText(tr("MCU Temperature: %1").arg(densInterface_->mcuTemp()));
        ui->sensorTempLabel->setText(tr("Sensor Temperature: %1").arg(densInterface_->sensorTemp()));
    } else {
        ui->mcuTempLabel->setText(tr("Temperature: %1").arg(densInterface_->mcuTemp()));
    }
}

void DiagnosticsTab::onDiagDisplayScreenshot(const QByteArray &data)
{
    qDebug() << "Got screenshot:" << data.size();
    QImage image = QImage::fromData(data, "XBM");
    if (!image.isNull()) {
        image = image.mirrored(true, true);
        image.invertPixels();

        QString fileName = QFileDialog::getSaveFileName(this, tr("Save Screenshot"),
                                                        "screenshot.png",
                                                        tr("Images (*.png *.jpg)"));
        if (!fileName.isEmpty()) {
            if (image.save(fileName)) {
                qDebug() << "Saved screenshot to:" << fileName;
            } else {
                qDebug() << "Error saving screenshot to:" << fileName;
            }
        }
    }
}

void DiagnosticsTab::onRemoteControl()
{
    if (!densInterface_->connected()) {
        return;
    }
    if (remoteDialog_) {
        remoteDialog_->setFocus();
        return;
    }
    remoteDialog_ = new RemoteControlDialog(densInterface_, this);
    connect(remoteDialog_, &QDialog::finished, this, &DiagnosticsTab::onRemoteControlFinished);
    remoteDialog_->show();
}

void DiagnosticsTab::onRemoteControlFinished()
{
    remoteDialog_->deleteLater();
    remoteDialog_ = nullptr;
}


void DiagnosticsTab::configureForDeviceType()
{
    DensInterface::DeviceType deviceType;
    if (densInterface_->connected()) {
        deviceType = densInterface_->deviceType();
        lastDeviceType_ = deviceType;
    } else {
        deviceType = lastDeviceType_;
    }

    if (deviceType == DensInterface::DeviceUvVis) {
        ui->sensorTempLabel->setVisible(true);
    } else {
        ui->sensorTempLabel->setVisible(false);
    }
}

void DiagnosticsTab::refreshButtonState()
{
    const bool connected = densInterface_->connected();
    if (connected) {
        ui->refreshSensorsPushButton->setEnabled(true);
        ui->screenshotButton->setEnabled(true);
        ui->remotePushButton->setEnabled(true);

    } else {
        ui->refreshSensorsPushButton->setEnabled(false);
        ui->screenshotButton->setEnabled(false);
        ui->remotePushButton->setEnabled(false);
    }
}
