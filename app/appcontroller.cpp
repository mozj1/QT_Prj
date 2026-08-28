#include "appcontroller.h"

#include "core/modbusclient.h"
#include "core/xmlconfigloader.h"

#include <algorithm>
#include <limits>

#include <QCoreApplication>
#include <QDir>
#include <QSerialPortInfo>
#include <QTimer>

namespace {
constexpr int kMonitorPageIndex = 1;
constexpr int kFaultPageIndex = 2;
constexpr int kFaultRegisterLow = 150;
constexpr int kFaultRegisterHigh = 390;
constexpr int kFaultPollIntervalMs = 1000;
constexpr int kPositionControlModeRegister = 0;
constexpr int kPositionCommandSourceRegister = 1;
constexpr int kPositionEnableRegister = 44;
constexpr int kPositionStepJogRegister = 59;
constexpr int kPositionJogSpeedRegister = 80;
constexpr int kPositionJogAccelerationRegister = 81;
constexpr int kPositionJogDecelerationRegister = 82;
constexpr int kPositionRunDistanceRegister = 83;
constexpr int kPositionWaitTimeRegister = 85;
constexpr int kPositionRepeatFlagRegister = 86;
constexpr int kPositionDirectionRegister = 87;
constexpr int kPositionRepeatCountRegister = 88;
constexpr int kPositionRunPauseRegister = 89;
constexpr int kPositionCurrentLowRegister = 449;
constexpr int kPositionCurrentRegisterCount = 2;
constexpr int kPositionCurrentPollIntervalMs = 100;

/**
 * @brief 将通信格式文本转换为串口数据位、校验位和停止位。
 * @author mozhengjie
 * @param formatText 通信格式文本。
 * @param config 待更新的通信参数。
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
 * @brief 解析单个 Modbus 地址，过滤 XML 中的地址范围。
 * @author mozhengjie
 * @param addressText 地址文本。
 * @param address 输出地址。
 * @return bool 解析成功返回 true。
 */
bool parseSingleAddress(const QString &addressText, int *address)
{
    if (!address || addressText.contains(QLatin1Char('-'))) {
        return false;
    }

    bool ok = false;
    const int parsedAddress = addressText.trimmed().toInt(&ok);
    if (!ok || parsedAddress < 0) {
        return false;
    }

    *address = parsedAddress;
    return true;
}

/**
 * @brief 判断寄存器定义是否能参与批量上传或下载。
 * @author mozhengjie
 * @param definition 参数定义。
 * @return bool 可传输返回 true。
 */
bool isTransferableRegister(const RegisterDefinition &definition)
{
    int address = 0;
    return parseSingleAddress(definition.modbusAddr, &address)
           && !definition.functionCn.contains(QStringLiteral("保留"))
           && !definition.functionEn.contains(QStringLiteral("reserve"), Qt::CaseInsensitive)
           && !definition.functionEn.contains(QStringLiteral("reserved"), Qt::CaseInsensitive);
}

/**
 * @brief 判断监控项是否属于故障总表 bit 定义。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @return bool 寄存器 150 或 390 的 bit 项返回 true。
 */
bool isFaultMonitorDefinition(const MonitorDefinition &definition)
{
    bool addressOk = false;
    const int address = definition.modbusAddr.trimmed().toInt(&addressOk);
    bool bitOk = false;
    const int bitOffset = definition.bitOffset.trimmed().toInt(&bitOk);
    return addressOk && bitOk
           && (address == kFaultRegisterLow || address == kFaultRegisterHigh)
           && bitOffset >= 0 && bitOffset < 16;
}

/**
 * @brief 将控制命令请求前缀转换为用户可读操作名。
 * @author mozhengjie
 * @param requestPrefix 请求标识前缀。
 * @return QString 用户可读操作名。
 */
QString commandOperationName(const QString &requestPrefix)
{
    if (requestPrefix == QStringLiteral("save-user") || requestPrefix == QStringLiteral("save-motor")) {
        return QStringLiteral("保存参数");
    }
    if (requestPrefix == QStringLiteral("fault-reset")) {
        return QStringLiteral("故障复位");
    }
    if (requestPrefix == QStringLiteral("factory-reset") || requestPrefix == QStringLiteral("factory-reset-check")) {
        return QStringLiteral("恢复出厂");
    }
    if (requestPrefix == QStringLiteral("motor-reset") || requestPrefix == QStringLiteral("motor-reset-check")) {
        return QStringLiteral("电机复位");
    }
    return requestPrefix;
}

/**
 * @brief 获取参数占用的 Modbus 保持寄存器数量。
 * @author mozhengjie
 * @param definition 参数定义。
 * @return int 保持寄存器数量。
 */
int registerCountForParameter(const RegisterDefinition &definition)
{
    return definition.remark.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0 ? 2 : 1;
}

/**
 * @brief 获取监控项占用的 Modbus 保持寄存器数量。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @return int 保持寄存器数量。
 */
int registerCountForMonitor(const MonitorDefinition &definition)
{
    const int inferredCount = definition.remark.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0 ? 2 : 1;
    bool ok = false;
    const int xmlCount = definition.readRegCount.trimmed().toInt(&ok);
    if (ok && xmlCount > 0) {
        return std::max(xmlCount, inferredCount);
    }
    return inferredCount;
}

/**
 * @brief 严格解析 double 文本。
 * @author mozhengjie
 * @param text 数值文本。
 * @param value 输出数值。
 * @return bool 解析成功返回 true。
 */
bool parseDoubleText(const QString &text, double *value)
{
    if (!value) {
        return false;
    }

    bool ok = false;
    const double parsedValue = text.trimmed().toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsedValue;
    return true;
}

/**
 * @brief 将用户输入值转换为 Modbus 写入寄存器列表。
 * @author mozhengjie
 * @param definition 参数定义。
 * @param newValue 输入参数值。
 * @param registers 输出寄存器列表。
 * @return bool 转换成功返回 true。
 */
bool convertParameterValueToRegisters(const RegisterDefinition &definition,
                                      const QString &newValue,
                                      QVector<quint16> *registers)
{
    if (!registers) {
        return false;
    }

    bool ok = false;
    const qint64 numericValue = newValue.trimmed().toLongLong(&ok);
    if (!ok) {
        return false;
    }

    registers->clear();
    if (definition.remark.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0) {
        if (numericValue < std::numeric_limits<qint32>::min()
            || numericValue > std::numeric_limits<quint32>::max()) {
            return false;
        }

        // 伺服 XML 中 int32 参数使用低字在前、高字在后的寄存器顺序。
        const quint32 rawValue = static_cast<quint32>(numericValue);
        registers->append(static_cast<quint16>(rawValue & 0xFFFFU));
        registers->append(static_cast<quint16>((rawValue >> 16U) & 0xFFFFU));
        return true;
    }

    if (numericValue < std::numeric_limits<qint16>::min()
        || numericValue > std::numeric_limits<quint16>::max()) {
        return false;
    }

    registers->append(static_cast<quint16>(numericValue & 0xFFFF));
    return true;
}

/**
 * @brief 将读取到的寄存器值转换为参数表显示文本。
 * @author mozhengjie
 * @param definition 参数定义。
 * @param values 寄存器值列表。
 * @return QString 参数显示文本。
 */
QString parameterValueFromRegisters(const RegisterDefinition &definition, const QVector<quint16> &values)
{
    if (values.isEmpty()) {
        return {};
    }

    double minimum = 0.0;
    const bool signedValue = parseDoubleText(definition.minimum, &minimum) && minimum < 0.0;
    if (registerCountForParameter(definition) == 2 && values.size() >= 2) {
        const quint32 rawValue = (static_cast<quint32>(values.at(1)) << 16U) | values.at(0);
        if (signedValue && rawValue > static_cast<quint32>(std::numeric_limits<qint32>::max())) {
            return QString::number(static_cast<qint32>(rawValue));
        }
        return QString::number(rawValue);
    }

    const quint16 rawValue = values.first();
    if (signedValue && rawValue > static_cast<quint16>(std::numeric_limits<qint16>::max())) {
        return QString::number(static_cast<qint16>(rawValue));
    }
    return QString::number(rawValue);
}

/**
 * @brief 将读取寄存器值转换为监控表显示值。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @param values 读取到的寄存器值。
 * @return QString 监控显示文本。
 */
QString monitorValueFromRegisters(const MonitorDefinition &definition, const QVector<quint16> &values)
{
    if (values.isEmpty()) {
        return {};
    }

    bool bitOk = false;
    const int bitOffset = definition.bitOffset.trimmed().toInt(&bitOk);
    if (bitOk && bitOffset >= 0 && bitOffset < 16) {
        return QString::number((values.first() >> bitOffset) & 0x1U);
    }

    if (registerCountForMonitor(definition) == 2 && values.size() >= 2) {
        const quint32 rawValue = (static_cast<quint32>(values.at(1)) << 16U) | values.at(0);
        return QString::number(rawValue);
    }
    return QString::number(values.first());
}

/**
 * @brief 将寄存器 187 的系统状态值转换为底部状态栏文本。
 * @author mozhengjie
 * @param stateValue 系统状态原始值。
 * @return QString 精简后的状态文本。
 */
QString servoSystemStateText(quint16 stateValue)
{
    switch (stateValue) {
    case 0x00:
        return QStringLiteral("上电初始化");
    case 0x01:
        return QStringLiteral("启动延时");
    case 0x02:
        return QStringLiteral("电流采样");
    case 0x03:
        return QStringLiteral("参数计算");
    case 0x04:
        return QStringLiteral("检查编码器");
    case 0x05:
        return QStringLiteral("伺服就绪");
    case 0x06:
        return QStringLiteral("重新运行");
    case 0x07:
        return QStringLiteral("伺服运行");
    case 0x08:
        return QStringLiteral("电流环测试");
    case 0x09:
        return QStringLiteral("开环运行");
    case 0x0A:
        return QStringLiteral("锁轴");
    case 0x0B:
        return QStringLiteral("编码器校准");
    case 0x0C:
        return QStringLiteral("编码器复位");
    case 0x0D:
        return QStringLiteral("编码器PWM测量");
    case 0x0E:
        return QStringLiteral("刹车");
    case 0x0F:
        return QStringLiteral("V/F控制");
    case 0x10:
        return QStringLiteral("保存编码器参数");
    case 0x11:
        return QStringLiteral("伺服报警");
    default:
        return QStringLiteral("未知状态 0x%1").arg(stateValue, 2, 16, QLatin1Char('0')).toUpper();
    }
}
} // namespace

