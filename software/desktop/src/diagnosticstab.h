#ifndef DIAGNOSTICSTAB_H
#define DIAGNOSTICSTAB_H

#include <QWidget>

#include "densinterface.h"

namespace Ui {
class DiagnosticsTab;
}

class RemoteControlDialog;

class DiagnosticsTab : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticsTab(DensInterface *densInterface, QWidget *parent = nullptr);
    ~DiagnosticsTab();

    bool isRemoteOpen() const;

private slots:
    void onConnectionOpened();
    void onConnectionClosed();

    void onSystemVersionResponse();
    void onSystemBuildResponse();
    void onSystemDeviceResponse();
    void onSystemUniqueId();
    void onSystemInternalSensors();

    void onDiagDisplayScreenshot(const QByteArray &data);

    void onRemoteControl();
    void onRemoteControlFinished();

private:
    void configureForDeviceType();
    void refreshButtonState();

    Ui::DiagnosticsTab *ui;
    DensInterface *densInterface_ = nullptr;
    DensInterface::DeviceType lastDeviceType_ = DensInterface::DeviceBaseline;
    RemoteControlDialog *remoteDialog_ = nullptr;
};

#endif // DIAGNOSTICSTAB_H
