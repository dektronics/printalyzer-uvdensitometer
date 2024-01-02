#include "calibrationtab.h"

#include <QtWidgets/QLineEdit>

CalibrationTab::CalibrationTab(DensInterface *densInterface, QWidget *parent)
    : QWidget{parent}
    , densInterface_(densInterface)
{
}

CalibrationTab::~CalibrationTab()
{
}

void CalibrationTab::updateLineEditDirtyState(QLineEdit *lineEdit, int value)
{
    if (!lineEdit) { return; }

    if (lineEdit->text().isNull() || lineEdit->text().isEmpty()
        || lineEdit->text() == QString::number(value)) {
        lineEdit->setStyleSheet(styleSheet());
    } else {
        lineEdit->setStyleSheet("QLineEdit { background-color: lightgoldenrodyellow; }");
    }
}

void CalibrationTab::updateLineEditDirtyState(QLineEdit *lineEdit, float value, int prec)
{
    if (!lineEdit) { return; }

    if (lineEdit->text().isNull() || lineEdit->text().isEmpty()
        || lineEdit->text() == QString::number(value, 'f', prec)) {
        lineEdit->setStyleSheet(styleSheet());
    } else {
        lineEdit->setStyleSheet("QLineEdit { background-color: lightgoldenrodyellow; }");
    }
}
