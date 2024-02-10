#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QMimeData>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QImage>
#include <QtGui/QValidator>
#include <QtGui/QStandardItemModel>
#include <QtGui/QClipboard>

#include "connectdialog.h"
#include "densinterface.h"
#include "diagnosticstab.h"
#include "calibrationbaselinetab.h"
#include "calibrationuvvistab.h"
#include "logwindow.h"
#include "settingsexporter.h"
#include "settingsimportdialog.h"
#include "floatitemdelegate.h"

namespace
{
static const int MEAS_TABLE_ROWS = 10;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , statusLabel_(new QLabel)
    , serialPort_(new QSerialPort(this))
    , densInterface_(new DensInterface(this))
    , logWindow_(new LogWindow(this))
{
    ui->setupUi(this);

    diagnosticsTab_ = new DiagnosticsTab(densInterface_);
    ui->tabDiagnosticsLayout->replaceWidget(ui->tabDiagnosticsWidget, diagnosticsTab_);

    calibrationTab_ = new CalibrationBaselineTab(densInterface_);
    ui->tabCalibrationLayout->replaceWidget(ui->tabCalibrationWidget, calibrationTab_);

    // Setup initial state of menu items
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionConfigure->setEnabled(true);
    ui->actionExit->setEnabled(true);

    ui->actionImportSettings->setEnabled(false);
    ui->actionExportSettings->setEnabled(false);

    ui->statusBar->addWidget(statusLabel_);

    ui->zeroIndicatorLabel->setPixmap(QPixmap());

    // Hide this menu item until we figure out what to use it for
    ui->actionConfigure->setVisible(false);

    // Setup menu shortcuts
    ui->actionCut->setShortcut(QKeySequence::Cut);
    ui->actionCopy->setShortcut(QKeySequence::Copy);
    ui->actionPaste->setShortcut(QKeySequence::Paste);
    ui->actionDelete->setShortcut(QKeySequence::Delete);
    ui->actionExit->setShortcut(QKeySequence::Quit);

    // Top-level UI signals
    connect(ui->menuEdit, &QMenu::aboutToShow, this, &MainWindow::onMenuEditAboutToShow);
    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::openConnection);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeConnection);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
    //connect(ui->actionConfigure, &QAction::triggered, settings_, &SettingsDialog::show);
    connect(ui->actionCut, &QAction::triggered, this, &MainWindow::onActionCut);
    connect(ui->actionCopy, &QAction::triggered, this, &MainWindow::onActionCopy);
    connect(ui->actionPaste, &QAction::triggered, this, &MainWindow::onActionPaste);
    connect(ui->actionDelete, &QAction::triggered, this, &MainWindow::onActionDelete);
    connect(ui->actionImportSettings, &QAction::triggered, this, &MainWindow::onImportSettings);
    connect(ui->actionExportSettings, &QAction::triggered, this, &MainWindow::onExportSettings);
    connect(ui->actionLogger, &QAction::triggered, this, &MainWindow::onLogger);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

    // Log window UI signals
    connect(logWindow_, &LogWindow::opened, this, &MainWindow::onLoggerOpened);
    connect(logWindow_, &LogWindow::closed, this, &MainWindow::onLoggerClosed);

    // Measurement UI signals
    connect(ui->addReadingPushButton, &QPushButton::clicked, this, &MainWindow::onAddReadingClicked);
    connect(ui->copyTablePushButton, &QPushButton::clicked, this, &MainWindow::onCopyTableClicked);
    connect(ui->clearTablePushButton, &QPushButton::clicked, this, &MainWindow::onClearTableClicked);

    // Densitometer interface update signals
    connect(densInterface_, &DensInterface::connectionOpened, this, &MainWindow::onConnectionOpened);
    connect(densInterface_, &DensInterface::connectionClosed, this, &MainWindow::onConnectionClosed);
    connect(densInterface_, &DensInterface::connectionError, this, &MainWindow::onConnectionError);
    connect(densInterface_, &DensInterface::densityReading, this, &MainWindow::onDensityReading);
    connect(densInterface_, &DensInterface::diagLogLine, logWindow_, &LogWindow::appendLogLine);

    // Loop back the set-complete signals to refresh their associated values
    connect(densInterface_, &DensInterface::calLightSetComplete, densInterface_, &DensInterface::sendGetCalLight);
    connect(densInterface_, &DensInterface::calGainSetComplete, densInterface_, &DensInterface::sendGetCalGain);
    connect(densInterface_, &DensInterface::calSlopeSetComplete, densInterface_, &DensInterface::sendGetCalSlope);
    connect(densInterface_, &DensInterface::calReflectionSetComplete, densInterface_, &DensInterface::sendGetCalReflection);
    connect(densInterface_, &DensInterface::calTransmissionSetComplete, densInterface_, &DensInterface::sendGetCalTransmission);

    // Setup the measurement model
    measModel_ = new QStandardItemModel(MEAS_TABLE_ROWS, 2, this);
    measModel_->setHorizontalHeaderLabels(QStringList() << tr("Mode") << tr("Measurement") << tr("Offset"));
    ui->measTableView->setModel(measModel_);
    ui->measTableView->setItemDelegateForColumn(1, new FloatItemDelegate(0.0, 5.0, 2));
    ui->measTableView->setItemDelegateForColumn(2, new FloatItemDelegate(0.0, 5.0, 2));
    ui->measTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->measTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->measTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    // Set the initial state of table items
    for (int row = 0; row < measModel_->rowCount(); row++) {
        // Non-editable mode item
        QStandardItem *item = new QStandardItem();
        item->setSelectable(false);
        item->setEditable(false);
        measModel_->setItem(row, 0, item);

        // Non-editable offset item
        item = new QStandardItem();
        item->setSelectable(false);
        item->setEditable(false);
        measModel_->setItem(row, 2, item);
    }

    QModelIndex index = measModel_->index(0, 1);
    ui->measTableView->setCurrentIndex(index);
    ui->measTableView->selectionModel()->clearSelection();

    ui->autoAddPushButton->setChecked(true);
    ui->addReadingPushButton->setEnabled(false);

    refreshButtonState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::connectToPort(const QString &portName)
{
    if (!portName.isEmpty()) {
        openConnectionToPort(portName);
    }
}

