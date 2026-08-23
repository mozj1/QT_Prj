#ifndef COMMUNICATIONSETTINGSDIALOG_H
#define COMMUNICATIONSETTINGSDIALOG_H

#include "../core/communicationconfig.h"

#include <QDialog>

class QComboBox;
class QSpinBox;

/**
 * @brief 通讯设置对话框，采集 Modbus 地址、串口、波特率和通信格式。
 * @author mozhengjie
 */
class CommunicationSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造通讯设置对话框。
     * @author mozhengjie
     * @param config 当前通信参数。
     * @param parent 父窗口指针。
     */
    explicit CommunicationSettingsDialog(const CommunicationConfig &config, QWidget *parent = nullptr);

    /**
     * @brief 获取用户确认后的通信参数。
     * @author mozhengjie
     * @return CommunicationConfig 通信参数配置。
     */
    CommunicationConfig settings() const;

private:
    /**
     * @brief 刷新本机可用串口列表。
     * @author mozhengjie
     * @param currentPort 当前串口名。
     */
    void populateSerialPorts(const QString &currentPort);

    /**
     * @brief 刷新常用波特率选项。
     * @author mozhengjie
     * @param currentBaudRate 当前波特率。
     */
    void populateBaudRates(int currentBaudRate);

    /**
     * @brief 刷新通信格式选项。
     * @author mozhengjie
     * @param config 当前通信参数。
     */
    void populateFormats(const CommunicationConfig &config);

    QComboBox *portComboBox_ = nullptr;
    QSpinBox *slaveAddressSpinBox_ = nullptr;
    QComboBox *baudRateComboBox_ = nullptr;
    QComboBox *formatComboBox_ = nullptr;
    QSpinBox *timeoutSpinBox_ = nullptr;
    QSpinBox *retrySpinBox_ = nullptr;
};

#endif // COMMUNICATIONSETTINGSDIALOG_H
