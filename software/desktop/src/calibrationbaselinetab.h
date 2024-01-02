#ifndef CALIBRATIONBASELINETAB_H
#define CALIBRATIONBASELINETAB_H

#include <QWidget>

#include "densinterface.h"

class QLineEdit;

namespace Ui {
class CalibrationBaselineTab;
}

class CalibrationBaselineTab : public QWidget
{
    Q_OBJECT

public:
    explicit CalibrationBaselineTab(DensInterface *densInterface, QWidget *parent = nullptr);
    ~CalibrationBaselineTab();

    void reloadAll();

private slots:
    void onConnectionOpened();
    void onConnectionClosed();

    void onDensityReading(DensInterface::DensityType type, float dValue, float dZero, float rawValue, float corrValue);

    void onCalGetAllValues();
    void onCalLightSetClicked();
    void onCalGainCalClicked();
    void onCalGainSetClicked();
    void onCalSlopeSetClicked();
    void onCalReflectionSetClicked();
    void onCalTransmissionSetClicked();

    void onCalLightTextChanged();
    void onCalGainTextChanged();
    void onCalSlopeTextChanged();
    void onCalReflectionTextChanged();
    void onCalTransmissionTextChanged();

    void onCalLightResponse();
    void onCalGainResponse();
    void onCalSlopeResponse();
    void onCalReflectionResponse();
    void onCalTransmissionResponse();

    void onSlopeCalibrationTool();
    void onSlopeCalibrationToolFinished(int result);

private:
    void configureForDeviceType();
    void refreshButtonState();
    void updateLineEditDirtyState(QLineEdit *lineEdit, int value);
    void updateLineEditDirtyState(QLineEdit *lineEdit, float value, int prec);

    Ui::CalibrationBaselineTab *ui;
    DensInterface *densInterface_ = nullptr;
    DensInterface::DeviceType lastDeviceType_ = DensInterface::DeviceBaseline;
};

#endif // CALIBRATIONBASELINETAB_H
