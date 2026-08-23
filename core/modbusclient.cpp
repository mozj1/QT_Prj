#include "modbusclient.h"

#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusRtuSerialClient>
#include <QVariant>

/**
 * @brief 构造 Modbus RTU 客户端。
 * @author mozhengjie
 * @param parent 父对象指针。
 */
ModbusClient::ModbusClient(QObject *parent)
    : QObject(parent)
    , client_(new QModbusRtuSerialClient(this))
{
    connect(client_, &QModbusRtuSerialClient::stateChanged, this, [this](QModbusDevice::State state) {
        if (state == QModbusDevice::ConnectedState) {
            emit connectionStatusChanged(true, QStringLiteral("已连接 %1").arg(config_.summaryText()));
        } else if (state == QModbusDevice::ConnectingState) {
            emit connectionStatusChanged(false, QStringLiteral("正在连接 %1").arg(config_.summaryText()));
        } else if (state == QModbusDevice::ClosingState) {
            emit connectionStatusChanged(false, QStringLiteral("正在断开"));
        } else {
            emit connectionStatusChanged(false, QStringLiteral("已断开"));
        }
    });

    connect(client_, &QModbusRtuSerialClient::errorOccurred, this, [this](QModbusDevice::Error error) {
        if (error != QModbusDevice::NoError) {
            emit errorOccurred(client_->errorString());
        }
    });
}

/**
 * @brief 析构 Modbus RTU 客户端并断开连接。
 * @author mozhengjie
 */
ModbusClient::~ModbusClient()
{
    closeDevice();
}

/**
 * @brief 按指定通信参数打开 Modbus 连接。
 * @author mozhengjie
 * @param config 通信参数配置。
 * @return bool 发起连接成功返回 true。
 */
bool ModbusClient::openDevice(const CommunicationConfig &config)
{
    if (config.portName.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("串口名称为空，请先完成通讯设置"));
        return false;
    }

    if (client_->state() != QModbusDevice::UnconnectedState) {
        client_->disconnectDevice();
    }

    config_ = config;
    client_->setConnectionParameter(QModbusDevice::SerialPortNameParameter, QVariant(config_.portName));
    client_->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, QVariant(config_.baudRate));
    client_->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QVariant::fromValue(config_.dataBits));
    client_->setConnectionParameter(QModbusDevice::SerialParityParameter, QVariant::fromValue(config_.parity));
    client_->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QVariant::fromValue(config_.stopBits));
    client_->setTimeout(config_.responseTimeoutMs);
    client_->setNumberOfRetries(config_.retryCount);

    emit connectionStatusChanged(false, QStringLiteral("正在连接 %1").arg(config_.summaryText()));
    if (!client_->connectDevice()) {
        emit errorOccurred(client_->errorString());
        return false;
    }
    return true;
}

/**
 * @brief 主动关闭 Modbus 连接。
 * @author mozhengjie
 */
void ModbusClient::closeDevice()
{
    if (client_ && client_->state() != QModbusDevice::UnconnectedState) {
        client_->disconnectDevice();
    }
}

/**
 * @brief 判断当前是否已连接。
 * @author mozhengjie
 * @return bool 已连接返回 true。
 */
bool ModbusClient::isConnected() const
{
    return client_ && client_->state() == QModbusDevice::ConnectedState;
}

/**
 * @brief 获取当前通信参数。
 * @author mozhengjie
 * @return CommunicationConfig 当前通信参数配置。
 */
CommunicationConfig ModbusClient::config() const
{
    return config_;
}

/**
 * @brief 写入保持寄存器。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param values 需要写入的 16 位寄存器值列表。
 * @return bool 请求成功发出返回 true。
 */
bool ModbusClient::writeHoldingRegisters(int startAddress, const QVector<quint16> &values)
{
    return writeHoldingRegisters(startAddress, values, QString());
}

/**
 * @brief 写入保持寄存器。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param values 需要写入的 16 位寄存器值列表。
 * @param requestTag 请求标识，用于上层区分业务来源。
 * @return bool 请求成功发出返回 true。
 */
bool ModbusClient::writeHoldingRegisters(int startAddress,
                                         const QVector<quint16> &values,
                                         const QString &requestTag)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("未连接伺服电机，无法下发参数"));
        return false;
    }
    if (startAddress < 0 || values.isEmpty()) {
        emit errorOccurred(QStringLiteral("寄存器地址或写入值无效"));
        return false;
    }

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, values.size());
    for (qsizetype index = 0; index < values.size(); ++index) {
        writeUnit.setValue(index, values.at(index));
    }

    QModbusReply *reply = client_->sendWriteRequest(writeUnit, config_.slaveAddress);
    if (!reply) {
        emit errorOccurred(client_->errorString());
        return false;
    }

    if (reply->isFinished()) {
        reply->deleteLater();
        emit writeCompleted(startAddress, true, QStringLiteral("广播写入请求已发送"), requestTag);
        return true;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply, startAddress, requestTag]() {
        const bool success = reply->error() == QModbusDevice::NoError;
        const QString message = success ? QStringLiteral("写入完成") : reply->errorString();
        emit writeCompleted(startAddress, success, message, requestTag);
        reply->deleteLater();
    });
    return true;
}

/**
 * @brief 读取保持寄存器。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param registerCount 读取寄存器数量。
 * @param requestTag 请求标识，用于上层区分业务来源。
 * @return bool 请求成功发出返回 true。
 */
bool ModbusClient::readHoldingRegisters(int startAddress, int registerCount, const QString &requestTag)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("未连接伺服电机，无法读取参数"));
        return false;
    }
    if (startAddress < 0 || registerCount <= 0) {
        emit errorOccurred(QStringLiteral("读取寄存器地址或数量无效"));
        return false;
    }

    const QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startAddress, registerCount);
    QModbusReply *reply = client_->sendReadRequest(readUnit, config_.slaveAddress);
    if (!reply) {
        emit errorOccurred(client_->errorString());
        return false;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply, startAddress, requestTag]() {
        QVector<quint16> values;
        const bool success = reply->error() == QModbusDevice::NoError;
        QString message = success ? QStringLiteral("读取完成") : reply->errorString();
        if (success) {
            const QModbusDataUnit resultUnit = reply->result();
            values.reserve(resultUnit.valueCount());
            for (uint index = 0; index < resultUnit.valueCount(); ++index) {
                values.append(resultUnit.value(index));
            }
        }
        emit readCompleted(startAddress, values, success, message, requestTag);
        reply->deleteLater();
    });
    return true;
}
