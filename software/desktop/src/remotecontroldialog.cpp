#include "remotecontroldialog.h"
#include "ui_remotecontroldialog.h"

#include <QDebug>

RemoteControlDialog::RemoteControlDialog(DensInterface *densInterface, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlDialog),
    densInterface_(densInterface),
    sensorStarted_(false),
    sensorConfigOnStart_(true)
{
    ui->setupUi(this);
    connect(densInterface_, &DensInterface::systemRemoteControl, this, &RemoteControlDialog::onSystemRemoteControl);
    connect(densInterface_, &DensInterface::diagLightReflChanged, this, &RemoteControlDialog::onDiagLightChanged);
    connect(densInterface_, &DensInterface::diagLightTranChanged, this, &RemoteControlDialog::onDiagLightChanged);
    connect(densInterface_, &DensInterface::diagLightTranUvChanged, this, &RemoteControlDialog::onDiagLightChanged);
    connect(densInterface_, &DensInterface::diagSensorInvoked, this, &RemoteControlDialog::onDiagSensorInvoked);
    connect(densInterface_, &DensInterface::diagSensorChanged, this, &RemoteControlDialog::onDiagSensorChanged);
    connect(densInterface_, &DensInterface::diagSensorBaselineGetReading, this, &RemoteControlDialog::onDiagSensorBaselineGetReading);
    connect(densInterface_, &DensInterface::diagSensorUvGetReading, this, &RemoteControlDialog::onDiagSensorUvGetReading);
    connect(densInterface_, &DensInterface::diagSensorBaselineInvokeReading, this, &RemoteControlDialog::onDiagSensorBaselineInvokeReading);
    connect(densInterface_, &DensInterface::diagSensorUvInvokeReading, this, &RemoteControlDialog::onDiagSensorUvInvokeReading);

    connect(ui->reflOffPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onReflOffClicked);
    connect(ui->reflOnPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onReflOnClicked);
    connect(ui->reflSetPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onReflSetClicked);
    connect(ui->reflSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RemoteControlDialog::onReflSpinBoxValueChanged);

    connect(ui->tranOffPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranOffClicked);
    connect(ui->tranOnPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranOnClicked);
    connect(ui->tranSetPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranSetClicked);
    connect(ui->tranSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RemoteControlDialog::onTranSpinBoxValueChanged);

    connect(ui->tranUvOffPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranUvOffClicked);
    connect(ui->tranUvOnPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranUvOnClicked);
    connect(ui->tranUvSetPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranUvSetClicked);
    connect(ui->tranUvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RemoteControlDialog::onTranUvSpinBoxValueChanged);

    connect(ui->sensorStartPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onSensorStartClicked);
    connect(ui->sensorStopPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onSensorStopClicked);
    connect(ui->modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onSensorModeIndexChanged);
    connect(ui->gainComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onSensorGainIndexChanged);
    connect(ui->intComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onSensorIntIndexChanged);
    connect(ui->agcCheckBox, &QCheckBox::stateChanged, this, &RemoteControlDialog::onAgcCheckBoxStateChanged);
    connect(ui->reflReadPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onReflReadClicked);
    connect(ui->tranReadPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranReadClicked);
    connect(ui->tranUvReadPushButton, &QPushButton::clicked, this, &RemoteControlDialog::onTranUvReadClicked);

    if (densInterface->deviceType() == DensInterface::DeviceUvVis) {
        ui->reflGroupBox->setTitle(tr("VIS Reflection Light"));
        ui->tranGroupBox->setTitle(tr("VIS Transmission Light"));
        ui->tranUvGroupBox->setVisible(true);

        ui->gainComboBox->clear();
        ui->gainComboBox->addItems(
            QStringList() << "0.5x" << "1x" << "2x" << "4x" << "8x"
                          << "16x" << "32x" << "64x" << "128x" << "256x");
        ui->gainComboBox->setCurrentIndex(8);

        ui->reflReadPushButton->setText(tr("VIS Reflection Read"));
        ui->tranReadPushButton->setText(tr("VIS Transmission Read"));
        ui->ch1Label->setVisible(false);
        ui->ch1LineEdit->setVisible(false);

    } else {
        ui->tranUvGroupBox->setVisible(false);
        ui->tranUvReadPushButton->setVisible(false);
        ui->modeLabel->setVisible(false);
        ui->modeComboBox->setVisible(false);
        ui->agcCheckBox->setVisible(false);
    }

    ledControlState(true);
    sensorControlState(true);
}

RemoteControlDialog::~RemoteControlDialog()
{
    delete ui;
}

void RemoteControlDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (densInterface_->connected()) {
        densInterface_->sendInvokeSystemRemoteControl(true);
    }
}

void RemoteControlDialog::closeEvent(QCloseEvent *event)
{
    if (densInterface_->connected()) {
        densInterface_->sendInvokeSystemRemoteControl(false);
    }
    QDialog::closeEvent(event);
}