void MainWindow::openConnection()
{
    qDebug() << "Open connection";
    ConnectDialog *dialog = new ConnectDialog(this);
    connect(dialog, &QDialog::finished, this, &MainWindow::onOpenConnectionDialogFinished);
    dialog->setModal(true);
    dialog->show();
}

void MainWindow::onOpenConnectionDialogFinished(int result)
{
    ConnectDialog *dialog = dynamic_cast<ConnectDialog *>(sender());
    dialog->deleteLater();

    if (result == QDialog::Accepted) {
        const QString portName = dialog->portName();
        openConnectionToPort(portName);
    }
}

void MainWindow::openConnectionToPort(const QString &portName)
{
    qDebug() << "Connecting to:" << portName;
    serialPort_->setPortName(portName);
    serialPort_->setBaudRate(QSerialPort::Baud115200);
    serialPort_->setDataBits(QSerialPort::Data8);
    serialPort_->setParity(QSerialPort::NoParity);
    serialPort_->setStopBits(QSerialPort::OneStop);
    serialPort_->setFlowControl(QSerialPort::NoFlowControl);
    if (serialPort_->open(QIODevice::ReadWrite)) {
        serialPort_->setDataTerminalReady(true);
        if (densInterface_->connectToDevice(serialPort_)) {
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
            statusLabel_->setText(tr("Connected to %1").arg(portName));
        } else {
            serialPort_->close();
            statusLabel_->setText(tr("Unrecognized device"));
            QMessageBox::critical(this, tr("Error"), tr("Unrecognized device"));
        }
    } else {
        statusLabel_->setText(tr("Open error"));
        QMessageBox::critical(this, tr("Error"), serialPort_->errorString());
    }
}

void MainWindow::closeConnection()
{
    qDebug() << "Close connection";
    densInterface_->disconnectFromDevice();
    if (serialPort_->isOpen()) {
        serialPort_->close();
    }
    refreshButtonState();
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
}

void MainWindow::onImportSettings()
{
    QFileDialog fileDialog(this, tr("Load Device Settings"), QString(), tr("Settings Files (*.pds)"));
    fileDialog.setDefaultSuffix(".pds");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    if (fileDialog.exec() && !fileDialog.selectedFiles().isEmpty()) {
        QString filename = fileDialog.selectedFiles().constFirst();
        if (!filename.isEmpty()) {
            SettingsImportDialog importDialog;
            if (!importDialog.loadFile(filename)) {
                QMessageBox::warning(this, tr("Error"), tr("Unable to read settings file"));
                return;
            }
            if (importDialog.exec() == QDialog::Accepted) {
                QMessageBox messageBox;
                messageBox.setWindowTitle(tr("Send to Device"));
                messageBox.setText(tr("Replace the current device settings with the selected values?"));
                messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                messageBox.setDefaultButton(QMessageBox::Cancel);

                if (messageBox.exec() == QMessageBox::Ok) {
                    importDialog.sendSelectedSettings(densInterface_);
                    calibrationTab_->reloadAll();
                }
            }
        }
    }
}

