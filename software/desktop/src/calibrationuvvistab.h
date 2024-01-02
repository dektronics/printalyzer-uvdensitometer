#ifndef CALIBRATIONUVVISTAB_H
#define CALIBRATIONUVVISTAB_H

#include <QWidget>

#include "calibrationtab.h"
#include "densinterface.h"

class QLineEdit;

namespace Ui {
class CalibrationUvVisTab;
}

class CalibrationUvVisTab : public CalibrationTab
{
    Q_OBJECT

public:
    explicit CalibrationUvVisTab(DensInterface *densInterface, QWidget *parent = nullptr);
    ~CalibrationUvVisTab();

    virtual DensInterface::DeviceType deviceType() const { return DensInterface::DeviceUvVis; }

    virtual void clear();

    virtual void reloadAll();

private slots:
    void onConnectionOpened();
    void onConnectionClosed();

    void onDensityReading(DensInterface::DensityType type, float dValue, float dZero, float rawValue, float corrValue);

    void onCalGetAllValues();
    void onCalGainCalClicked();
    void onCalGainSetClicked();
    void onCalSlopeSetClicked();
    void onCalReflectionSetClicked();
    void onCalTransmissionSetClicked();

    void onCalGainTextChanged();
    void onCalSlopeTextChanged();
    void onCalReflectionTextChanged();
    void onCalTransmissionTextChanged();

    void onCalGainResponse();
    void onCalSlopeResponse();
    void onCalReflectionResponse();
    void onCalTransmissionResponse();

    void onSlopeCalibrationTool();
    void onSlopeCalibrationToolFinished(int result);

private:
    void refreshButtonState();

    Ui::CalibrationUvVisTab *ui;
};

#endif // CALIBRATIONUVVISTAB_H
