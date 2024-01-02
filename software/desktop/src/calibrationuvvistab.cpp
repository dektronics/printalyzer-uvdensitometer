#include "calibrationuvvistab.h"
#include "ui_calibrationuvvistab.h"

#include <QtWidgets/QMessageBox>
#include <QDebug>

#include "gaincalibrationdialog.h"
#include "slopecalibrationdialog.h"
#include "util.h"

CalibrationUvVisTab::CalibrationUvVisTab(DensInterface *densInterface, QWidget *parent)
    : CalibrationTab(densInterface, parent)
    , ui(new Ui::CalibrationUvVisTab)
{
    ui->setupUi(this);

    // Densitometer interface update signals
    connect(densInterface_, &DensInterface::connectionOpened, this, &CalibrationUvVisTab::onConnectionOpened);
    connect(densInterface_, &DensInterface::connectionClosed, this, &CalibrationUvVisTab::onConnectionClosed);
    connect(densInterface_, &DensInterface::densityReading, this, &CalibrationUvVisTab::onDensityReading);
    connect(densInterface_, &DensInterface::calGainResponse, this, &CalibrationUvVisTab::onCalGainResponse);
    connect(densInterface_, &DensInterface::calSlopeResponse, this, &CalibrationUvVisTab::onCalSlopeResponse);
    connect(densInterface_, &DensInterface::calReflectionResponse, this, &CalibrationUvVisTab::onCalReflectionResponse);
    connect(densInterface_, &DensInterface::calTransmissionResponse, this, &CalibrationUvVisTab::onCalTransmissionResponse);

    // Calibration UI signals
    connect(ui->calGetAllPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalGetAllValues);
    connect(ui->gainCalPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalGainCalClicked);
    connect(ui->gainGetPushButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetCalGain);
    connect(ui->gainSetPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalGainSetClicked);
    connect(ui->slopeGetPushButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetCalSlope);
    connect(ui->slopeSetPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalSlopeSetClicked);
    connect(ui->reflGetPushButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetCalReflection);
    connect(ui->reflSetPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalReflectionSetClicked);
    connect(ui->tranGetPushButton, &QPushButton::clicked, densInterface_, &DensInterface::sendGetCalTransmission);
    connect(ui->tranSetPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onCalTransmissionSetClicked);
    connect(ui->slopeCalPushButton, &QPushButton::clicked, this, &CalibrationUvVisTab::onSlopeCalibrationTool);

    // Calibration (gain) field validation
    ui->med0LineEdit->setValidator(util::createFloatValidator(22.0, 27.0, 6, this));
    ui->med1LineEdit->setValidator(util::createFloatValidator(22.0, 27.0, 6, this));
    ui->high0LineEdit->setValidator(util::createFloatValidator(360.0, 440.0, 6, this));
    ui->high1LineEdit->setValidator(util::createFloatValidator(360.0, 440.0, 6, this));
    ui->max0LineEdit->setValidator(util::createFloatValidator(8500.0, 9900.0, 6, this));
    ui->max1LineEdit->setValidator(util::createFloatValidator(9100.0, 10700.0, 6, this));
    connect(ui->med0LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);
    connect(ui->med1LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);
    connect(ui->high0LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);
    connect(ui->high1LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);
    connect(ui->max0LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);
    connect(ui->max1LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalGainTextChanged);

    // Calibration (slope) field validation
    ui->zLineEdit->setValidator(util::createFloatValidator(-100.0, 100.0, 6, this));
    ui->b0LineEdit->setValidator(util::createFloatValidator(-100.0, 100.0, 6, this));
    ui->b1LineEdit->setValidator(util::createFloatValidator(-100.0, 100.0, 6, this));
    ui->b2LineEdit->setValidator(util::createFloatValidator(-100.0, 100.0, 6, this));
    connect(ui->zLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalSlopeTextChanged);
    connect(ui->b0LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalSlopeTextChanged);
    connect(ui->b1LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalSlopeTextChanged);
    connect(ui->b2LineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalSlopeTextChanged);

    // Calibration (reflection density) field validation
    ui->reflLoDensityLineEdit->setValidator(util::createFloatValidator(0.0, 2.5, 2, this));
    ui->reflLoReadingLineEdit->setValidator(util::createFloatValidator(0.0, 500.0, 6, this));
    ui->reflHiDensityLineEdit->setValidator(util::createFloatValidator(0.0, 2.5, 2, this));
    ui->reflHiReadingLineEdit->setValidator(util::createFloatValidator(0.0, 500.0, 6, this));
    connect(ui->reflLoDensityLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalReflectionTextChanged);
    connect(ui->reflLoReadingLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalReflectionTextChanged);
    connect(ui->reflHiDensityLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalReflectionTextChanged);
    connect(ui->reflHiReadingLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalReflectionTextChanged);

    // Calibration (transmission density) field validation
    ui->tranLoReadingLineEdit->setValidator(util::createFloatValidator(0.0, 500.0, 6, this));
    ui->tranHiDensityLineEdit->setValidator(util::createFloatValidator(0.0, 5.0, 2, this));
    ui->tranHiReadingLineEdit->setValidator(util::createFloatValidator(0.0, 500.0, 6, this));
    connect(ui->tranLoReadingLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalTransmissionTextChanged);
    connect(ui->tranHiDensityLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalTransmissionTextChanged);
    connect(ui->tranHiReadingLineEdit, &QLineEdit::textChanged, this, &CalibrationUvVisTab::onCalTransmissionTextChanged);

    refreshButtonState();
}

