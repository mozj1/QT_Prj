#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include "communicationconfig.h"

#include <QObject>
#include <QVector>

class QModbusRtuSerialClient;

/**
 * @brief Modbus RTU 客户端封装，负责连接状态和寄存器写入请求。
 * @author mozhengjie
 */
class ModbusClient final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造 Modbus RTU 客户端。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit ModbusClient(QObject *parent = nullptr);

    /**
     * @brief 析构 Modbus RTU 客户端并断开连接。
     * @author mozhengjie
     */
    ~ModbusClient() override;

    /**
     * @brief 按指定通信参数打开 Modbus 连接。
     * @author mozhengjie
     * @param config 通信参数配置。
     * @return bool 发起连接成功返回 true。
     */
    bool openDevice(const CommunicationConfig &config);

    /**
     * @brief 主动关闭 Modbus 连接。
     * @author mozhengjie
     */
    void closeDevice();

    /**
     * @brief 判断当前是否已连接。
     * @author mozhengjie
     * @return bool 已连接返回 true。
     */
    bool isConnected() const;

    /**
     * @brief 获取当前通信参数。
     * @author mozhengjie
     * @return CommunicationConfig 当前通信参数配置。
     */
    CommunicationConfig config() const;

    /**
     * @brief 写入保持寄存器。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param values 需要写入的 16 位寄存器值列表。
     * @return bool 请求成功发出返回 true。
     */
    bool writeHoldingRegisters(int startAddress, const QVector<quint16> &values);

    /**
     * @brief 写入保持寄存器。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param values 需要写入的 16 位寄存器值列表。
     * @param requestTag 请求标识，用于上层区分业务来源。
     * @return bool 请求成功发出返回 true。
     */
    bool writeHoldingRegisters(int startAddress, const QVector<quint16> &values, const QString &requestTag);

    /**
     * @brief 读取保持寄存器。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param registerCount 读取寄存器数量。
     * @param requestTag 请求标识，用于上层区分业务来源。
     * @return bool 请求成功发出返回 true。
     */
    bool readHoldingRegisters(int startAddress, int registerCount, const QString &requestTag = QString());

signals:
    /**
     * @brief 连接状态变化时发出。
     * @author mozhengjie
     * @param connected 已连接标志。
     * @param statusText 状态描述文本。
     */
    void connectionStatusChanged(bool connected, const QString &statusText);

    /**
     * @brief Modbus 错误发生时发出。
     * @author mozhengjie
     * @param errorText 错误描述文本。
     */
    void errorOccurred(const QString &errorText);

    /**
     * @brief 寄存器写入请求完成时发出。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param success 写入是否成功。
     * @param message 写入结果描述。
     */
    void writeCompleted(int startAddress, bool success, const QString &message, const QString &requestTag);

    /**
     * @brief 寄存器读取请求完成时发出。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param values 读取到的 16 位寄存器值。
     * @param success 读取是否成功。
     * @param message 读取结果描述。
     * @param requestTag 请求标识。
     */
    void readCompleted(int startAddress,
                       const QVector<quint16> &values,
                       bool success,
                       const QString &message,
                       const QString &requestTag);

private:
    QModbusRtuSerialClient *client_ = nullptr;
    CommunicationConfig config_;
};

#endif // MODBUSCLIENT_H
