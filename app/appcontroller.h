#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include "core/communicationconfig.h"
#include "core/deviceconfig.h"
#include "models/faulttablemodel.h"
#include "models/monitortablemodel.h"
#include "models/parametertablemodel.h"

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVector>

class ModbusClient;
class QTimer;

/**
 * @brief QML 主界面控制器，负责第一阶段的型号加载、通讯连接和参数总表操作。
 * @author mozhengjie
 */
class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ParameterTableModel *parameterModel READ parameterModel CONSTANT)
    Q_PROPERTY(MonitorTableModel *monitorModel READ monitorModel CONSTANT)
    Q_PROPERTY(FaultTableModel *faultModel READ faultModel CONSTANT)
    Q_PROPERTY(QStringList modelNames READ modelNames NOTIFY modelNamesChanged)
    Q_PROPERTY(int currentModelIndex READ currentModelIndex WRITE setCurrentModelIndex NOTIFY currentModelIndexChanged)
    Q_PROPERTY(QStringList serialPortNames READ serialPortNames NOTIFY serialPortNamesChanged)
    Q_PROPERTY(QStringList communicationFormats READ communicationFormats CONSTANT)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY operationStatusChanged)
    Q_PROPERTY(QString selectedModelStatus READ selectedModelStatus NOTIFY selectedModelStatusChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY communicationConfigChanged)
    Q_PROPERTY(int slaveAddress READ slaveAddress NOTIFY communicationConfigChanged)
    Q_PROPERTY(int baudRate READ baudRate NOTIFY communicationConfigChanged)
    Q_PROPERTY(QString serialFormat READ serialFormat NOTIFY communicationConfigChanged)
    Q_PROPERTY(int responseTimeoutMs READ responseTimeoutMs NOTIFY communicationConfigChanged)
    Q_PROPERTY(int retryCount READ retryCount NOTIFY communicationConfigChanged)
    Q_PROPERTY(int progressValue READ progressValue NOTIFY progressChanged)
    Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(int monitorIntervalMs READ monitorIntervalMs WRITE setMonitorIntervalMs NOTIFY monitorIntervalMsChanged)
    Q_PROPERTY(bool monitorPollingActive READ monitorPollingActive NOTIFY monitorPollingActiveChanged)
    Q_PROPERTY(QString servoStateText READ servoStateText NOTIFY servoStateChanged)
    Q_PROPERTY(bool servoAlarmActive READ servoAlarmActive NOTIFY servoStateChanged)