CalibrationUvVisTab::~CalibrationUvVisTab()
{
    delete ui;
}

void CalibrationUvVisTab::clear()
{
    ui->low0LineEdit->clear();
    ui->low1LineEdit->clear();
    ui->med0LineEdit->clear();
    ui->med1LineEdit->clear();
    ui->high0LineEdit->clear();
    ui->high1LineEdit->clear();
    ui->max0LineEdit->clear();
    ui->max1LineEdit->clear();

    ui->zLineEdit->clear();
    ui->b0LineEdit->clear();
    ui->b1LineEdit->clear();
    ui->b2LineEdit->clear();

    ui->reflLoDensityLineEdit->clear();
    ui->reflLoReadingLineEdit->clear();
    ui->reflHiDensityLineEdit->clear();
    ui->reflHiReadingLineEdit->clear();

    ui->tranLoDensityLineEdit->clear();
    ui->tranLoReadingLineEdit->clear();
    ui->tranHiDensityLineEdit->clear();
    ui->tranHiReadingLineEdit->clear();

    refreshButtonState();
}

void CalibrationUvVisTab::reloadAll()
{
    onCalGetAllValues();
}

void CalibrationUvVisTab::onConnectionOpened()
{
    // Clear the calibration page since values could have changed
    clear();
}

void CalibrationUvVisTab::onConnectionClosed()
{
    refreshButtonState();
}

void CalibrationUvVisTab::refreshButtonState()
{
    const bool connected = densInterface_->connected();
    if (connected) {
        ui->calGetAllPushButton->setEnabled(true);
        ui->gainCalPushButton->setEnabled(true);
        ui->gainGetPushButton->setEnabled(true);
        ui->slopeGetPushButton->setEnabled(true);
        ui->reflGetPushButton->setEnabled(true);
        ui->tranGetPushButton->setEnabled(true);

        // Populate read-only edit fields that are only set
        // via the protocol for consistency of the data formats
        if (ui->low0LineEdit->text().isEmpty()) {
            ui->low0LineEdit->setText("1");
        }
        if (ui->low1LineEdit->text().isEmpty()) {
            ui->low1LineEdit->setText("1");
        }
        if (ui->tranLoDensityLineEdit->text().isEmpty()) {
            ui->tranLoDensityLineEdit->setText("0.00");
        }

        ui->low0LineEdit->setEnabled(true);
        ui->low1LineEdit->setEnabled(true);
        ui->med0LineEdit->setEnabled(true);
        ui->med1LineEdit->setEnabled(true);
        ui->high0LineEdit->setEnabled(true);
        ui->high1LineEdit->setEnabled(true);
        ui->max0LineEdit->setEnabled(true);
        ui->max1LineEdit->setEnabled(true);

    } else {
        ui->calGetAllPushButton->setEnabled(false);
        ui->gainCalPushButton->setEnabled(false);
        ui->gainGetPushButton->setEnabled(false);
        ui->slopeGetPushButton->setEnabled(false);
        ui->reflGetPushButton->setEnabled(false);
        ui->tranGetPushButton->setEnabled(false);
    }

    // Make calibration values editable only if connected
    ui->med0LineEdit->setReadOnly(!connected);
    ui->med1LineEdit->setReadOnly(!connected);
    ui->high0LineEdit->setReadOnly(!connected);
    ui->high1LineEdit->setReadOnly(!connected);
    ui->max0LineEdit->setReadOnly(!connected);
    ui->max1LineEdit->setReadOnly(!connected);

    ui->zLineEdit->setReadOnly(!connected);
    ui->b0LineEdit->setReadOnly(!connected);
    ui->b1LineEdit->setReadOnly(!connected);
    ui->b2LineEdit->setReadOnly(!connected);

    ui->reflLoDensityLineEdit->setReadOnly(!connected);
    ui->reflLoReadingLineEdit->setReadOnly(!connected);
    ui->reflHiDensityLineEdit->setReadOnly(!connected);
    ui->reflHiReadingLineEdit->setReadOnly(!connected);

    ui->tranLoReadingLineEdit->setReadOnly(!connected);
    ui->tranHiDensityLineEdit->setReadOnly(!connected);
    ui->tranHiReadingLineEdit->setReadOnly(!connected);
}