void RemoteControlDialog::onSystemRemoteControl(bool enabled)
{
    qDebug() << "Remote control:" << enabled;
}

void RemoteControlDialog::onDiagLightChanged()
{
    ui->reflSpinBox->setStyleSheet("QSpinBox { background-color: lightgreen; }");
    ui->tranSpinBox->setStyleSheet("QSpinBox { background-color: lightgreen; }");
    ui->tranUvSpinBox->setStyleSheet("QSpinBox { background-color: lightgreen; }");
    ledControlState(true);
}

void RemoteControlDialog::onReflOffClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightRefl(0);
}

void RemoteControlDialog::onReflOnClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(128);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightRefl(128);
}

void RemoteControlDialog::onReflSetClicked()
{
    ledControlState(false);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightRefl(ui->reflSpinBox->value());
}

void RemoteControlDialog::onReflSpinBoxValueChanged(int value)
{
    Q_UNUSED(value)
    ui->reflSpinBox->setStyleSheet(styleSheet());
}

void RemoteControlDialog::onTranOffClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightTran(0);
}

void RemoteControlDialog::onTranOnClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(128);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightTran(128);
}

void RemoteControlDialog::onTranSetClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightTran(ui->tranSpinBox->value());
}

void RemoteControlDialog::onTranSpinBoxValueChanged(int value)
{
    Q_UNUSED(value)
    ui->tranSpinBox->setStyleSheet(styleSheet());
}

void RemoteControlDialog::onTranUvOffClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    densInterface_->sendSetDiagLightTranUv(0);
}

void RemoteControlDialog::onTranUvOnClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(128);
    densInterface_->sendSetDiagLightTranUv(128);
}

void RemoteControlDialog::onTranUvSetClicked()
{
    ledControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    densInterface_->sendSetDiagLightTranUv(ui->tranUvSpinBox->value());
}

void RemoteControlDialog::onTranUvSpinBoxValueChanged(int value)
{
    Q_UNUSED(value)
    ui->tranUvSpinBox->setStyleSheet(styleSheet());
}

void RemoteControlDialog::ledControlState(bool enabled)
{
    ui->reflOffPushButton->setEnabled(enabled);
    ui->reflOnPushButton->setEnabled(enabled);
    ui->reflSetPushButton->setEnabled(enabled);
    ui->reflSpinBox->setEnabled(enabled);

    ui->tranOffPushButton->setEnabled(enabled);
    ui->tranOnPushButton->setEnabled(enabled);
    ui->tranSetPushButton->setEnabled(enabled);
    ui->tranSpinBox->setEnabled(enabled);

    ui->tranUvOffPushButton->setEnabled(enabled);
    ui->tranUvOnPushButton->setEnabled(enabled);
    ui->tranUvSetPushButton->setEnabled(enabled);
    ui->tranUvSpinBox->setEnabled(enabled);
}

void RemoteControlDialog::onSensorStartClicked()
{
    sensorControlState(false);
    sensorStarted_ = true;
    if (sensorConfigOnStart_) {
        densInterface_->sendSetUvDiagSensorMode(ui->modeComboBox->currentIndex());
        sendSetDiagSensorConfig();
        sendSetDiagSensorAgc();
    }
    densInterface_->sendInvokeDiagSensorStart();
}

void RemoteControlDialog::sendSetDiagSensorConfig()
{
    if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
        densInterface_->sendSetUvDiagSensorConfig(
            ui->gainComboBox->currentIndex(),
            719, ((ui->intComboBox->currentIndex() + 1) * 100) - 1);
    } else {
        densInterface_->sendSetBaselineDiagSensorConfig(ui->gainComboBox->currentIndex(), ui->intComboBox->currentIndex());
    }
}

void RemoteControlDialog::sendSetDiagSensorAgc()
{
    if (ui->agcCheckBox->isChecked()) {
        // Currently using the same sample count as ALS measurements.
        // Perhaps this should be a separate setting, or something to
        // experiment with.
        densInterface_->sendSetUvDiagSensorAgcEnable(((ui->intComboBox->currentIndex() + 1) * 100) - 1);
    } else {
        densInterface_->sendSetUvDiagSensorAgcDisable();
    }
}

void RemoteControlDialog::onSensorStopClicked()
{
    sensorControlState(false);
    sensorStarted_ = false;
    densInterface_->sendInvokeDiagSensorStop();
}

void RemoteControlDialog::onSensorModeIndexChanged(int index)
{
    if (sensorStarted_) {
        sensorControlState(false);
        densInterface_->sendSetUvDiagSensorMode(index);
    } else {
        sensorConfigOnStart_ = true;
    }
}

void RemoteControlDialog::onSensorGainIndexChanged(int index)
{
    Q_UNUSED(index)
    if (sensorStarted_) {
        sensorControlState(false);
        sendSetDiagSensorConfig();
    } else {
        sensorConfigOnStart_ = true;
    }
}