void MainWindow::onExportSettings()
{
    SettingsExporter *exporter = new SettingsExporter(densInterface_, this);
    connect(exporter, &SettingsExporter::exportReady, this, [this, exporter]() {
        QFileDialog fileDialog(this, tr("Save Device Settings"), QString(), tr("Settings File (*.pds)"));
        fileDialog.setDefaultSuffix(".pds");
        fileDialog.setAcceptMode(QFileDialog::AcceptSave);
        if (fileDialog.exec() && !fileDialog.selectedFiles().isEmpty()) {
            QString filename = fileDialog.selectedFiles().constFirst();
            if (!filename.isEmpty()) {
                exporter->saveExport(filename);
            }
        }
        exporter->deleteLater();
    });
    connect(exporter, &SettingsExporter::exportFailed, this, [exporter]() {
        exporter->deleteLater();
    });
    exporter->prepareExport();
}

void MainWindow::onLogger(bool checked)
{
    if (checked) {
        logWindow_->show();
    } else {
        logWindow_->hide();
    }
}

void MainWindow::onLoggerOpened()
{
    qDebug() << "Log window opened";
    ui->actionLogger->setChecked(true);
    if (densInterface_->connected()) {
        densInterface_->sendSetDiagLoggingModeUsb();
    }
}

void MainWindow::onLoggerClosed()
{
    qDebug() << "Log window closed";
    ui->actionLogger->setChecked(false);
    if (densInterface_->connected()) {
        densInterface_->sendSetDiagLoggingModeDebug();
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About"),
                       tr("<b>%1 v%2</b><br>"
                          "<br>"
                          "Copyright 2022 Dektronics, Inc. All rights reserved.")
                       .arg(QApplication::applicationName(),
                            QApplication::applicationVersion()));
}

void MainWindow::refreshButtonState()
{
    const bool connected = densInterface_->connected();
    if (connected) {
        ui->actionImportSettings->setEnabled(true);
        ui->actionExportSettings->setEnabled(true);
    } else {
        ui->actionImportSettings->setEnabled(false);
        ui->actionExportSettings->setEnabled(false);
    }
}

void MainWindow::onMenuEditAboutToShow()
{
    bool hasCut = false;
    bool hasCopy = false;
    bool hasPaste = false;
    bool hasDelete = false;

    if (ui->tabWidget->currentWidget() == ui->tabMeasurement) {
        const QWidget *focusWidget = ui->tabMeasurement->focusWidget();
        if (focusWidget) {
            if (ui->readingValueLineEdit == focusWidget && ui->readingValueLineEdit->hasSelectedText()) {
                hasCopy = true;
            } else if (ui->measTableView == focusWidget || ui->measTableView->isAncestorOf(focusWidget)) {
                if (!ui->measTableView->selectionModel()->selectedRows(1).isEmpty()) {
                    // It is possible to have a table selection, and have focus
                    // or highlight elsewhere on the tab. However, making the table
                    // still handle edit actions under this situation has a lot of
                    // corner cases that would need to be handled. Probably easier
                    // to leave it alone for now.
                    hasCut = true;
                    hasCopy = true;
                    hasPaste = true;
                    hasDelete = true;
                }
            }
        }
    } else if (ui->tabWidget->currentWidget() == ui->tabCalibration) {
        const QWidget *focusWidget = ui->tabCalibration->focusWidget();
        if (focusWidget) {
            const QLineEdit *lineEdit = qobject_cast<const QLineEdit *>(focusWidget);
            if (lineEdit) {
                if (lineEdit->hasSelectedText()) {
                    hasCopy = true;
                    if (!lineEdit->isReadOnly()) {
                        hasCut = true;
                        hasPaste = true;
                        hasDelete = true;
                    }
                } else {
                    if (!lineEdit->isReadOnly()) {
                        hasPaste = true;
                    }
                }
            }
        }
    }

    // Only let paste stay enabled if the clipboard has content
    if (hasPaste) {
        const QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();
        if (!mimeData->hasText()) {
            hasPaste = false;
        }
    }

    ui->actionCut->setEnabled(hasCut);
    ui->actionCopy->setEnabled(hasCopy);
    ui->actionPaste->setEnabled(hasPaste);
    ui->actionDelete->setEnabled(hasDelete);
}