void CalibrationUvVisTab::onDensityReading(DensInterface::DensityType type, float dValue, float dZero, float rawValue, float corrValue)
{
    Q_UNUSED(dValue)
    Q_UNUSED(dZero)
    Q_UNUSED(rawValue)

    // Update calibration tab fields, if focused
    if (type == DensInterface::DensityReflection) {
        if (ui->reflLoReadingLineEdit->hasFocus()) {
            ui->reflLoReadingLineEdit->setText(QString::number(corrValue, 'f'));
        } else if (ui->reflHiReadingLineEdit->hasFocus()) {
            ui->reflHiReadingLineEdit->setText(QString::number(corrValue, 'f'));
        }
    } else {
        if (ui->tranLoReadingLineEdit->hasFocus()) {
            ui->tranLoReadingLineEdit->setText(QString::number(corrValue, 'f'));
        } else if (ui->tranHiReadingLineEdit->hasFocus()) {
            ui->tranHiReadingLineEdit->setText(QString::number(corrValue, 'f'));
        }
    }
}

void CalibrationUvVisTab::onCalGetAllValues()
{
    densInterface_->sendGetCalGain();
    densInterface_->sendGetCalSlope();
    densInterface_->sendGetCalReflection();
    densInterface_->sendGetCalTransmission();
}

void CalibrationUvVisTab::onCalGainCalClicked()
{
    if (densInterface_->remoteControlEnabled()) {
        qWarning() << "Cannot start gain calibration while in remote mode";
        return;
    }
    ui->gainCalPushButton->setEnabled(false);

    QMessageBox messageBox;
    messageBox.setWindowTitle(tr("Sensor Gain Calibration"));
    messageBox.setText(tr("Hold the device firmly closed with no film in the optical path."));
    messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Ok);

    if (messageBox.exec() == QMessageBox::Ok) {
        GainCalibrationDialog dialog(densInterface_, this);
        dialog.exec();
        if (dialog.success()) {
            densInterface_->sendGetCalLight();
            densInterface_->sendGetCalGain();
        }
    }

    ui->gainCalPushButton->setEnabled(true);
}