public:
    /**
     * @brief 构造 QML 应用控制器并初始化模型、Modbus 客户端和 XML 型号列表。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit AppController(QObject *parent = nullptr);

    /**
     * @brief 获取参数总表模型。
     * @author mozhengjie
     * @return ParameterTableModel* 参数总表模型指针。
     */
    ParameterTableModel *parameterModel();

    /**
     * @brief 获取监控总表模型。
     * @author mozhengjie
     * @return MonitorTableModel* 监控总表模型指针。
     */
    MonitorTableModel *monitorModel();

    /**
     * @brief 获取故障总表模型。
     * @author mozhengjie
     * @return FaultTableModel* 故障总表模型指针。
     */
    FaultTableModel *faultModel();

    /**
     * @brief 获取可选择的型号名称列表。
     * @author mozhengjie
     * @return QStringList 型号名称列表。
     */
    QStringList modelNames() const;

    /**
     * @brief 获取当前型号索引。
     * @author mozhengjie
     * @return int 当前型号索引。
     */
    int currentModelIndex() const;

    /**
     * @brief 设置当前型号并刷新参数总表。
     * @author mozhengjie
     * @param index 型号索引，0 表示未选择。
     */
    void setCurrentModelIndex(int index);

    /**
     * @brief 获取当前可用串口名称。
     * @author mozhengjie
     * @return QStringList 串口名称列表。
     */
    QStringList serialPortNames() const;

    /**
     * @brief 获取支持的串口通信格式。
     * @author mozhengjie
     * @return QStringList 通信格式列表。
     */
    QStringList communicationFormats() const;

    QString connectionStatus() const;
    QString operationStatus() const;
    QString selectedModelStatus() const;
    bool isConnected() const;
    QString portName() const;
    int slaveAddress() const;
    int baudRate() const;
    QString serialFormat() const;
    int responseTimeoutMs() const;
    int retryCount() const;
    int progressValue() const;
    int progressMaximum() const;
    QString progressText() const;
    int monitorIntervalMs() const;
    bool monitorPollingActive() const;
    QString servoStateText() const;
    bool servoAlarmActive() const;

    /**
     * @brief 重新扫描本机串口并通知 QML 通讯设置弹窗刷新。
     * @author mozhengjie
     */
    Q_INVOKABLE void refreshSerialPorts();

    /**
     * @brief 保存 QML 通讯设置弹窗提交的 Modbus RTU 参数。
     * @author mozhengjie
     * @param portName 串口名称。
     * @param slaveAddress Modbus 从站地址。
     * @param baudRate 波特率。
     * @param format 通信格式，例如 8N1。
     * @param timeoutMs 响应超时时间。
     * @param retryCount 重试次数。
     */
    Q_INVOKABLE void setCommunicationSettings(const QString &portName,
                                              int slaveAddress,
                                              int baudRate,
                                              const QString &format,
                                              int timeoutMs,
                                              int retryCount);

    /**
     * @brief 根据当前状态连接或断开伺服。
     * @author mozhengjie
     */
    Q_INVOKABLE void toggleConnection();

    /**
     * @brief 上传 XML 中全部可读参数到调试软件。
     * @author mozhengjie
     */
    Q_INVOKABLE void uploadAllParameters();

    /**
     * @brief 上传勾选参数到调试软件。
     * @author mozhengjie
     */
    Q_INVOKABLE void uploadCheckedParameters();

    /**
     * @brief 将参数总表全部 RW 参数下载至伺服。
     * @author mozhengjie
     */
    Q_INVOKABLE void downloadAllParameters();

    /**
     * @brief 将参数总表勾选 RW 参数下载至伺服。
     * @author mozhengjie
     */
    Q_INVOKABLE void downloadCheckedParameters();

    /**
     * @brief 本地修改指定参数行但不下发。
     * @author mozhengjie
     * @param row 参数行号。
     * @param value 新参数值。
     */
    Q_INVOKABLE void editParameterLocal(int row, const QString &value);

    /**
     * @brief 回车确认指定参数行并立即下发。
     * @author mozhengjie
     * @param row 参数行号。
     * @param value 新参数值。
     */
    Q_INVOKABLE void submitParameterValue(int row, const QString &value);

    /**
     * @brief 设置参数行勾选状态。
     * @author mozhengjie
     * @param row 参数行号。
     * @param checked 是否勾选。
     */
    Q_INVOKABLE void setParameterChecked(int row, bool checked);

    /**
     * @brief 设置当前主页面索引，用于启动或停止故障表轮询。
     * @author mozhengjie
     * @param pageIndex 主页面索引。
     */
    Q_INVOKABLE void setActivePage(int pageIndex);

    /**
     * @brief 设置监控轮询间隔。
     * @author mozhengjie
     * @param intervalMs 轮询间隔，单位 ms。
     */
    Q_INVOKABLE void setMonitorIntervalMs(int intervalMs);

    /**
     * @brief 切换监控总表轮询启停状态。
     * @author mozhengjie
     */
    Q_INVOKABLE void toggleMonitorPolling();

    /**
     * @brief 设置监控总表行勾选状态。
     * @author mozhengjie
     * @param row 监控表行号。
     * @param checked 是否勾选。
     */
    Q_INVOKABLE void setMonitorChecked(int row, bool checked);

    /**
     * @brief 下发故障复位命令，写寄存器 45 为 1。
     * @author mozhengjie
     */
    Q_INVOKABLE void sendFaultResetCommand();

    /**
     * @brief 保存用户参数，写寄存器 90 为 1。
     * @author mozhengjie
     */
    Q_INVOKABLE void saveUserParameters();

    /**
     * @brief 保存电机参数，写寄存器 90 为 99。
     * @author mozhengjie
     */
    Q_INVOKABLE void saveMotorParameters();

    /**
     * @brief 校验伺服状态后执行恢复出厂命令。
     * @author mozhengjie
     */
    Q_INVOKABLE void requestFactoryResetCommand();

    /**
     * @brief 校验伺服状态后执行电机复位命令。
     * @author mozhengjie
     */
    Q_INVOKABLE void requestMotorResetCommand();