void MainWindow::onConnectionOpened()
{
    qDebug() << "Connection opened";

    if (calibrationTab_->deviceType() != densInterface_->deviceType()) {
        ui->tabCalibrationLayout->replaceWidget(calibrationTab_, ui->tabCalibrationWidget);
        calibrationTab_->deleteLater();
        calibrationTab_ = nullptr;

        if (densInterface_->deviceType() == DensInterface::DeviceBaseline) {
            calibrationTab_ = new CalibrationBaselineTab(densInterface_);
        } else if (densInterface_->deviceType() == DensInterface::DeviceUvVis) {
            calibrationTab_ = new CalibrationUvVisTab(densInterface_);
        }

        if (calibrationTab_) {
            ui->tabCalibrationLayout->replaceWidget(ui->tabCalibrationWidget, calibrationTab_);
            calibrationTab_->clear();
        }
    }

    densInterface_->sendSetMeasurementFormat(DensInterface::FormatExtended);
    densInterface_->sendSetAllowUncalibratedMeasurements(true);
    densInterface_->sendGetSystemBuild();
    densInterface_->sendGetSystemDeviceInfo();
    densInterface_->sendGetSystemUID();
    densInterface_->sendGetSystemInternalSensors();

    refreshButtonState();

    if (logWindow_->isVisible()) {
        densInterface_->sendSetDiagLoggingModeUsb();
    }
}

void MainWindow::onConnectionClosed()
{
    qDebug() << "Connection closed";
    refreshButtonState();
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);

    if (densInterface_->deviceUnrecognized()) {
        statusLabel_->setText(tr("Unrecognized device"));
        QMessageBox::critical(this, tr("Error"), tr("Unrecognized device"));
    } else {
        statusLabel_->setText(tr("Disconnected"));
    }
}

void MainWindow::onConnectionError()
{
    closeConnection();
}

void MainWindow::onDensityReading(DensInterface::DensityType type, float dValue, float dZero, float rawValue, float corrValue)
{
    Q_UNUSED(rawValue)
    Q_UNUSED(corrValue)

    // Update main tab contents
    if (type == DensInterface::DensityReflection) {
        ui->readingTypeLogoLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/reflection-icon.png")));
        ui->readingTypeNameLabel->setText(tr("Reflection"));
    } else {
        ui->readingTypeLogoLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/transmission-icon.png")));
        ui->readingTypeNameLabel->setText(tr("Transmission"));
    }

    if (!qIsNaN(dZero)) {
        ui->zeroIndicatorLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/zero-set-indicator.png")));
        float displayZero = dZero;
        if (qAbs(displayZero) < 0.01F) {
            displayZero = 0.0F;
        }
        ui->zeroIndicatorLabel->setToolTip(QString("%1D").arg(displayZero, 4, 'f', 2));
    } else {
        ui->zeroIndicatorLabel->setPixmap(QPixmap());
        ui->zeroIndicatorLabel->setToolTip(QString());
    }

    // Clean up the display value
    float displayValue;
    if (!qIsNaN(dZero)) {
        displayValue = dValue - dZero;
    } else {
        displayValue = dValue;
    }
    if (qAbs(displayValue) < 0.01F) {
        displayValue = 0.0F;
    }
    ui->readingValueLineEdit->setText(QString("%1D").arg(displayValue, 4, 'f', 2));

    // Save values so they can be referenced later
    lastReadingType_ = type;
    lastReadingDensity_ = displayValue;
    lastReadingOffset_ = dZero;
    ui->addReadingPushButton->setEnabled(true);

    // Update the measurement tab table view, if the tab is focused
    if (ui->tabWidget->currentWidget() == ui->tabMeasurement) {
        if (ui->autoAddPushButton->isChecked()) {
            onAddReadingClicked();
        }
    }
}

