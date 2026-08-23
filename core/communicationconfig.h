#ifndef COMMUNICATIONCONFIG_H
#define COMMUNICATIONCONFIG_H

#include <QSerialPort>
#include <QString>

/**
 * @brief Modbus RTU 通信参数配置。
 * @author mozhengjie
 */
struct CommunicationConfig
{
    QString portName;
    int slaveAddress = 1;
    int baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    int responseTimeoutMs = 1000;
    int retryCount = 3;

    /**
     * @brief 获取通信格式显示文本。
     * @author mozhengjie
     * @return QString 例如 8N1、8E1、8O1。
     */
    QString formatText() const
    {
        QString parityText = QStringLiteral("N");
        if (parity == QSerialPort::EvenParity) {
            parityText = QStringLiteral("E");
        } else if (parity == QSerialPort::OddParity) {
            parityText = QStringLiteral("O");
        }

        const int dataBitCount = dataBits == QSerialPort::Data7 ? 7 : 8;
        const int stopBitCount = stopBits == QSerialPort::TwoStop ? 2 : 1;
        return QStringLiteral("%1%2%3").arg(dataBitCount).arg(parityText).arg(stopBitCount);
    }

    /**
     * @brief 获取通信参数摘要文本。
     * @author mozhengjie
     * @return QString 串口、站号、波特率和通信格式摘要。
     */
    QString summaryText() const
    {
        return QStringLiteral("%1 地址 %2 %3bps %4")
            .arg(portName.isEmpty() ? QStringLiteral("未设置串口") : portName)
            .arg(slaveAddress)
            .arg(baudRate)
            .arg(formatText());
    }
};

#endif // COMMUNICATIONCONFIG_H
