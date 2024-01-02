#ifndef CALIBRATIONTAB_H
#define CALIBRATIONTAB_H

#include <QWidget>

#include "densinterface.h"

class QLineEdit;

class CalibrationTab : public QWidget
{
    Q_OBJECT
public:
    explicit CalibrationTab(DensInterface *densInterface, QWidget *parent = nullptr);
    virtual ~CalibrationTab();

    virtual DensInterface::DeviceType deviceType() const = 0;
    virtual void clear() = 0;
    virtual void reloadAll() = 0;

protected:
    void updateLineEditDirtyState(QLineEdit *lineEdit, int value);
    void updateLineEditDirtyState(QLineEdit *lineEdit, float value, int prec);

    DensInterface *densInterface_ = nullptr;
};

#endif // CALIBRATIONTAB_H