void MainWindow::onActionCut()
{
    QWidget *focusWidget = ui->tabWidget->currentWidget()->focusWidget();
    if (focusWidget) {
        // Handle the common case for a line edit widget
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(focusWidget);
        if (lineEdit && !lineEdit->isReadOnly()) {
            lineEdit->cut();
            return;
        }

        // Handle the case for a measurement table selection
        if (ui->tabWidget->currentWidget() == ui->tabMeasurement
                && focusWidget == ui->measTableView
                && !ui->measTableView->selectionModel()->selectedRows(1).isEmpty()) {
            measTableCut();
        }
    }
}

void MainWindow::onActionCopy()
{
    const QWidget *focusWidget = ui->tabWidget->currentWidget()->focusWidget();
    if (focusWidget) {
        // Handle the common case for a line edit widget
        const QLineEdit *lineEdit = qobject_cast<const QLineEdit *>(focusWidget);
        if (lineEdit) {
            lineEdit->copy();
            return;
        }

        // Handle the case for a measurement table selection
        if (ui->tabWidget->currentWidget() == ui->tabMeasurement
                && focusWidget == ui->measTableView
                && !ui->measTableView->selectionModel()->selectedRows(1).isEmpty()) {
            measTableCopy();
        }
    }
}

void MainWindow::onActionPaste()
{
    QWidget *focusWidget = ui->tabWidget->currentWidget()->focusWidget();
    if (focusWidget) {
        // Handle the common case for a line edit widget
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(focusWidget);
        if (lineEdit && !lineEdit->isReadOnly()) {
            lineEdit->paste();
            return;
        }

        // Handle the case for a measurement table selection
        if (ui->tabWidget->currentWidget() == ui->tabMeasurement
                && focusWidget == ui->measTableView
                && !ui->measTableView->selectionModel()->selectedRows(1).isEmpty()) {
            measTablePaste();
        }
    }
}

void MainWindow::onActionDelete()
{
    QWidget *focusWidget = ui->tabWidget->currentWidget()->focusWidget();
    if (focusWidget) {
        // Handle the common case for a line edit widget
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(focusWidget);
        if (lineEdit && !lineEdit->isReadOnly()) {
            lineEdit->del();
            return;
        }

        // Handle the case for a measurement table selection
        if (ui->tabWidget->currentWidget() == ui->tabMeasurement
                && focusWidget == ui->measTableView
                && !ui->measTableView->selectionModel()->selectedRows(1).isEmpty()) {
            measTableDelete();
        }
    }
}

void MainWindow::measTableAddReading(DensInterface::DensityType type, float density, float offset)
{

    QString numStr = QString("%1").arg(density, 4, 'f', 2);
    QString typeStr;
    QIcon typeIcon;
    QString offsetStr;

    if (type == DensInterface::DensityReflection) {
        typeIcon = QIcon(QString::fromUtf8(":/images/reflection-icon.png"));
        typeStr = QLatin1String("R");
    } else if (type == DensInterface::DensityTransmission) {
        typeIcon = QIcon(QString::fromUtf8(":/images/transmission-icon.png"));
        typeStr = QLatin1String("T");
    }

    if (!qIsNaN(offset)) {
        offsetStr = QString("%1").arg(offset, 4, 'f', 2);
    }

    int row = -1;
    QModelIndexList selected = ui->measTableView->selectionModel()->selectedIndexes();
    selected.append(ui->measTableView->selectionModel()->currentIndex());
    for (const QModelIndex &index : std::as_const(selected)) {
        if (row == -1 || index.row() < row) {
            row = index.row();
        }
    }
    ui->measTableView->selectionModel()->clearSelection();

    if (row >= 0) {
        QStandardItem *typeItem = new QStandardItem(typeIcon, typeStr);
        typeItem->setSelectable(false);
        typeItem->setEditable(false);
        measModel_->setItem(row, 0, typeItem);

        QStandardItem *measItem = new QStandardItem(numStr);
        measModel_->setItem(row, 1, measItem);

        QStandardItem *offsetItem = new QStandardItem(offsetStr);
        offsetItem->setSelectable(false);
        offsetItem->setEditable(false);
        measModel_->setItem(row, 2, offsetItem);

        if (row < measModel_->rowCount() - 1) {
            QModelIndex index = measModel_->index(row + 1, 1);
            ui->measTableView->setCurrentIndex(index);
        } else {
            measModel_->insertRow(row + 1);
            QModelIndex index = measModel_->index(row + 1, 1);
            ui->measTableView->setCurrentIndex(index);
        }
        ui->measTableView->scrollTo(ui->measTableView->currentIndex());
    }
}