/**
 * @brief 构造 QML 应用控制器并初始化模型、Modbus 客户端和 XML 型号列表。
 * @author mozhengjie
 * @param parent 父对象指针。
 */
AppController::AppController(QObject *parent)
    : QObject(parent)
    , parameterModel_(this)
    , monitorModel_(this)
    , faultModel_(this)
    , modbusClient_(new ModbusClient(this))
    , monitorTimer_(new QTimer(this))
    , servoStateTimer_(new QTimer(this))
    , faultPollTimer_(new QTimer(this))
    , positionCurrentTimer_(new QTimer(this))
    , connectionStatus_(QStringLiteral("连接状态：断开"))
    , operationStatus_(QStringLiteral("操作状态：空闲"))
    , selectedModelStatus_(QStringLiteral("当前型号：未选择"))
    , progressText_(QStringLiteral("参数进度：空闲"))
    , servoStateText_(QStringLiteral("伺服状态：未连接"))
{
    connect(modbusClient_, &ModbusClient::connectionStatusChanged,
            this, &AppController::handleConnectionStatusChanged);
    connect(modbusClient_, &ModbusClient::errorOccurred,
            this, &AppController::handleModbusError);
    connect(modbusClient_, &ModbusClient::readCompleted,
            this, &AppController::handleRegisterReadCompleted);
    connect(modbusClient_, &ModbusClient::writeCompleted,
            this, &AppController::handleRegisterWriteCompleted);
    connect(&parameterModel_, &ParameterTableModel::parameterValueChanged,
            this, &AppController::writeParameterToServo);

    monitorTimer_->setInterval(monitorIntervalMs_);
    connect(monitorTimer_, &QTimer::timeout, this, &AppController::pollSelectedMonitors);
    servoStateTimer_->setInterval(4000);
    connect(servoStateTimer_, &QTimer::timeout, this, &AppController::pollServoSystemState);
    faultPollTimer_->setInterval(kFaultPollIntervalMs);
    connect(faultPollTimer_, &QTimer::timeout, this, &AppController::pollFaultRegisters);
    positionCurrentTimer_->setInterval(kPositionCurrentPollIntervalMs);
    connect(positionCurrentTimer_, &QTimer::timeout, this, &AppController::pollPositionCurrentPosition);

    refreshSerialPorts();
    scanXmlModelFiles();
}

ParameterTableModel *AppController::parameterModel()
{
    return &parameterModel_;
}

MonitorTableModel *AppController::monitorModel()
{
    return &monitorModel_;
}

FaultTableModel *AppController::faultModel()
{
    return &faultModel_;
}

QStringList AppController::modelNames() const
{
    return modelNames_;
}

int AppController::currentModelIndex() const
{
    return currentModelIndex_;
}

void AppController::setCurrentModelIndex(int index)
{
    const int boundedIndex = index >= 0 && index < modelNames_.size() ? index : 0;
    if (currentModelIndex_ == boundedIndex) {
        return;
    }

    currentModelIndex_ = boundedIndex;
    emit currentModelIndexChanged();
    loadCurrentDeviceConfig();
}

QStringList AppController::serialPortNames() const
{
    return serialPortNames_;
}

QStringList AppController::communicationFormats() const
{
    return {QStringLiteral("8N1"), QStringLiteral("8E1"), QStringLiteral("8O1"),
            QStringLiteral("8N2"), QStringLiteral("7E1")};
}

QString AppController::connectionStatus() const
{
    return connectionStatus_;
}

QString AppController::operationStatus() const
{
    return operationStatus_;
}

QString AppController::selectedModelStatus() const
{
    return selectedModelStatus_;
}

bool AppController::isConnected() const
{
    return connected_;
}

QString AppController::portName() const
{
    return communicationConfig_.portName;
}

int AppController::slaveAddress() const
{
    return communicationConfig_.slaveAddress;
}

int AppController::baudRate() const
{
    return communicationConfig_.baudRate;
}

QString AppController::serialFormat() const
{
    return communicationConfig_.formatText();
}

int AppController::responseTimeoutMs() const
{
    return communicationConfig_.responseTimeoutMs;
}

int AppController::retryCount() const
{
    return communicationConfig_.retryCount;
}

int AppController::progressValue() const
{
    return progressValue_;
}

int AppController::progressMaximum() const
{
    return progressMaximum_;
}

QString AppController::progressText() const
{
    return progressText_;
}

int AppController::monitorIntervalMs() const
{
    return monitorIntervalMs_;
}

bool AppController::monitorPollingActive() const
{
    return monitorPollingActive_;
}

QString AppController::servoStateText() const
{
    return servoStateText_;
}

bool AppController::servoAlarmActive() const
{
    return servoAlarmActive_;
}

void AppController::refreshSerialPorts()
{
    QStringList portNames;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : ports) {
        portNames.append(portInfo.portName());
    }
    portNames.removeDuplicates();
    if (!communicationConfig_.portName.isEmpty() && !portNames.contains(communicationConfig_.portName)) {
        portNames.prepend(communicationConfig_.portName);
    }

    if (serialPortNames_ == portNames) {
        return;
    }
    serialPortNames_ = portNames;
    emit serialPortNamesChanged();
}

