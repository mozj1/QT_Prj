#include "communicationsettingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace {
/**
 * @brief Applies a serial-format display string to a communication configuration.
 * @author mozhengjie
 * @param formatText Format text such as 8N1, 8E1, 8O1, 8N2 or 7E1.
 * @param config Configuration object to update.
 */
void applyFormatText(const QString &formatText, CommunicationConfig *config)
{
    if (!config) {
        return;
    }

    if (formatText == QStringLiteral("7E1")) {
        config->dataBits = QSerialPort::Data7;
        config->parity = QSerialPort::EvenParity;
        config->stopBits = QSerialPort::OneStop;
    } else if (formatText == QStringLiteral("8E1")) {
        config->dataBits = QSerialPort::Data8;
        config->parity = QSerialPort::EvenParity;
        config->stopBits = QSerialPort::OneStop;
    } else if (formatText == QStringLiteral("8O1")) {
        config->dataBits = QSerialPort::Data8;
        config->parity = QSerialPort::OddParity;
        config->stopBits = QSerialPort::OneStop;
    } else if (formatText == QStringLiteral("8N2")) {
        config->dataBits = QSerialPort::Data8;
        config->parity = QSerialPort::NoParity;
        config->stopBits = QSerialPort::TwoStop;
    } else {
        config->dataBits = QSerialPort::Data8;
        config->parity = QSerialPort::NoParity;
        config->stopBits = QSerialPort::OneStop;
    }
}

/**
 * @brief Centers editable combo-box text when a line edit exists.
 * @author mozhengjie
 * @param combo Combo box to update.
 */
void centerComboText(QComboBox *combo)
{
    if (!combo || !combo->lineEdit()) {
        return;
    }

    combo->lineEdit()->setAlignment(Qt::AlignCenter);
}
} // namespace

/**
 * @brief Builds the communication settings dialog with current values as defaults.
 * @author mozhengjie
 * @param config Current communication configuration.
 * @param parent Parent widget.
 */
CommunicationSettingsDialog::CommunicationSettingsDialog(const CommunicationConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("通讯设置"));
    setModal(true);
    setMinimumWidth(360);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #FFFFFF; }"
        "QLabel { color: #000000; background: transparent; }"
        "QComboBox, QSpinBox { background: #FFFFFF; border: 1px solid #B8B8B8; padding: 4px 6px; color: #000000; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #000000; selection-background-color: #DDEAF7; }"
        "QPushButton { background: #F8F8F8; border: 1px solid #A8A8A8; padding: 5px 14px; color: #000000; }"
        "QPushButton:hover { background: #FFFFFF; }"
        "QPushButton:pressed { background: #E5E5E5; }"));

    auto *rootLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    portComboBox_ = new QComboBox;
    portComboBox_->setEditable(true);
    populateSerialPorts(config.portName);
    centerComboText(portComboBox_);
    formLayout->addRow(QStringLiteral("串口："), portComboBox_);

    slaveAddressSpinBox_ = new QSpinBox;
    slaveAddressSpinBox_->setRange(1, 247);
    slaveAddressSpinBox_->setAlignment(Qt::AlignCenter);
    slaveAddressSpinBox_->setValue(config.slaveAddress);
    formLayout->addRow(QStringLiteral("Modbus 地址："), slaveAddressSpinBox_);

    baudRateComboBox_ = new QComboBox;
    baudRateComboBox_->setEditable(true);
    populateBaudRates(config.baudRate);
    centerComboText(baudRateComboBox_);
    formLayout->addRow(QStringLiteral("波特率："), baudRateComboBox_);

    formatComboBox_ = new QComboBox;
    formatComboBox_->setEditable(true);
    populateFormats(config);
    if (formatComboBox_->lineEdit()) {
        formatComboBox_->lineEdit()->setReadOnly(true);
    }
    centerComboText(formatComboBox_);
    formLayout->addRow(QStringLiteral("通信格式："), formatComboBox_);

    timeoutSpinBox_ = new QSpinBox;
    timeoutSpinBox_->setRange(100, 10000);
    timeoutSpinBox_->setSingleStep(100);
    timeoutSpinBox_->setSuffix(QStringLiteral(" ms"));
    timeoutSpinBox_->setAlignment(Qt::AlignCenter);
    timeoutSpinBox_->setValue(config.responseTimeoutMs);
    formLayout->addRow(QStringLiteral("响应超时："), timeoutSpinBox_);

    retrySpinBox_ = new QSpinBox;
    retrySpinBox_->setRange(0, 10);
    retrySpinBox_->setAlignment(Qt::AlignCenter);
    retrySpinBox_->setValue(config.retryCount);
    formLayout->addRow(QStringLiteral("重试次数："), retrySpinBox_);

    rootLayout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttonBox);
}

/**
 * @brief Returns the communication configuration confirmed by the user.
 * @author mozhengjie
 * @return CommunicationConfig Confirmed communication configuration.
 */
CommunicationConfig CommunicationSettingsDialog::settings() const
{
    CommunicationConfig config;
    config.portName = portComboBox_->currentText().trimmed();
    config.slaveAddress = slaveAddressSpinBox_->value();
    config.baudRate = baudRateComboBox_->currentText().toInt();
    config.responseTimeoutMs = timeoutSpinBox_->value();
    config.retryCount = retrySpinBox_->value();
    applyFormatText(formatComboBox_->currentData().toString(), &config);
    return config;
}

/**
 * @brief Populates available local serial ports.
 * @author mozhengjie
 * @param currentPort Current port name.
 */
void CommunicationSettingsDialog::populateSerialPorts(const QString &currentPort)
{
    QStringList portNames;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : ports) {
        portNames.append(portInfo.portName());
    }

    portNames.removeDuplicates();
    if (!currentPort.isEmpty() && !portNames.contains(currentPort)) {
        portNames.prepend(currentPort);
    }

    portComboBox_->addItems(portNames);
    if (!currentPort.isEmpty()) {
        portComboBox_->setCurrentText(currentPort);
    }
}

/**
 * @brief Populates common baud-rate options.
 * @author mozhengjie
 * @param currentBaudRate Current baud rate.
 */
void CommunicationSettingsDialog::populateBaudRates(int currentBaudRate)
{
    const QList<int> baudRates = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (int baudRate : baudRates) {
        baudRateComboBox_->addItem(QString::number(baudRate));
    }
    baudRateComboBox_->setCurrentText(QString::number(currentBaudRate));
}

/**
 * @brief Populates supported serial-format options.
 * @author mozhengjie
 * @param config Current communication configuration.
 */
void CommunicationSettingsDialog::populateFormats(const CommunicationConfig &config)
{
    const QStringList formats = {QStringLiteral("8N1"),
                                 QStringLiteral("8E1"),
                                 QStringLiteral("8O1"),
                                 QStringLiteral("8N2"),
                                 QStringLiteral("7E1")};
    for (const QString &format : formats) {
        formatComboBox_->addItem(format, format);
    }

    const int currentIndex = formatComboBox_->findData(config.formatText());
    formatComboBox_->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
}