void MainWindow::measTableCut()
{
    measTableCopy();
    measTableDelete();
}

void MainWindow::measTableCopy()
{
    QModelIndexList selected = ui->measTableView->selectionModel()->selectedRows(1);
    std::sort(selected.begin(), selected.end());
    measTableCopyList(selected, true);
}

void MainWindow::measTableCopyList(const QModelIndexList &indexList, bool includeEmpty)
{
    QVector<QString> numList;

    // Collect the list of populated measurement items in the table
    for (const QModelIndex &index : std::as_const(indexList)) {
        const QStandardItem *item = measModel_->itemFromIndex(index);
        if (item && item->column() == 1 && (includeEmpty || !item->text().isEmpty())) {
            numList.append(item->text());
        }
    }

    // Get the copy orientation
    bool horizCopy;
    if (ui->copyDirButtonGroup->checkedButton() == ui->horizCopyRadioButton) {
        horizCopy = true;
    } else {
        horizCopy = false;
    }

    // Build the string to put in the clipboard
    QString copiedText;
    for (const auto &numElement : numList) {
        if (!copiedText.isEmpty()) {
            if (horizCopy) {
                copiedText.append(QLatin1String("\t"));
            } else {
#if defined(Q_OS_WIN)
                copiedText.append(QLatin1String("\r\n"));
#else
                copiedText.append(QLatin1String("\n"));
#endif
            }
        }
        copiedText.append(numElement);
    }

    // Move to the clipboard
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(copiedText, QClipboard::Clipboard);

    if (clipboard->supportsSelection()) {
        clipboard->setText(copiedText, QClipboard::Selection);
    }

#if defined(Q_OS_UNIX)
    QThread::msleep(1);
#endif
}

void MainWindow::measTablePaste()
{
    static QRegularExpression re("\n|\r\n|\r|\t|[,;]\\s*|\\s+");

    // Capture and split the text to be pasted
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    QList<float> numList;
    if (mimeData->hasText()) {
        const QString text = mimeData->text();
        const QStringList elements = text.split(re, Qt::SkipEmptyParts);
        for (const QString& element : elements) {
            bool ok;
            float num = element.toFloat(&ok);
            if (ok) {
                numList.append(num);
            }
        }
    }

    // Add the pasted readings
    for (float num : numList) {
        measTableAddReading(DensInterface::DensityUnknown, num, qSNaN());
    }
}

void MainWindow::measTableDelete()
{
    QModelIndexList selected = ui->measTableView->selectionModel()->selectedRows(1);

    for (const QModelIndex &index : std::as_const(selected)) {
        QStandardItem *item = measModel_->item(index.row(), 0);
        if (item) {
            item->setText(QString());
            item->setIcon(QIcon());
        }

        item = measModel_->item(index.row(), 1);
        if (item) { item->setText(QString()); }

        item = measModel_->item(index.row(), 2);
        if (item) { item->setText(QString()); }
    }
}

void MainWindow::onAddReadingClicked()
{
    if (lastReadingType_ == DensInterface::DensityUnknown
            || qIsNaN(lastReadingDensity_)) {
        return;
    }

    measTableAddReading(lastReadingType_, lastReadingDensity_, lastReadingOffset_);
}

void MainWindow::onCopyTableClicked()
{
    // Build a list of all the items in the measurement column
    QModelIndexList indexList;
    for (int row = 0; row < measModel_->rowCount(); row++) {
        indexList.append(measModel_->index(row, 1));
    }

    // Call the common function for copying data from the list
    measTableCopyList(indexList, false);
}

void MainWindow::onClearTableClicked()
{
    if (measModel_->rowCount() > MEAS_TABLE_ROWS) {
        measModel_->removeRows(MEAS_TABLE_ROWS, measModel_->rowCount() - 10);
    }

    for (int row = 0; row < measModel_->rowCount(); row++) {
        QStandardItem *item = measModel_->item(row, 0);
        if (item) {
            item->setText(QString());
            item->setIcon(QIcon());
        }

        item = measModel_->item(row, 1);
        if (item) { item->setText(QString()); }

        item = measModel_->item(row, 2);
        if (item) { item->setText(QString()); }
    }

    QModelIndex index = measModel_->index(0, 1);
    ui->measTableView->setCurrentIndex(index);
    ui->measTableView->selectionModel()->clearSelection();
    ui->measTableView->scrollToTop();
}