void AppController::setCommunicationSettings(const QString &portName,
                                             int slaveAddress,
                                             int baudRate,
                                             const QString &format,
                                             int timeoutMs,
                                             int retryCount)
{
    if (connected_) {
        emit toastRequested(QStringLiteral("请先断开连接，再修改通讯参数。"));
        return;
    }

    communicationConfig_.portName = portName.trimmed();
    communicationConfig_.slaveAddress = std::clamp(slaveAddress, 1, 247);
    communicationConfig_.baudRate = baudRate > 0 ? baudRate : 115200;
    communicationConfig_.responseTimeoutMs = std::clamp(timeoutMs, 100, 10000);
    communicationConfig_.retryCount = std::clamp(retryCount, 0, 10);
    applyFormatText(format, &communicationConfig_);
    emit communicationConfigChanged();

    refreshSerialPorts();
    updateConnectionStatus(QStringLiteral("连接状态：通讯参数已设置，%1").arg(communicationConfig_.summaryText()));
}

void AppController::toggleConnection()
{
    if (!modbusClient_) {
        return;
    }

    if (modbusClient_->isConnected()) {
        modbusClient_->closeDevice();
        return;
    }

    if (!currentConfig_.isValid()) {
        emit toastRequested(QStringLiteral("请先选择电机型号 XML。"));
        updateConnectionStatus(QStringLiteral("连接状态：连接失败，请先选择电机型号 XML"));
        return;
    }

    if (communicationConfig_.portName.trimmed().isEmpty()) {
        emit toastRequested(QStringLiteral("请先完成通讯设置。"));
        updateConnectionStatus(QStringLiteral("连接状态：请先完成通讯设置"));
        return;
    }

    modbusClient_->openDevice(communicationConfig_);
}

void AppController::uploadAllParameters()
{
    startParameterUpload(false);
}

void AppController::uploadCheckedParameters()
{
    startParameterUpload(true);
}

void AppController::downloadAllParameters()
{
    startParameterDownload(false);
}

void AppController::downloadCheckedParameters()
{
    startParameterDownload(true);
}

void AppController::editParameterLocal(int row, const QString &value)
{
    if (!parameterModel_.editLocalValue(row, value)) {
        updateOperationStatus(QStringLiteral("操作状态：参数本地修改无效"));
    }
}

void AppController::submitParameterValue(int row, const QString &value)
{
    if (!parameterModel_.submitValue(row, value)) {
        updateOperationStatus(QStringLiteral("操作状态：参数提交无效"));
    }
}

void AppController::setParameterChecked(int row, bool checked)
{
    parameterModel_.setRowChecked(row, checked);
}

void AppController::setActivePage(int pageIndex)
{
    if (activePageIndex_ == pageIndex) {
        if (pageIndex == kFaultPageIndex) {
            if (faultPollTimer_ && faultPollTimer_->isActive()) {
                stopFaultPolling();
                updateOperationStatus(QStringLiteral("操作状态：故障轮询已停止"));
            } else {
                startFaultPolling();
            }
        }
        return;
    }

    activePageIndex_ = pageIndex;
    if (pageIndex == kFaultPageIndex) {
        startFaultPolling();
        return;
    }

    stopFaultPolling();
}

void AppController::setMonitorIntervalMs(int intervalMs)
{
    const int boundedInterval = std::clamp(intervalMs, 10, 60000);
    if (monitorIntervalMs_ == boundedInterval) {
        return;
    }

    monitorIntervalMs_ = boundedInterval;
    if (monitorTimer_) {
        monitorTimer_->setInterval(monitorIntervalMs_);
    }
    emit monitorIntervalMsChanged();
}

void AppController::toggleMonitorPolling()
{
    if (monitorPollingActive_) {
        stopMonitorPolling();
        updateOperationStatus(QStringLiteral("操作状态：监控已停止"));
        return;
    }

    if (!ensureModbusReady(QStringLiteral("启动监控"))) {
        return;
    }

    if (monitorModel_.checkedMonitors().isEmpty()) {
        emit toastRequested(QStringLiteral("请先勾选需要监控的参数。"));
        updateOperationStatus(QStringLiteral("操作状态：未选择监控参数"));
        return;
    }

    monitorPollingActive_ = true;
    emit monitorPollingActiveChanged();
    monitorTimer_->start(monitorIntervalMs_);
    updateOperationStatus(QStringLiteral("操作状态：监控中，间隔 %1 ms").arg(monitorIntervalMs_));
    pollSelectedMonitors();
}

void AppController::setMonitorChecked(int row, bool checked)
{
    monitorModel_.setRowChecked(row, checked);
}