void RemoteControlDialog::onSensorIntIndexChanged(int index)
{
    Q_UNUSED(index)
    if (sensorStarted_) {
        sensorControlState(false);
        sendSetDiagSensorConfig();
    } else {
        sensorConfigOnStart_ = true;
    }
}

void RemoteControlDialog::onAgcCheckBoxStateChanged(int state)
{
    Q_UNUSED(state)
    if (sensorStarted_) {
        sensorControlState(false);
        sendSetDiagSensorAgc();
    } else {
        sensorConfigOnStart_ = true;
    }
}

void RemoteControlDialog::onReflReadClicked()
{
    ledControlState(false);
    sensorControlState(false);
    ui->reflSpinBox->setValue(128);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    ui->ch0LineEdit->setEnabled(false);
    ui->ch1LineEdit->setEnabled(false);
    sendInvokeDiagRead(DensInterface::SensorLightReflection);
}

void RemoteControlDialog::onTranReadClicked()
{
    ledControlState(false);
    sensorControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(128);
    ui->tranUvSpinBox->setValue(0);
    ui->ch0LineEdit->setEnabled(false);
    ui->ch1LineEdit->setEnabled(false);
    sendInvokeDiagRead(DensInterface::SensorLightTransmission);
}

void RemoteControlDialog::onTranUvReadClicked()
{
    ledControlState(false);
    sensorControlState(false);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(128);
    ui->ch0LineEdit->setEnabled(false);
    ui->ch1LineEdit->setEnabled(false);

    sendInvokeDiagRead(DensInterface::SensorLightUvTransmission);
}

void RemoteControlDialog::sendInvokeDiagRead(DensInterface::SensorLight light)
{
    if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
        densInterface_->sendInvokeUvDiagRead(light,
                                             ui->modeComboBox->currentIndex(),
                                             ui->gainComboBox->currentIndex(),
                                             719, ((ui->intComboBox->currentIndex() + 1) * 100) - 1);
    } else {
        densInterface_->sendInvokeBaselineDiagRead(light,
                                                   ui->gainComboBox->currentIndex(),
                                                   ui->intComboBox->currentIndex());
    }
}

void RemoteControlDialog::sensorControlState(bool enabled)
{
    ui->sensorStartPushButton->setEnabled(enabled ? !sensorStarted_ : false);
    ui->sensorStopPushButton->setEnabled(enabled ? sensorStarted_ : false);

    if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
        if (ui->agcCheckBox->isChecked()) {
            ui->gainComboBox->setEnabled(!sensorStarted_);
        } else {
            ui->gainComboBox->setEnabled(enabled);
        }
    } else {
        ui->gainComboBox->setEnabled(enabled);
    }

    ui->intComboBox->setEnabled(enabled);
    ui->reflReadPushButton->setEnabled(enabled ? !sensorStarted_ : false);
    ui->tranReadPushButton->setEnabled(enabled ? !sensorStarted_ : false);
    ui->tranUvReadPushButton->setEnabled(enabled ? !sensorStarted_ : false);
}

void RemoteControlDialog::onDiagSensorInvoked()
{
    sensorControlState(true);
}

void RemoteControlDialog::onDiagSensorChanged()
{
    sensorControlState(true);
    sensorConfigOnStart_ = false;
}

void RemoteControlDialog::onDiagSensorBaselineGetReading(int ch0, int ch1)
{
    updateSensorReading(ch0, ch1);
    sensorControlState(true);
}

void RemoteControlDialog::onDiagSensorUvGetReading(unsigned int ch0, int gain, int sampleTime, int sampleCount)
{
    Q_UNUSED(sampleTime)
    Q_UNUSED(sampleCount)
    updateSensorReading(ch0, 0);

    if (ui->gainComboBox->currentIndex() != gain) {
        disconnect(ui->gainComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onSensorGainIndexChanged);
        ui->gainComboBox->setCurrentIndex(gain);
        connect(ui->gainComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onSensorGainIndexChanged);
    }

    sensorControlState(true);
}

void RemoteControlDialog::onDiagSensorBaselineInvokeReading(int ch0, int ch1)
{
    updateSensorReading(ch0, ch1);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    ui->ch0LineEdit->setEnabled(true);
    ui->ch1LineEdit->setEnabled(true);
    sensorControlState(true);
    ledControlState(true);
}

void RemoteControlDialog::onDiagSensorUvInvokeReading(unsigned int ch0)
{
    updateSensorReading(ch0, 0);
    ui->reflSpinBox->setValue(0);
    ui->tranSpinBox->setValue(0);
    ui->tranUvSpinBox->setValue(0);
    ui->ch0LineEdit->setEnabled(true);
    ui->ch1LineEdit->setEnabled(true);
    sensorControlState(true);
    ledControlState(true);
}

void RemoteControlDialog::updateSensorReading(int ch0, int ch1)
{
    ui->ch0LineEdit->setText(QString::number(ch0));
    ui->ch1LineEdit->setText(QString::number(ch1));
}