void CalibrationUvVisTab::onCalGainSetClicked()
{
    DensCalGain calSlope;
    bool ok;

    calSlope.setLow0(1.0F);
    calSlope.setLow1(1.0F);

    calSlope.setMed0(ui->med0LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setMed1(ui->med1LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setHigh0(ui->high0LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setHigh1(ui->high1LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setMax0(ui->max0LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setMax1(ui->max1LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    densInterface_->sendSetCalGain(calSlope);
}

void CalibrationUvVisTab::onCalSlopeSetClicked()
{
    DensCalSlope calSlope;
    bool ok;

    calSlope.setZ(ui->zLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setB0(ui->b0LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setB1(ui->b1LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calSlope.setB2(ui->b2LineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    densInterface_->sendSetCalSlope(calSlope);
}

void CalibrationUvVisTab::onCalReflectionSetClicked()
{
    DensCalTarget calTarget;
    bool ok;

    calTarget.setLoDensity(ui->reflLoDensityLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calTarget.setLoReading(ui->reflLoReadingLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calTarget.setHiDensity(ui->reflHiDensityLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calTarget.setHiReading(ui->reflHiReadingLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    densInterface_->sendSetCalReflection(calTarget);
}

void CalibrationUvVisTab::onCalTransmissionSetClicked()
{
    DensCalTarget calTarget;
    bool ok;

    calTarget.setLoDensity(0.0F);

    calTarget.setLoReading(ui->tranLoReadingLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calTarget.setHiDensity(ui->tranHiDensityLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    calTarget.setHiReading(ui->tranHiReadingLineEdit->text().toFloat(&ok));
    if (!ok) { return; }

    densInterface_->sendSetCalTransmission(calTarget);
}

void CalibrationUvVisTab::onCalGainTextChanged()
{
    if (densInterface_->connected()
        && !ui->low0LineEdit->text().isEmpty()
        && !ui->low1LineEdit->text().isEmpty()
        && ui->med0LineEdit->hasAcceptableInput()
        && ui->med1LineEdit->hasAcceptableInput()
        && ui->high0LineEdit->hasAcceptableInput()
        && ui->high1LineEdit->hasAcceptableInput()
        && ui->max0LineEdit->hasAcceptableInput()
        && ui->max1LineEdit->hasAcceptableInput()) {
        ui->gainSetPushButton->setEnabled(true);
    } else {
        ui->gainSetPushButton->setEnabled(false);
    }

    const DensCalGain calGain = densInterface_->calGain();
    updateLineEditDirtyState(ui->med0LineEdit, calGain.med0(), 6);
    updateLineEditDirtyState(ui->med1LineEdit, calGain.med1(), 6);
    updateLineEditDirtyState(ui->high0LineEdit, calGain.high0(), 6);
    updateLineEditDirtyState(ui->high1LineEdit, calGain.high1(), 6);
    updateLineEditDirtyState(ui->max0LineEdit, calGain.max0(), 6);
    updateLineEditDirtyState(ui->max1LineEdit, calGain.max1(), 6);
}

void CalibrationUvVisTab::onCalSlopeTextChanged()
{
    const bool hasZ = (densInterface_->deviceType() == DensInterface::DeviceUvVis);
    if (densInterface_->connected()
        && (ui->zLineEdit->hasAcceptableInput() || !hasZ)
        && ui->b0LineEdit->hasAcceptableInput()
        && ui->b1LineEdit->hasAcceptableInput()
        && ui->b2LineEdit->hasAcceptableInput()) {
        ui->slopeSetPushButton->setEnabled(true);
    } else {
        ui->slopeSetPushButton->setEnabled(false);
    }

    const DensCalSlope calSlope = densInterface_->calSlope();
    updateLineEditDirtyState(ui->zLineEdit, calSlope.z(), 6);
    updateLineEditDirtyState(ui->b0LineEdit, calSlope.b0(), 6);
    updateLineEditDirtyState(ui->b1LineEdit, calSlope.b1(), 6);
    updateLineEditDirtyState(ui->b2LineEdit, calSlope.b2(), 6);
}

void CalibrationUvVisTab::onCalReflectionTextChanged()
{
    if (densInterface_->connected()
        && ui->reflLoDensityLineEdit->hasAcceptableInput()
        && ui->reflLoReadingLineEdit->hasAcceptableInput()
        && ui->reflHiDensityLineEdit->hasAcceptableInput()
        && ui->reflHiReadingLineEdit->hasAcceptableInput()) {
        ui->reflSetPushButton->setEnabled(true);
    } else {
        ui->reflSetPushButton->setEnabled(false);
    }

    const DensCalTarget calTarget = densInterface_->calReflection();
    updateLineEditDirtyState(ui->reflLoDensityLineEdit, calTarget.loDensity(), 2);
    updateLineEditDirtyState(ui->reflLoReadingLineEdit, calTarget.loReading(), 6);
    updateLineEditDirtyState(ui->reflHiDensityLineEdit, calTarget.hiDensity(), 2);
    updateLineEditDirtyState(ui->reflHiReadingLineEdit, calTarget.hiReading(), 6);
}

void CalibrationUvVisTab::onCalTransmissionTextChanged()
{
    if (densInterface_->connected()
        && !ui->tranLoDensityLineEdit->text().isEmpty()
        && ui->tranLoReadingLineEdit->hasAcceptableInput()
        && ui->tranHiDensityLineEdit->hasAcceptableInput()
        && ui->tranHiReadingLineEdit->hasAcceptableInput()) {
        ui->tranSetPushButton->setEnabled(true);
    } else {
        ui->tranSetPushButton->setEnabled(false);
    }

    const DensCalTarget calTarget = densInterface_->calTransmission();
    updateLineEditDirtyState(ui->tranLoReadingLineEdit, calTarget.loReading(), 6);
    updateLineEditDirtyState(ui->tranHiDensityLineEdit, calTarget.hiDensity(), 2);
    updateLineEditDirtyState(ui->tranHiReadingLineEdit, calTarget.hiReading(), 6);
}

void CalibrationUvVisTab::onCalGainResponse()
{
    const DensCalGain calGain = densInterface_->calGain();

    ui->low0LineEdit->setText(QString::number(calGain.low0(), 'f'));
    ui->low1LineEdit->setText(QString::number(calGain.low1(), 'f'));

    ui->med0LineEdit->setText(QString::number(calGain.med0(), 'f'));
    ui->med1LineEdit->setText(QString::number(calGain.med1(), 'f'));

    ui->high0LineEdit->setText(QString::number(calGain.high0(), 'f'));
    ui->high1LineEdit->setText(QString::number(calGain.high1(), 'f'));

    ui->max0LineEdit->setText(QString::number(calGain.max0(), 'f'));
    ui->max1LineEdit->setText(QString::number(calGain.max1(), 'f'));

    onCalGainTextChanged();
}

void CalibrationUvVisTab::onCalSlopeResponse()
{
    const DensCalSlope calSlope = densInterface_->calSlope();

    ui->zLineEdit->setText(QString::number(calSlope.z(), 'f'));
    ui->b0LineEdit->setText(QString::number(calSlope.b0(), 'f'));
    ui->b1LineEdit->setText(QString::number(calSlope.b1(), 'f'));
    ui->b2LineEdit->setText(QString::number(calSlope.b2(), 'f'));

    onCalSlopeTextChanged();
}

void CalibrationUvVisTab::onCalReflectionResponse()
{
    const DensCalTarget calReflection = densInterface_->calReflection();

    ui->reflLoDensityLineEdit->setText(QString::number(calReflection.loDensity(), 'f', 2));
    ui->reflLoReadingLineEdit->setText(QString::number(calReflection.loReading(), 'f', 6));
    ui->reflHiDensityLineEdit->setText(QString::number(calReflection.hiDensity(), 'f', 2));
    ui->reflHiReadingLineEdit->setText(QString::number(calReflection.hiReading(), 'f', 6));

    onCalReflectionTextChanged();
}

void CalibrationUvVisTab::onCalTransmissionResponse()
{
    const DensCalTarget calTransmission = densInterface_->calTransmission();

    ui->tranLoDensityLineEdit->setText(QString::number(calTransmission.loDensity(), 'f', 2));
    ui->tranLoReadingLineEdit->setText(QString::number(calTransmission.loReading(), 'f', 6));
    ui->tranHiDensityLineEdit->setText(QString::number(calTransmission.hiDensity(), 'f', 2));
    ui->tranHiReadingLineEdit->setText(QString::number(calTransmission.hiReading(), 'f', 6));

    onCalTransmissionTextChanged();
}

void CalibrationUvVisTab::onSlopeCalibrationTool()
{
    SlopeCalibrationDialog *dialog = new SlopeCalibrationDialog(densInterface_, this);
    connect(dialog, &QDialog::finished, this, &CalibrationUvVisTab::onSlopeCalibrationToolFinished);
    dialog->setCalculateZeroAdjustment(true);
    dialog->show();
}

void CalibrationUvVisTab::onSlopeCalibrationToolFinished(int result)
{
    SlopeCalibrationDialog *dialog = dynamic_cast<SlopeCalibrationDialog *>(sender());
    dialog->deleteLater();

    if (result == QDialog::Accepted) {
        if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
            ui->zLineEdit->setText(QString::number(dialog->zeroAdjustment(), 'f'));
        } else {
            ui->zLineEdit->setText(QString());
        }

        auto result = dialog->calValues();
        ui->b0LineEdit->setText(QString::number(std::get<0>(result), 'f'));
        ui->b1LineEdit->setText(QString::number(std::get<1>(result), 'f'));
        ui->b2LineEdit->setText(QString::number(std::get<2>(result), 'f'));
    }
}