void AppController::sendFaultResetCommand()
{
    if (writeControlRegister(45, 1, QStringLiteral("fault-reset"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在执行故障复位"));
    }
}

void AppController::saveUserParameters()
{
    if (writeControlRegister(90, 1, QStringLiteral("save-user"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在保存用户参数"));
    }
}

void AppController::saveMotorParameters()
{
    if (writeControlRegister(90, 99, QStringLiteral("save-motor"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在保存电机参数"));
    }
}

void AppController::requestFactoryResetCommand()
{
    if (requestServoStateCheck(QStringLiteral("factory-reset-check"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在校验恢复出厂条件"));
    }
}

void AppController::requestMotorResetCommand()
{
    if (requestServoStateCheck(QStringLiteral("motor-reset-check"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在校验电机复位条件"));
    }
}

/**
 * @brief Enters position-run mode, backs up control source registers and initializes panel values.
 * @author mozhengjie
 */
void AppController::enterPositionRunMode()
{
    if (!ensureModbusReady(QStringLiteral("定位运行"))) {
        return;
    }

    if (positionRunActive_) {
        if (positionCurrentTimer_ && !positionCurrentTimer_->isActive()) {
            positionCurrentTimer_->start(kPositionCurrentPollIntervalMs);
        }
        pollPositionCurrentPosition();
        return;
    }

    positionRunActive_ = true;
    positionSetupApplied_ = false;
    positionCurrentPollingPaused_ = false;
    positionCurrentReadPending_ = false;
    positionCurrentZeroCaptured_ = false;
    positionCurrentZero_ = 0;
    positionModeBackups_.clear();
    pendingPositionReadMap_.clear();
    pendingPositionWriteMap_.clear();
    pendingPositionWriteValueMap_.clear();
    emit positionPollingPausedChanged(false);

    readPositionRegister(kPositionControlModeRegister, 1, QStringLiteral("position-backup"));
    readPositionRegister(kPositionCommandSourceRegister, 1, QStringLiteral("position-backup"));
    readPositionRegister(kPositionEnableRegister, 1, QStringLiteral("position-backup"));
    readPositionInitialRegisters();

    if (positionCurrentTimer_) {
        positionCurrentTimer_->start(kPositionCurrentPollIntervalMs);
    }
    pollPositionCurrentPosition();
    updateOperationStatus(QStringLiteral("操作状态：定位运行初始化中"));
}

/**
 * @brief Leaves position-run mode and restores the backed-up Pn0/Pn1/Pn44 values.
 * @author mozhengjie
 */
void AppController::leavePositionRunMode()
{
    if (!positionRunActive_) {
        return;
    }

    if (positionCurrentTimer_) {
        positionCurrentTimer_->stop();
    }
    positionRunActive_ = false;
    positionSetupApplied_ = false;
    positionCurrentPollingPaused_ = false;
    positionCurrentZeroCaptured_ = false;
    positionCurrentZero_ = 0;
    emit positionPollingPausedChanged(false);

    if (!modbusClient_ || !modbusClient_->isConnected()) {
        positionModeBackups_.clear();
        pendingPositionReadMap_.clear();
        pendingPositionWriteMap_.clear();
        pendingPositionWriteValueMap_.clear();
        positionCurrentReadPending_ = false;
        positionCurrentZeroCaptured_ = false;
        positionCurrentZero_ = 0;
        return;
    }

    writePositionRawRegister(kPositionStepJogRegister, 6, QStringLiteral("position-stepjog"));
    if (positionModeBackups_.contains(kPositionControlModeRegister)) {
        writePositionRawRegister(kPositionControlModeRegister,
                                 positionModeBackups_.value(kPositionControlModeRegister),
                                 QStringLiteral("position-restore"));
    }
    if (positionModeBackups_.contains(kPositionCommandSourceRegister)) {
        writePositionRawRegister(kPositionCommandSourceRegister,
                                 positionModeBackups_.value(kPositionCommandSourceRegister),
                                 QStringLiteral("position-restore"));
    }
    if (positionModeBackups_.contains(kPositionEnableRegister)) {
        writePositionRawRegister(kPositionEnableRegister,
                                 positionModeBackups_.value(kPositionEnableRegister),
                                 QStringLiteral("position-restore"));
    }
    positionModeBackups_.clear();
    updateOperationStatus(QStringLiteral("操作状态：定位运行已关闭，正在恢复控制模式"));
}

/**
 * @brief Toggles the 100ms current-position polling state.
 * @author mozhengjie
 */
void AppController::togglePositionCurrentPolling()
{
    if (!positionRunActive_) {
        return;
    }

    positionCurrentPollingPaused_ = !positionCurrentPollingPaused_;
    emit positionPollingPausedChanged(positionCurrentPollingPaused_);
    updateOperationStatus(positionCurrentPollingPaused_
                              ? QStringLiteral("操作状态：当前位置轮询已暂停")
                              : QStringLiteral("操作状态：当前位置轮询已恢复"));
    if (!positionCurrentPollingPaused_) {
        pollPositionCurrentPosition();
    }
}

/**
 * @brief Writes a position-run numeric input value to its mapped register.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param value Value to write.
 */
void AppController::writePositionRunRegister(int address, qint64 value)
{
    if (writePositionRawRegister(address, value, QStringLiteral("position-input"))) {
        updateOperationStatus(QStringLiteral("操作状态：正在下发定位寄存器 %1").arg(address));
    }
}

/**
 * @brief Writes the position-run enable register Pn44.
 * @author mozhengjie
 * @param enabled true writes 1, false writes 0.
 */
void AppController::writePositionEnable(bool enabled)
{
    if (writePositionRawRegister(kPositionEnableRegister, enabled ? 1 : 0, QStringLiteral("position-enable"))) {
        updateOperationStatus(enabled
                                  ? QStringLiteral("操作状态：正在使能定位运行")
                                  : QStringLiteral("操作状态：正在失能定位运行"));
    }
}

/**
 * @brief Writes the single/round-trip/continuous command pair to Pn88 and Pn86.
 * @author mozhengjie
 * @param modeIndex 0 single, 1 round-trip, 2 continuous.
 */
void AppController::writePositionCycleMode(int modeIndex)
{
    const int boundedMode = std::clamp(modeIndex, 0, 2);
    if (boundedMode == 0) {
        writePositionRawRegister(kPositionRepeatCountRegister, 1, QStringLiteral("position-cycle"));
        writePositionRawRegister(kPositionRepeatFlagRegister, 0, QStringLiteral("position-cycle"));
        updateOperationStatus(QStringLiteral("操作状态：定位运行已切换为单次"));
        return;
    }

    writePositionRawRegister(kPositionRepeatCountRegister, 50000, QStringLiteral("position-cycle"));
    writePositionRawRegister(kPositionRepeatFlagRegister, boundedMode == 2 ? 1 : 0, QStringLiteral("position-cycle"));
    updateOperationStatus(boundedMode == 2
                              ? QStringLiteral("操作状态：定位运行已切换为连续")
                              : QStringLiteral("操作状态：定位运行已切换为往返"));
}

/**
 * @brief Writes the position direction register Pn87.
 * @author mozhengjie
 * @param reverse true writes reverse, false writes forward.
 */
void AppController::writePositionDirection(bool reverse)
{
    if (writePositionRawRegister(kPositionDirectionRegister, reverse ? 1 : 0, QStringLiteral("position-direction"))) {
        updateOperationStatus(reverse
                                  ? QStringLiteral("操作状态：定位方向已切换为反向")
                                  : QStringLiteral("操作状态：定位方向已切换为正向"));
    }
}

/**
 * @brief Writes the position run/pause register Pn89.
 * @author mozhengjie
 * @param paused true writes pause, false writes run.
 */
void AppController::writePositionPause(bool paused)
{
    if (writePositionRawRegister(kPositionRunPauseRegister, paused ? 1 : 0, QStringLiteral("position-runpause"))) {
        updateOperationStatus(paused
                                  ? QStringLiteral("操作状态：定位运行已暂停")
                                  : QStringLiteral("操作状态：定位运行已启动"));
    }
}

/**
 * @brief Writes the step1 jog command register Pn59.
 * @author mozhengjie
 * @param commandValue 3 reverse, 4 forward, 6 stop.
 */
void AppController::writePositionStepJogCommand(int commandValue)
{
    const int boundedValue = (commandValue == 3 || commandValue == 4) ? commandValue : 6;
    if (writePositionRawRegister(kPositionStepJogRegister, boundedValue, QStringLiteral("position-stepjog"))) {
        if (boundedValue == 4) {
            updateOperationStatus(QStringLiteral("操作状态：step1 正向点动中"));
        } else if (boundedValue == 3) {
            updateOperationStatus(QStringLiteral("操作状态：step1 反向点动中"));
        } else {
            updateOperationStatus(QStringLiteral("操作状态：step1 点动已松开"));
        }
    }
}

void AppController::scanXmlModelFiles()
{
    const QStringList xmlDirectories = {
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("XML")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../XML")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../../XML")),
        QDir::current().absoluteFilePath(QStringLiteral("XML")),
        QDir(QStringLiteral(APP_SOURCE_DIR)).absoluteFilePath(QStringLiteral("XML"))};

    availableConfigs_.clear();
    for (const QString &directoryPath : xmlDirectories) {
        availableConfigs_ = XmlConfigLoader::scanDirectory(directoryPath);
        if (!availableConfigs_.isEmpty()) {
            break;
        }
    }

    modelNames_.clear();
    modelNames_.append(QStringLiteral("请选择型号 XML"));
    for (const DeviceConfig &config : availableConfigs_) {
        modelNames_.append(config.productSeries.isEmpty()
                               ? config.productName
                               : QStringLiteral("%1 (%2)").arg(config.productName, config.productSeries));
    }
    emit modelNamesChanged();

    if (availableConfigs_.isEmpty()) {
        updateConnectionStatus(QStringLiteral("连接状态：未找到 XML 型号文件"));
        updateOperationStatus(QStringLiteral("操作状态：请检查 XML 文件夹"));
    }
}

void AppController::loadCurrentDeviceConfig()
{
    if (currentModelIndex_ <= 0 || currentModelIndex_ > availableConfigs_.size()) {
        currentConfig_ = DeviceConfig{};
        parameterModel_.setRegisters({});
        monitorModel_.setMonitors({});
        faultModel_.setFaults({});
        updateSelectedModelStatus();
        updateConnectionStatus(QStringLiteral("连接状态：未选择型号"));
        updateOperationStatus(QStringLiteral("操作状态：待选择型号"));
        return;
    }

    currentConfig_ = availableConfigs_.at(currentModelIndex_ - 1);
    parameterModel_.setRegisters(currentConfig_.registers);
    refreshMonitorTableFromConfig();
    refreshFaultTableFromConfig();
    updateSelectedModelStatus();
    updateConnectionStatus(QStringLiteral("连接状态：已加载 %1，待连接").arg(currentConfig_.productName));
    updateOperationStatus(QStringLiteral("操作状态：参数总表已初始化"));
}

void AppController::updateConnectionStatus(const QString &statusText)
{
    if (connectionStatus_ == statusText) {
        return;
    }
    connectionStatus_ = statusText;
    emit connectionStatusChanged();
}

void AppController::updateOperationStatus(const QString &statusText)
{
    if (operationStatus_ == statusText) {
        return;
    }
    operationStatus_ = statusText;
    emit operationStatusChanged();
}

void AppController::updateSelectedModelStatus()
{
    const QString statusText = currentConfig_.isValid()
                                   ? QStringLiteral("当前型号：%1").arg(currentConfig_.productName)
                                   : QStringLiteral("当前型号：未选择");
    if (selectedModelStatus_ == statusText) {
        return;
    }
    selectedModelStatus_ = statusText;
    emit selectedModelStatusChanged();
}

void AppController::updateProgress(int finished, int total, const QString &prefix)
{
    if (total <= 0) {
        progressMaximum_ = 100;
        progressValue_ = 0;
        progressText_ = QStringLiteral("%1：空闲").arg(prefix);
        emit progressChanged();
        return;
    }

    progressMaximum_ = total;
    progressValue_ = std::clamp(finished, 0, total);
    progressText_ = QStringLiteral("%1：%2/%3").arg(prefix).arg(progressValue_).arg(progressMaximum_);
    emit progressChanged();
}

void AppController::refreshMonitorTableFromConfig()
{
    QVector<MonitorDefinition> monitorDefinitions;
    monitorDefinitions.reserve(currentConfig_.monitors.size());
    for (const MonitorDefinition &definition : currentConfig_.monitors) {
        if (!isFaultMonitorDefinition(definition)) {
            monitorDefinitions.append(definition);
        }
    }
    monitorModel_.setMonitors(monitorDefinitions);
}

void AppController::refreshFaultTableFromConfig()
{
    QVector<MonitorDefinition> faultDefinitions;
    faultDefinitions.reserve(currentConfig_.monitors.size());
    for (const MonitorDefinition &definition : currentConfig_.monitors) {
        if (isFaultMonitorDefinition(definition)) {
            faultDefinitions.append(definition);
        }
    }
    faultModel_.setFaults(faultDefinitions);
}

void AppController::startParameterUpload(bool checkedOnly, bool automatic)
{
    if (!ensureModbusReady(checkedOnly ? QStringLiteral("上传勾选") : QStringLiteral("上传全部"))) {
        return;
    }

    parameterUploadIsAutomatic_ = automatic;
    const QVector<RegisterDefinition> source = checkedOnly ? parameterModel_.checkedRegisters()
                                                           : parameterModel_.allRegisters();
    pendingParameterUploadQueue_.clear();
    pendingParameterReadMap_.clear();
    for (const RegisterDefinition &definition : source) {
        if (isTransferableRegister(definition)) {
            pendingParameterUploadQueue_.append(definition);
        }
    }

    parameterUploadTotal_ = pendingParameterUploadQueue_.size();
    parameterUploadFinished_ = 0;
    updateProgress(0, parameterUploadTotal_, automatic ? QStringLiteral("自动上传") : QStringLiteral("上传"));
    if (parameterUploadTotal_ == 0) {
        updateOperationStatus(automatic ? QStringLiteral("操作状态：自动上传无可上传参数")
                                        : QStringLiteral("操作状态：没有可上传参数"));
        parameterUploadIsAutomatic_ = false;
        return;
    }

    updateOperationStatus(automatic ? QStringLiteral("操作状态：自动上传 %1 个参数").arg(parameterUploadTotal_)
                                    : QStringLiteral("操作状态：开始上传 %1 个参数").arg(parameterUploadTotal_));
    startNextParameterUpload();
}

void AppController::startNextParameterUpload()
{
    while (!pendingParameterUploadQueue_.isEmpty()) {
        const RegisterDefinition definition = pendingParameterUploadQueue_.takeFirst();
        int startAddress = 0;
        if (!parseSingleAddress(definition.modbusAddr, &startAddress)) {
            ++parameterUploadFinished_;
            updateProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                           ? QStringLiteral("自动上传")
                                                                           : QStringLiteral("上传"));
            continue;
        }

        const QString requestTag = QStringLiteral("parameter-upload:%1:%2")
                                       .arg(startAddress)
                                       .arg(++modbusRequestSerial_);
        pendingParameterReadMap_.insert(requestTag, definition);
        if (modbusClient_->readHoldingRegisters(startAddress, registerCountForParameter(definition), requestTag)) {
            return;
        }

        pendingParameterReadMap_.remove(requestTag);
        ++parameterUploadFinished_;
        updateProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                       ? QStringLiteral("自动上传")
                                                                       : QStringLiteral("上传"));
    }

    updateOperationStatus(parameterUploadIsAutomatic_
                              ? QStringLiteral("操作状态：自动上传完成，共 %1 个").arg(parameterUploadFinished_)
                              : QStringLiteral("操作状态：参数上传完成，共 %1 个").arg(parameterUploadFinished_));
    if (parameterUploadIsAutomatic_) {
        startServoStatePolling();
    }
    parameterUploadIsAutomatic_ = false;
}

void AppController::startParameterDownload(bool checkedOnly)
{
    if (!ensureModbusReady(checkedOnly ? QStringLiteral("下载勾选") : QStringLiteral("下载全部"))) {
        return;
    }

    const QVector<RegisterDefinition> source = checkedOnly ? parameterModel_.checkedRegisters()
                                                           : parameterModel_.allRegisters();
    pendingParameterDownloadQueue_.clear();
    for (const RegisterDefinition &definition : source) {
        if (isTransferableRegister(definition)
            && definition.rwAttribution.trimmed().compare(QStringLiteral("RW"), Qt::CaseInsensitive) == 0) {
            pendingParameterDownloadQueue_.append(definition);
        }
    }

    parameterDownloadTotal_ = pendingParameterDownloadQueue_.size();
    parameterDownloadFinished_ = 0;
    updateProgress(0, parameterDownloadTotal_, QStringLiteral("下载"));
    if (parameterDownloadTotal_ == 0) {
        updateOperationStatus(QStringLiteral("操作状态：没有可下载参数"));
        return;
    }

    updateOperationStatus(QStringLiteral("操作状态：开始下载 %1 个参数").arg(parameterDownloadTotal_));
    startNextParameterDownload();
}

void AppController::startNextParameterDownload()
{
    while (!pendingParameterDownloadQueue_.isEmpty()) {
        const RegisterDefinition definition = pendingParameterDownloadQueue_.takeFirst();
        int startAddress = 0;
        QVector<quint16> registers;
        if (!parseSingleAddress(definition.modbusAddr, &startAddress)
            || !convertParameterValueToRegisters(definition, definition.parameter, &registers)) {
            ++parameterDownloadFinished_;
            updateProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
            continue;
        }

        const QString requestTag = QStringLiteral("parameter-download:%1:%2")
                                       .arg(startAddress)
                                       .arg(++modbusRequestSerial_);
        if (modbusClient_->writeHoldingRegisters(startAddress, registers, requestTag)) {
            return;
        }

        ++parameterDownloadFinished_;
        updateProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
    }

    updateOperationStatus(QStringLiteral("操作状态：参数下载完成，共 %1 个").arg(parameterDownloadFinished_));
}

void AppController::handleConnectionStatusChanged(bool connected, const QString &statusText)
{
    updateConnectionStatus(QStringLiteral("连接状态：%1").arg(statusText));
    if (connected_ != connected) {
        connected_ = connected;
        emit connectedChanged();
    }

    if (connected) {
        updateOperationStatus(QStringLiteral("操作状态：连接成功，自动上传全部参数"));
        startParameterUpload(false, true);
        return;
    }

    if (statusText.contains(QStringLiteral("正在连接")) || statusText.contains(QStringLiteral("正在断开"))) {
        updateOperationStatus(QStringLiteral("操作状态：%1").arg(statusText));
        return;
    }

    pendingParameterUploadQueue_.clear();
    pendingParameterDownloadQueue_.clear();
    pendingParameterReadMap_.clear();
    pendingMonitorReadMap_.clear();
    pendingFaultReadMap_.clear();
    pendingPositionReadMap_.clear();
    pendingPositionWriteMap_.clear();
    pendingPositionWriteValueMap_.clear();
    positionModeBackups_.clear();
    positionRunActive_ = false;
    positionSetupApplied_ = false;
    positionCurrentPollingPaused_ = false;
    positionCurrentReadPending_ = false;
    positionCurrentZeroCaptured_ = false;
    positionCurrentZero_ = 0;
    if (positionCurrentTimer_) {
        positionCurrentTimer_->stop();
    }
    emit positionPollingPausedChanged(false);
    stopMonitorPolling();
    stopFaultPolling();
    stopServoStatePolling();
    parameterUploadIsAutomatic_ = false;
    setServoStateDisplay(QStringLiteral("未连接"), false);
    updateProgress(0, 0, QStringLiteral("参数进度"));
    updateOperationStatus(QStringLiteral("操作状态：已断开"));
}

void AppController::handleModbusError(const QString &errorText)
{
    updateConnectionStatus(QStringLiteral("连接状态：错误 - %1").arg(errorText));
}

void AppController::handleRegisterReadCompleted(int startAddress,
                                                const QVector<quint16> &values,
                                                bool success,
                                                const QString &message,
                                                const QString &requestTag)
{
    if (requestTag.startsWith(QStringLiteral("position-backup:"))) {
        const int address = pendingPositionReadMap_.value(requestTag, startAddress);
        pendingPositionReadMap_.remove(requestTag);
        if (!positionRunActive_) {
            return;
        }

        if (!success || values.isEmpty()) {
            updateOperationStatus(QStringLiteral("操作状态：定位模式寄存器 %1 备份失败，%2").arg(address).arg(message));
            return;
        }

        positionModeBackups_.insert(address, values.first());
        if (address == kPositionEnableRegister) {
            emit positionEnableStateChanged(values.first() == 1);
        }
        maybeApplyPositionSetupAfterBackup();
        return;
    }

    if (requestTag.startsWith(QStringLiteral("position-init:"))) {
        const int address = pendingPositionReadMap_.value(requestTag, startAddress);
        pendingPositionReadMap_.remove(requestTag);
        if (!positionRunActive_) {
            return;
        }

        if (!success || values.isEmpty()) {
            updateOperationStatus(QStringLiteral("操作状态：定位寄存器 %1 初始化读取失败，%2").arg(address).arg(message));
            return;
        }

        if (address == kPositionEnableRegister) {
            emit positionEnableStateChanged(values.first() == 1);
        } else {
            emit positionRegisterValueChanged(address, positionValueFromRegisters(address, values));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("position-current:"))) {
        pendingPositionReadMap_.remove(requestTag);
        positionCurrentReadPending_ = false;
        if (!positionRunActive_) {
            return;
        }

        if (success && values.size() >= kPositionCurrentRegisterCount) {
            const quint32 rawPosition = (static_cast<quint32>(values.at(1)) << 16U) | values.at(0);
            const qint32 absolutePosition = static_cast<qint32>(rawPosition);
            if (!positionCurrentZeroCaptured_) {
                positionCurrentZero_ = absolutePosition;
                positionCurrentZeroCaptured_ = true;
            }

            const qint64 relativePosition = static_cast<qint64>(absolutePosition) - positionCurrentZero_;
            const qint64 boundedPosition = std::clamp(relativePosition,
                                                      static_cast<qint64>(std::numeric_limits<qint32>::min()),
                                                      static_cast<qint64>(std::numeric_limits<qint32>::max()));
            emit positionCurrentPositionChanged(static_cast<qint32>(boundedPosition));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("factory-reset-check:"))
        || requestTag.startsWith(QStringLiteral("motor-reset-check:"))) {
        if (!success || values.isEmpty()) {
            updateOperationStatus(QStringLiteral("操作状态：伺服状态读取失败，%1").arg(message));
            return;
        }

        if (values.first() == 0x07) {
            emit toastRequested(QStringLiteral("电机使能中，请先断使能！"));
            updateOperationStatus(QStringLiteral("操作状态：电机使能中，命令已取消"));
            return;
        }

        if (requestTag.startsWith(QStringLiteral("factory-reset-check:"))) {
            writeControlRegister(91, 1, QStringLiteral("factory-reset"));
        } else {
            writeControlRegister(46, 1, QStringLiteral("motor-reset"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("servo-state:"))) {
        if (!success || values.isEmpty()) {
            setServoStateDisplay(QStringLiteral("状态读取失败"), false);
            return;
        }

        const quint16 stateValue = values.first();
        const bool alarmActive = stateValue == 0x11;
        if (alarmActive) {
            if (!servoAlarmActive_) {
                setServoStateDisplay(QStringLiteral("伺服报警（故障读取中）"), true);
            }
            if (!hasHighPriorityModbusWork()) {
                requestFaultRegisters(QStringLiteral("servo-fault"));
            }
        } else {
            setServoStateDisplay(servoSystemStateText(stateValue), false);
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("fault-page:"))
        || requestTag.startsWith(QStringLiteral("servo-fault:"))) {
        pendingFaultReadMap_.take(requestTag);
        if (success) {
            updateFaultRegisterSnapshot(startAddress, values);
            if (requestTag.startsWith(QStringLiteral("fault-page:"))) {
                updateOperationStatus(QStringLiteral("操作状态：故障寄存器 %1 已刷新").arg(startAddress));
            }
        } else if (requestTag.startsWith(QStringLiteral("fault-page:"))) {
            updateOperationStatus(QStringLiteral("操作状态：故障寄存器 %1 读取失败，%2").arg(startAddress).arg(message));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("monitor:"))) {
        const MonitorDefinition definition = pendingMonitorReadMap_.take(requestTag);
        if (success) {
            monitorModel_.updateMonitorValue(definition, monitorValueFromRegisters(definition, values));
        } else {
            updateOperationStatus(QStringLiteral("操作状态：监控地址 %1 读取失败，%2").arg(startAddress).arg(message));
        }
        return;
    }

    if (!requestTag.startsWith(QStringLiteral("parameter-upload:"))) {
        return;
    }

    const RegisterDefinition definition = pendingParameterReadMap_.take(requestTag);
    ++parameterUploadFinished_;
    updateProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                   ? QStringLiteral("自动上传")
                                                                   : QStringLiteral("上传"));
    if (success) {
        parameterModel_.updateRegisterValue(startAddress, parameterValueFromRegisters(definition, values), true);
    }

    updateOperationStatus(success
                              ? QStringLiteral("操作状态：上传参数 %1/%2 完成")
                                    .arg(parameterUploadFinished_)
                                    .arg(parameterUploadTotal_)
                              : QStringLiteral("操作状态：上传参数 %1 失败，%2").arg(startAddress).arg(message));
    startNextParameterUpload();
}

void AppController::handleRegisterWriteCompleted(int startAddress,
                                                 bool success,
                                                 const QString &message,
                                                 const QString &requestTag)
{
    if (requestTag.startsWith(QStringLiteral("position-"))) {
        const int address = pendingPositionWriteMap_.value(requestTag, startAddress);
        const qint64 value = pendingPositionWriteValueMap_.value(requestTag, 0);
        pendingPositionWriteMap_.remove(requestTag);
        pendingPositionWriteValueMap_.remove(requestTag);

        if (!success) {
            updateOperationStatus(QStringLiteral("操作状态：定位寄存器 %1 写入失败，%2").arg(address).arg(message));
            return;
        }

        if (requestTag.startsWith(QStringLiteral("position-input:"))) {
            emit positionRegisterValueChanged(address, value);
            updateOperationStatus(QStringLiteral("操作状态：定位寄存器 %1 写入完成").arg(address));
        } else if (requestTag.startsWith(QStringLiteral("position-enable:"))) {
            emit positionEnableStateChanged(value == 1);
            updateOperationStatus(value == 1
                                      ? QStringLiteral("操作状态：定位使能已下发")
                                      : QStringLiteral("操作状态：定位失能已下发"));
        } else if (requestTag.startsWith(QStringLiteral("position-setup:"))) {
            updateOperationStatus(QStringLiteral("操作状态：定位模式寄存器 %1 设置完成").arg(address));
        } else if (requestTag.startsWith(QStringLiteral("position-restore:"))) {
            updateOperationStatus(QStringLiteral("操作状态：定位模式寄存器 %1 已恢复").arg(address));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("save-user:"))
        || requestTag.startsWith(QStringLiteral("save-motor:"))) {
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：保存参数完成")
                                  : QStringLiteral("操作状态：保存参数失败，%1").arg(message));
        if (success) {
            emit toastRequested(QStringLiteral("保存成功！"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("fault-reset:"))) {
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：故障复位命令已发送")
                                  : QStringLiteral("操作状态：故障复位失败，%1").arg(message));
        return;
    }

    if (requestTag.startsWith(QStringLiteral("factory-reset:"))) {
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：恢复出厂命令已发送")
                                  : QStringLiteral("操作状态：恢复出厂失败，%1").arg(message));
        if (success) {
            emit toastRequested(QStringLiteral("4S后再复位电机。"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("motor-reset:"))) {
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：电机复位完成")
                                  : QStringLiteral("操作状态：电机复位失败，%1").arg(message));
        if (success) {
            emit toastRequested(QStringLiteral("复位成功！"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("parameter-download:"))) {
        parameterModel_.markParameterSendState(startAddress, success);
        ++parameterDownloadFinished_;
        updateProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：下载参数 %1/%2 完成")
                                        .arg(parameterDownloadFinished_)
                                        .arg(parameterDownloadTotal_)
                                  : QStringLiteral("操作状态：下载参数 %1 失败，%2").arg(startAddress).arg(message));
        startNextParameterDownload();
        return;
    }

    if (requestTag.startsWith(QStringLiteral("parameter-single:"))) {
        parameterModel_.markParameterSendState(startAddress, success);
        updateOperationStatus(success
                                  ? QStringLiteral("操作状态：寄存器 %1 下发完成").arg(startAddress)
                                  : QStringLiteral("操作状态：寄存器 %1 下发失败，%2").arg(startAddress).arg(message));
    }
}

bool AppController::ensureModbusReady(const QString &operationName)
{
    if (!currentConfig_.isValid()) {
        updateConnectionStatus(QStringLiteral("连接状态：%1失败，请先选择电机型号 XML").arg(operationName));
        return false;
    }
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        updateConnectionStatus(QStringLiteral("连接状态：%1失败，请先连接伺服电机").arg(operationName));
        return false;
    }
    return true;
}

bool AppController::writeParameterToServo(const RegisterDefinition &definition, const QString &newValue)
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        updateConnectionStatus(QStringLiteral("连接状态：参数下发失败，请先连接伺服电机"));
        return false;
    }

    int startAddress = 0;
    if (!parseSingleAddress(definition.modbusAddr, &startAddress)) {
        updateConnectionStatus(QStringLiteral("连接状态：参数地址 %1 无法转换为 Modbus 地址").arg(definition.modbusAddr));
        return false;
    }

    QVector<quint16> registers;
    if (!convertParameterValueToRegisters(definition, newValue, &registers)) {
        updateConnectionStatus(QStringLiteral("连接状态：参数值 %1 无法转换为寄存器").arg(newValue));
        return false;
    }

    updateOperationStatus(QStringLiteral("操作状态：正在下发寄存器 %1").arg(startAddress));
    const QString requestTag = QStringLiteral("parameter-single:%1:%2").arg(startAddress).arg(++modbusRequestSerial_);
    return modbusClient_->writeHoldingRegisters(startAddress, registers, requestTag);
}

bool AppController::writeControlRegister(int address, quint16 value, const QString &requestPrefix)
{
    if (!ensureModbusReady(commandOperationName(requestPrefix))) {
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2").arg(requestPrefix).arg(++modbusRequestSerial_);
    return modbusClient_->writeHoldingRegisters(address, QVector<quint16>{value}, requestTag);
}

bool AppController::requestServoStateCheck(const QString &requestPrefix)
{
    if (!ensureModbusReady(commandOperationName(requestPrefix))) {
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2").arg(requestPrefix).arg(++modbusRequestSerial_);
    return modbusClient_->readHoldingRegisters(187, 1, requestTag);
}

void AppController::startServoStatePolling()
{
    if (!modbusClient_ || !modbusClient_->isConnected() || !servoStateTimer_) {
        return;
    }

    if (!servoStateTimer_->isActive()) {
        servoStateTimer_->start(4000);
    }
    pollServoSystemState();
}

void AppController::stopServoStatePolling()
{
    if (servoStateTimer_) {
        servoStateTimer_->stop();
    }
}

void AppController::pollServoSystemState()
{
    if (!modbusClient_ || !modbusClient_->isConnected() || hasHighPriorityModbusWork()) {
        return;
    }

    const QString requestTag = QStringLiteral("servo-state:%1").arg(++modbusRequestSerial_);
    if (!modbusClient_->readHoldingRegisters(187, 1, requestTag)) {
        setServoStateDisplay(QStringLiteral("状态读取失败"), false);
    }
}

void AppController::setServoStateDisplay(const QString &stateText, bool alarmActive)
{
    const QString displayText = QStringLiteral("伺服状态：%1").arg(stateText);
    if (servoAlarmActive_ == alarmActive && servoStateText_ == displayText) {
        return;
    }

    servoAlarmActive_ = alarmActive;
    servoStateText_ = displayText;
    emit servoStateChanged();
}

void AppController::refreshServoAlarmFaultText()
{
    if (!servoAlarmActive_) {
        return;
    }

    const QStringList activeFaults = faultModel_.activeFaultNames();
    const QString faultText = activeFaults.isEmpty()
                                  ? QStringLiteral("未解析到故障位")
                                  : activeFaults.join(QStringLiteral("、"));
    setServoStateDisplay(QStringLiteral("伺服报警（%1）").arg(faultText), true);
}

void AppController::pollSelectedMonitors()
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        stopMonitorPolling();
        return;
    }

    const QVector<MonitorDefinition> monitors = monitorModel_.checkedMonitors();
    if (monitors.isEmpty()) {
        return;
    }

    for (const MonitorDefinition &definition : monitors) {
        int startAddress = 0;
        if (!parseSingleAddress(definition.modbusAddr, &startAddress)) {
            continue;
        }

        const QString requestTag = QStringLiteral("monitor:%1:%2")
                                       .arg(startAddress)
                                       .arg(++modbusRequestSerial_);
        pendingMonitorReadMap_.insert(requestTag, definition);
        if (!modbusClient_->readHoldingRegisters(startAddress, registerCountForMonitor(definition), requestTag)) {
            pendingMonitorReadMap_.remove(requestTag);
        }
    }
}

void AppController::stopMonitorPolling()
{
    if (monitorTimer_) {
        monitorTimer_->stop();
    }
    if (monitorPollingActive_) {
        monitorPollingActive_ = false;
        emit monitorPollingActiveChanged();
    }
}

void AppController::startFaultPolling()
{
    if (!faultPollTimer_ || !ensureModbusReady(QStringLiteral("故障轮询"))) {
        return;
    }

    faultPollTimer_->start(kFaultPollIntervalMs);
    updateOperationStatus(QStringLiteral("操作状态：故障轮询中，间隔 %1 ms").arg(kFaultPollIntervalMs));
    pollFaultRegisters();
}

void AppController::stopFaultPolling()
{
    if (faultPollTimer_) {
        faultPollTimer_->stop();
    }
}

void AppController::pollFaultRegisters()
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        stopFaultPolling();
        return;
    }

    if (!pendingFaultReadMap_.isEmpty()) {
        return;
    }

    requestFaultRegisters(QStringLiteral("fault-page"));
}

void AppController::requestFaultRegisters(const QString &requestPrefix)
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        return;
    }

    const int faultAddresses[] = {kFaultRegisterLow, kFaultRegisterHigh};
    for (const int address : faultAddresses) {
        const QString requestTag = QStringLiteral("%1:%2:%3")
                                       .arg(requestPrefix)
                                       .arg(address)
                                       .arg(++modbusRequestSerial_);
        pendingFaultReadMap_.insert(requestTag, address);
        if (!modbusClient_->readHoldingRegisters(address, 1, requestTag)) {
            pendingFaultReadMap_.remove(requestTag);
        }
    }
}

void AppController::updateFaultRegisterSnapshot(int startAddress, const QVector<quint16> &values)
{
    if (values.isEmpty()) {
        return;
    }

    faultModel_.updateFaultRegisterValue(startAddress, values.first());
    refreshServoAlarmFaultText();
}

/**
 * @brief Reads the position-run initialization registers displayed by the QWidget panel.
 * @author mozhengjie
 */
void AppController::readPositionInitialRegisters()
{
    const int initialRegisters[] = {kPositionJogSpeedRegister,
                                    kPositionJogAccelerationRegister,
                                    kPositionJogDecelerationRegister,
                                    kPositionRunDistanceRegister,
                                    kPositionWaitTimeRegister};
    for (const int address : initialRegisters) {
        const RegisterDefinition *definition = registerDefinitionForAddress(address);
        readPositionRegister(address,
                             definition ? registerCountForParameter(*definition) : 1,
                             QStringLiteral("position-init"));
    }
}

/**
 * @brief Polls signed 32-bit current position from Pn449/Pn450 when the position panel is active.
 * @author mozhengjie
 */
void AppController::pollPositionCurrentPosition()
{
    if (!positionRunActive_ || positionCurrentPollingPaused_ || positionCurrentReadPending_) {
        return;
    }
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        return;
    }
    if (!pendingPositionWriteMap_.isEmpty()) {
        return;
    }

    const QString requestTag = QStringLiteral("position-current:%1").arg(++modbusRequestSerial_);
    pendingPositionReadMap_.insert(requestTag, kPositionCurrentLowRegister);
    positionCurrentReadPending_ = true;
    if (!modbusClient_->readHoldingRegisters(kPositionCurrentLowRegister,
                                             kPositionCurrentRegisterCount,
                                             requestTag)) {
        pendingPositionReadMap_.remove(requestTag);
        positionCurrentReadPending_ = false;
    }
}