signals:
    void modelNamesChanged();
    void currentModelIndexChanged();
    void serialPortNamesChanged();
    void connectionStatusChanged();
    void operationStatusChanged();
    void selectedModelStatusChanged();
    void connectedChanged();
    void communicationConfigChanged();
    void progressChanged();
    void monitorIntervalMsChanged();
    void monitorPollingActiveChanged();
    void servoStateChanged();
    void toastRequested(const QString &message);

private:
    void scanXmlModelFiles();
    void loadCurrentDeviceConfig();
    void updateConnectionStatus(const QString &statusText);
    void updateOperationStatus(const QString &statusText);
    void updateSelectedModelStatus();
    void updateProgress(int finished, int total, const QString &prefix);
    void refreshMonitorTableFromConfig();
    void refreshFaultTableFromConfig();
    void startParameterUpload(bool checkedOnly, bool automatic = false);
    void startNextParameterUpload();
    void startParameterDownload(bool checkedOnly);
    void startNextParameterDownload();
    void handleConnectionStatusChanged(bool connected, const QString &statusText);
    void handleModbusError(const QString &errorText);
    void handleRegisterReadCompleted(int startAddress,
                                     const QVector<quint16> &values,
                                     bool success,
                                     const QString &message,
                                     const QString &requestTag);
    void handleRegisterWriteCompleted(int startAddress,
                                      bool success,
                                      const QString &message,
                                      const QString &requestTag);
    bool ensureModbusReady(const QString &operationName);
    bool writeParameterToServo(const RegisterDefinition &definition, const QString &newValue);
    bool writeControlRegister(int address, quint16 value, const QString &requestPrefix);
    bool requestServoStateCheck(const QString &requestPrefix);
    void startServoStatePolling();
    void stopServoStatePolling();
    void pollServoSystemState();
    void setServoStateDisplay(const QString &stateText, bool alarmActive);
    void refreshServoAlarmFaultText();
    void pollSelectedMonitors();
    void stopMonitorPolling();
    void startFaultPolling();
    void stopFaultPolling();
    void pollFaultRegisters();
    void requestFaultRegisters(const QString &requestPrefix);
    void updateFaultRegisterSnapshot(int startAddress, const QVector<quint16> &values);
    bool hasHighPriorityModbusWork() const;

    ParameterTableModel parameterModel_;
    MonitorTableModel monitorModel_;
    FaultTableModel faultModel_;
    ModbusClient *modbusClient_ = nullptr;
    QTimer *monitorTimer_ = nullptr;
    QTimer *servoStateTimer_ = nullptr;
    QTimer *faultPollTimer_ = nullptr;
    CommunicationConfig communicationConfig_;
    QVector<DeviceConfig> availableConfigs_;
    DeviceConfig currentConfig_;
    QStringList modelNames_;
    QStringList serialPortNames_;
    QString connectionStatus_;
    QString operationStatus_;
    QString selectedModelStatus_;
    QVector<RegisterDefinition> pendingParameterUploadQueue_;
    QVector<RegisterDefinition> pendingParameterDownloadQueue_;
    QHash<QString, RegisterDefinition> pendingParameterReadMap_;
    QHash<QString, MonitorDefinition> pendingMonitorReadMap_;
    QHash<QString, int> pendingFaultReadMap_;
    int currentModelIndex_ = 0;
    int progressValue_ = 0;
    int progressMaximum_ = 100;
    int parameterUploadTotal_ = 0;
    int parameterUploadFinished_ = 0;
    int parameterDownloadTotal_ = 0;
    int parameterDownloadFinished_ = 0;
    int modbusRequestSerial_ = 0;
    int monitorIntervalMs_ = 500;
    int activePageIndex_ = 0;
    bool connected_ = false;
    bool parameterUploadIsAutomatic_ = false;
    bool monitorPollingActive_ = false;
    bool servoAlarmActive_ = false;
    QString progressText_;
    QString servoStateText_;
};

#endif // APPCONTROLLER_H
