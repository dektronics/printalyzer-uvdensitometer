#ifndef REMOTECONTROLDIALOG_H
#define REMOTECONTROLDIALOG_H

#include <QDialog>
#include "densinterface.h"

namespace Ui {
class RemoteControlDialog;
}

class RemoteControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteControlDialog(DensInterface *densInterface, QWidget *parent = nullptr);
    ~RemoteControlDialog();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSystemRemoteControl(bool enabled);
    void onDiagLightChanged();
    void onDiagSensorInvoked();
    void onDiagSensorChanged();
    void onDiagSensorBaselineGetReading(int ch0, int ch1);
    void onDiagSensorUvGetReading(unsigned int ch0, int gain, int sampleTime, int sampleCount);
    void onDiagSensorBaselineInvokeReading(int ch0, int ch1);
    void onDiagSensorUvInvokeReading(unsigned int ch0);

    void onReflOffClicked();
    void onReflOnClicked();
    void onReflSetClicked();
    void onReflSpinBoxValueChanged(int value);

    void onTranOffClicked();
    void onTranOnClicked();
    void onTranSetClicked();
    void onTranSpinBoxValueChanged(int value);

    void onTranUvOffClicked();
    void onTranUvOnClicked();
    void onTranUvSetClicked();
    void onTranUvSpinBoxValueChanged(int value);

    void onSensorStartClicked();
    void onSensorStopClicked();
    void onSensorModeIndexChanged(int index);
    void onSensorGainIndexChanged(int index);
    void onSensorIntIndexChanged(int index);
    void onAgcCheckBoxStateChanged(int state);
    void onReflReadClicked();
    void onTranReadClicked();
    void onTranUvReadClicked();

private:
    void sendSetDiagSensorConfig();
    void sendSetDiagSensorAgc();
    void sendInvokeDiagRead(DensInterface::SensorLight light);
    void ledControlState(bool enabled);
    void sensorControlState(bool enabled);
    void updateSensorReading(int ch0, int ch1);

    Ui::RemoteControlDialog *ui;
    DensInterface *densInterface_;
    bool sensorStarted_;
    bool sensorConfigOnStart_;
};

#endif // REMOTECONTROLDIALOG_H