/**
 * @brief Sends a tagged position-run register read request.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param registerCount Number of holding registers to read.
 * @param requestPrefix Position request prefix.
 * @return bool true when the request is queued.
 */
bool AppController::readPositionRegister(int address, int registerCount, const QString &requestPrefix)
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2:%3")
                                   .arg(requestPrefix)
                                   .arg(address)
                                   .arg(++modbusRequestSerial_);
    pendingPositionReadMap_.insert(requestTag, address);
    if (modbusClient_->readHoldingRegisters(address, qMax(1, registerCount), requestTag)) {
        return true;
    }

    pendingPositionReadMap_.remove(requestTag);
    return false;
}

/**
 * @brief Sends a tagged position-run register write request.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param value Value to convert and write.
 * @param requestPrefix Position request prefix.
 * @return bool true when the request is queued.
 */
bool AppController::writePositionRawRegister(int address, qint64 value, const QString &requestPrefix)
{
    if (!ensureModbusReady(QStringLiteral("定位运行"))) {
        return false;
    }

    const QVector<quint16> registers = positionRegistersForValue(address, value);
    if (registers.isEmpty()) {
        updateOperationStatus(QStringLiteral("操作状态：定位寄存器 %1 数值无效").arg(address));
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2:%3")
                                   .arg(requestPrefix)
                                   .arg(address)
                                   .arg(++modbusRequestSerial_);
    pendingPositionWriteMap_.insert(requestTag, address);
    pendingPositionWriteValueMap_.insert(requestTag, value);
    if (modbusClient_->writeHoldingRegisters(address, registers, requestTag)) {
        return true;
    }

    pendingPositionWriteMap_.remove(requestTag);
    pendingPositionWriteValueMap_.remove(requestTag);
    return false;
}

/**
 * @brief Converts a position-run value to low-word-first Modbus registers.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param value Numeric value to write.
 * @return QVector<quint16> Register words; empty when conversion fails.
 */
QVector<quint16> AppController::positionRegistersForValue(int address, qint64 value) const
{
    if (const RegisterDefinition *definition = registerDefinitionForAddress(address)) {
        QVector<quint16> registers;
        if (convertParameterValueToRegisters(*definition, QString::number(value), &registers)) {
            return registers;
        }
        return {};
    }

    if (value < std::numeric_limits<qint16>::min() || value > std::numeric_limits<quint16>::max()) {
        return {};
    }
    return {static_cast<quint16>(value & 0xFFFF)};
}

/**
 * @brief Converts low-word-first position-run registers to a display integer.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param values Raw register values.
 * @return qint64 Display value.
 */
qint64 AppController::positionValueFromRegisters(int address, const QVector<quint16> &values) const
{
    if (values.isEmpty()) {
        return 0;
    }

    if (const RegisterDefinition *definition = registerDefinitionForAddress(address)) {
        bool ok = false;
        const qint64 value = parameterValueFromRegisters(*definition, values).toLongLong(&ok);
        return ok ? value : 0;
    }

    return values.first();
}

/**
 * @brief Finds the XML parameter definition for a Modbus address when available.
 * @author mozhengjie
 * @param address Modbus register address.
 * @return const RegisterDefinition* Matching XML definition, or nullptr.
 */
const RegisterDefinition *AppController::registerDefinitionForAddress(int address) const
{
    for (const RegisterDefinition &definition : currentConfig_.registers) {
        int parsedAddress = 0;
        if (parseSingleAddress(definition.modbusAddr, &parsedAddress) && parsedAddress == address) {
            return &definition;
        }
    }
    return nullptr;
}

/**
 * @brief Applies Pn0/Pn1 position setup after Pn0/Pn1/Pn44 original values have been backed up.
 * @author mozhengjie
 */
void AppController::maybeApplyPositionSetupAfterBackup()
{
    if (!positionRunActive_ || positionSetupApplied_) {
        return;
    }
    if (!positionModeBackups_.contains(kPositionControlModeRegister)
        || !positionModeBackups_.contains(kPositionCommandSourceRegister)
        || !positionModeBackups_.contains(kPositionEnableRegister)) {
        return;
    }

    positionSetupApplied_ = true;
    writePositionRawRegister(kPositionControlModeRegister, 0, QStringLiteral("position-setup"));
    writePositionRawRegister(kPositionCommandSourceRegister, 3, QStringLiteral("position-setup"));
    updateOperationStatus(QStringLiteral("操作状态：定位运行控制模式已下发"));
}

bool AppController::hasHighPriorityModbusWork() const
{
    return !pendingParameterUploadQueue_.isEmpty()
           || !pendingParameterDownloadQueue_.isEmpty()
           || !pendingParameterReadMap_.isEmpty()
           || !pendingMonitorReadMap_.isEmpty()
           || !pendingFaultReadMap_.isEmpty()
           || !pendingPositionReadMap_.isEmpty()
           || !pendingPositionWriteMap_.isEmpty();
}
