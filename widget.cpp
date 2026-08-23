#include "widget.h"

#include "core/modbusclient.h"
#include "core/xmlconfigloader.h"
#include "dialogs/communicationsettingsdialog.h"
#include "delegates/parametervaluedelegate.h"

#include <algorithm>
#include <limits>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kTopBarHeight = 52;
constexpr int kLeftPanelWidth = 210;
constexpr int kBottomBarHeight = 28;
constexpr int kWindowMargin = 80;
constexpr int kParameterPageIndex = 0;
constexpr int kMonitorPageIndex = 1;
constexpr int kFaultPageIndex = 2;
constexpr int kScopePageIndex = 3;
constexpr int kJogActionIndex = -101;
constexpr int kPositionActionIndex = -102;
constexpr int kFaultRegisterLow = 150;
constexpr int kFaultRegisterHigh = 390;
constexpr int kFaultPollIntervalMs = 1000;
constexpr int kServoRunStateValue = 0x07;

/**
 * @brief 创建无边框基础容器，并应用指定样式。
 * @author mozhengjie
 * @param styleSheet Qt 样式表字符串。
 * @return QFrame* 已创建的基础容器指针。
 */
QFrame *createLineFrame(const QString &styleSheet)
{
    auto *frame = new QFrame;
    frame->setFrameShape(QFrame::NoFrame);
    frame->setStyleSheet(styleSheet);
    return frame;
}

/**
 * @brief 将用户输入的参数值转换为 Modbus 16 位寄存器列表。
 * @author mozhengjie
 * @param definition 参数定义。
 * @param newValue 新参数值。
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

        // XML 标记为 int32 的参数在该伺服协议中采用低字在前、 高字在后的 Modbus 字序。
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
 * @brief 将文本解析为 double。
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
 * @brief 解析寄存器定义的起始地址。
 * @author mozhengjie
 * @param addressText 地址文本。
 * @param address 输出地址。
 * @return bool 单地址解析成功返回 true。
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
 * @brief 判断寄存器定义是否为可批量传输参数。
 * @author mozhengjie
 * @param definition 参数定义。
 * @return bool 可批量读取或写入返回 true。
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
 * @brief 将顶部命令请求前缀转换为用户可读操作名。
 * @author mozhengjie
 * @param requestPrefix 请求标识前缀。
 * @return QString 用户可读操作名。
 */
QString commandOperationName(const QString &requestPrefix)
{
    if (requestPrefix == QStringLiteral("save-user")
        || requestPrefix == QStringLiteral("save-motor")) {
        return QStringLiteral("保存参数");
    }
    if (requestPrefix == QStringLiteral("fault-reset")) {
        return QStringLiteral("故障复位");
    }
    if (requestPrefix == QStringLiteral("factory-reset")
        || requestPrefix == QStringLiteral("factory-reset-check")) {
        return QStringLiteral("恢复出厂");
    }
    if (requestPrefix == QStringLiteral("motor-reset")
        || requestPrefix == QStringLiteral("motor-reset-check")) {
        return QStringLiteral("电机复位");
    }
    return requestPrefix;
}

/**
 * @brief 获取参数定义占用的 Modbus 寄存器数量。
 * @author mozhengjie
 * @param definition 参数定义。
 * @return int 寄存器数量。
 */
int registerCountForParameter(const RegisterDefinition &definition)
{
    return definition.remark.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0 ? 2 : 1;
}

/**
 * @brief 获取监控项占用的 Modbus 寄存器数量。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @return int 寄存器数量。
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
 * @brief 将读取寄存器值转换为参数表显示值。
 * @author mozhengjie
 * @param definition 参数定义。
 * @param values 读取到的寄存器值。
 * @return QString 参数显示值。
 */
QString parameterValueFromRegisters(const RegisterDefinition &definition, const QVector<quint16> &values)
{
    if (values.isEmpty()) {
        return {};
    }

    double minimum = 0.0;
    const bool hasSignedMinimum = parseDoubleText(definition.minimum, &minimum) && minimum < 0.0;
    if (registerCountForParameter(definition) == 2 && values.size() >= 2) {
        const quint32 rawValue = (static_cast<quint32>(values.at(1)) << 16U) | values.at(0);
        if (hasSignedMinimum && rawValue > static_cast<quint32>(std::numeric_limits<qint32>::max())) {
            return QString::number(static_cast<qint32>(rawValue));
        }
        return QString::number(rawValue);
    }

    const quint16 rawValue = values.first();
    if (hasSignedMinimum && rawValue > static_cast<quint16>(std::numeric_limits<qint16>::max())) {
        return QString::number(static_cast<qint16>(rawValue));
    }
    return QString::number(rawValue);
}

/**
 * @brief 将读取寄存器值转换为监控表显示值。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @param values 读取到的寄存器值。
 * @return QString 监控显示值。
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
 * @brief 将伺服系统状态寄存器值转换为精简显示文本。
 * @author mozhengjie
 * @param stateValue 寄存器 187 的系统状态值。
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
 * @brief 构造伺服调试软件主窗口，并初始化四区界面布局。
 * @author mozhengjie
 * @param parent 父窗口指针，默认为空。
 */
Widget::Widget(QWidget *parent)
    : QMainWindow(parent)
{
    modbusClient_ = new ModbusClient(this);
    connect(modbusClient_, &ModbusClient::connectionStatusChanged,
            this, &Widget::handleConnectionStatusChanged);
    connect(modbusClient_, &ModbusClient::errorOccurred, this, &Widget::handleModbusError);
    connect(modbusClient_, &ModbusClient::writeCompleted,
            this, &Widget::handleParameterWriteCompleted);
    connect(modbusClient_, &ModbusClient::readCompleted,
            this, &Widget::handleRegisterReadCompleted);

    monitorTimer_ = new QTimer(this);
    connect(monitorTimer_, &QTimer::timeout, this, &Widget::pollSelectedMonitors);

    servoStateTimer_ = new QTimer(this);
    servoStateTimer_->setInterval(4000);
    connect(servoStateTimer_, &QTimer::timeout, this, &Widget::pollServoSystemState);

    faultPollTimer_ = new QTimer(this);
    faultPollTimer_->setInterval(kFaultPollIntervalMs);
    connect(faultPollTimer_, &QTimer::timeout, this, &Widget::pollFaultRegisters);

    setWindowTitle(QStringLiteral("伺服调试软件"));
    setMinimumSize(800, 520);
    setStyleSheet(QStringLiteral("* { color: #000000; }"));

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 顶部命令区与中间内容区、左侧导航与主显示区均通过 splitter 支持拖动调整尺寸。
    auto *workAreaSplitter = new QSplitter(Qt::Vertical);
    workAreaSplitter->setChildrenCollapsible(false);
    workAreaSplitter->setHandleWidth(6);
    workAreaSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #A8A8A8; }"
        "QSplitter::handle:hover { background: #7DAAA6; }"));

    auto *contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setHandleWidth(6);
    contentSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #A8A8A8; }"
        "QSplitter::handle:hover { background: #7DAAA6; }"));
    contentSplitter->addWidget(createLeftPanel());
    contentSplitter->addWidget(createMainArea());
    contentSplitter->setStretchFactor(0, 0);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes({kLeftPanelWidth, 960});

    workAreaSplitter->addWidget(createTopCommandBar());
    workAreaSplitter->addWidget(contentSplitter);
    workAreaSplitter->setStretchFactor(0, 0);
    workAreaSplitter->setStretchFactor(1, 1);
    workAreaSplitter->setSizes({kTopBarHeight, 620});
    rootLayout->addWidget(workAreaSplitter, 1);

    rootLayout->addWidget(createBottomStatusBar());
    setCentralWidget(central);
    scanXmlModelFiles();
    applyInitialWindowGeometry();
}

/**
 * @brief 创建顶部命令栏。
 * @author mozhengjie
 * @return QWidget* 顶部命令栏控件指针。
 */
QWidget *Widget::createTopCommandBar()
{
    auto *bar = createLineFrame(QStringLiteral(
        "QFrame { background: #D9F5F2; border-bottom: 1px solid #222; }"));
    bar->setMinimumHeight(kTopBarHeight);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 0, 18, 0);
    layout->setSpacing(10);

    const auto addCommandButton = [this, layout](const QString &command) {
        QToolButton *button = createCommandButton(command);
        if (command == QStringLiteral("通讯设置")) {
            connect(button, &QToolButton::clicked, this, &Widget::showCommunicationSettings);
        } else if (command == QStringLiteral("连接/断开")) {
            connectionToggleButton_ = button;
            connect(button, &QToolButton::clicked, this, &Widget::toggleModbusConnection);
        } else if (command == QStringLiteral("保存参数")) {
            saveParameterButton_ = button;
            connect(button, &QToolButton::clicked, this, &Widget::toggleSaveParameterMenu);
        } else if (command == QStringLiteral("故障复位")) {
            connect(button, &QToolButton::clicked, this, &Widget::sendFaultResetCommand);
        } else if (command == QStringLiteral("恢复出厂")) {
            connect(button, &QToolButton::clicked, this, &Widget::requestFactoryResetCommand);
        } else if (command == QStringLiteral("电机复位")) {
            connect(button, &QToolButton::clicked, this, &Widget::requestMotorResetCommand);
        }
        layout->addWidget(button);
    };

    const QStringList leftCommands = {QStringLiteral("通讯设置"), QStringLiteral("连接/断开")};
    const QStringList rightCommands = {
        QStringLiteral("保存参数"), QStringLiteral("开环调试"),
        QStringLiteral("电机调零"), QStringLiteral("故障复位"),
        QStringLiteral("恢复出厂"), QStringLiteral("电机复位")};

    for (const QString &command : leftCommands) {
        addCommandButton(command);
    }
    layout->addStretch(1);
    for (const QString &command : rightCommands) {
        addCommandButton(command);
    }
    return bar;
}

/**
 * @brief 创建左侧型号选择和导航区域。
 * @author mozhengjie
 * @return QWidget* 左侧面板控件指针。
 */
QWidget *Widget::createLeftPanel()
{
    auto *panel = createLineFrame(QStringLiteral(
        "QFrame { background: #FFF2C5; border-right: 1px solid #222; }"));
    panel->setMinimumWidth(160);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 18, 10, 18);
    layout->setSpacing(10);

    // 型号选择由 scanXmlModelFiles() 扫描 XML 目录后动态填充。
    auto *modelTitle = new QLabel(QStringLiteral("选择电机型号"));
    modelTitle->setStyleSheet(QStringLiteral("font-weight: 600; color: #000000;"));
    layout->addWidget(modelTitle);

    modelSelector_ = new QComboBox;
    modelSelector_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    modelSelector_->addItem(QStringLiteral("请选择型号 XML"), QString());
    modelSelector_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #fff; border: 1px solid #999; padding: 4px 6px; }"));
    layout->addWidget(modelSelector_);

    auto *hint = new QLabel(QStringLiteral("型号下方可选择总表和运行工具"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #000000; line-height: 140%;"));
    layout->addWidget(hint);

    navigationTree_ = new QTreeWidget;
    navigationTree_->setHeaderHidden(true);
    navigationTree_->setRootIsDecorated(false);
    navigationTree_->setIndentation(12);
    navigationTree_->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #FFF8DA; border: 1px solid #C8B56A; }"
        "QTreeWidget::item { height: 30px; padding-left: 6px; }"
        "QTreeWidget::item:selected { background: #D8EFC8; color: #000000; }"));

    // 参数、监控和故障入口共用主显示区，故障总表点击后会自动开启 1000ms 轮询。
    auto *parameterItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("参数总表")});
    parameterItem->setData(0, Qt::UserRole, kParameterPageIndex);
    auto *monitorItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("监控总表")});
    monitorItem->setData(0, Qt::UserRole, kMonitorPageIndex);
    auto *faultItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("故障总表")});
    faultItem->setData(0, Qt::UserRole, kFaultPageIndex);
    auto *scopeItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("示波器")});
    scopeItem->setData(0, Qt::UserRole, kScopePageIndex);
    auto *jogItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("点动运行")});
    jogItem->setData(0, Qt::UserRole, kJogActionIndex);
    auto *positionItem = new QTreeWidgetItem(navigationTree_, QStringList{QStringLiteral("定位运行")});
    positionItem->setData(0, Qt::UserRole, kPositionActionIndex);
    navigationTree_->setCurrentItem(parameterItem);
    layout->addWidget(navigationTree_, 1);

    secondaryActionStack_ = new QStackedWidget;
    secondaryActionStack_->addWidget(createParameterActionPanel());
    secondaryActionStack_->addWidget(createMonitorActionPanel());
    secondaryActionStack_->addWidget(createFaultActionPanel());
    secondaryActionStack_->addWidget(createScopeActionPanel());
    layout->addWidget(secondaryActionStack_);

    connect(navigationTree_, &QTreeWidget::currentItemChanged,
            this, &Widget::selectMainPageForCurrentTreeItem);
    connect(navigationTree_, &QTreeWidget::itemClicked,
            this, &Widget::handleNavigationItemClicked);
    connect(modelSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Widget::loadDeviceConfig);
    connect(modelSelector_, &QComboBox::currentTextChanged,
            this, &Widget::updateSelectedModelStatus);

    return panel;
}

/**
 * @brief 创建中间主显示区域，并挂载参数总表和监控总表页面。
 * @author mozhengjie
 * @return QWidget* 主显示区域控件指针。
 */
QWidget *Widget::createMainArea()
{
    mainStack_ = new QStackedWidget;
    mainStack_->setStyleSheet(QStringLiteral(
        "QStackedWidget { background: #CDE8B7; border: none; }"));

    // 参数总表、监控总表和故障总表由 XML 文件动态刷新，控件结构保持稳定。
    mainStack_->addWidget(createParameterPage());
    mainStack_->addWidget(createMonitorPage());
    mainStack_->addWidget(createFaultPage());
    mainStack_->addWidget(createScopePage());
    return mainStack_;
}

/**
 * @brief 创建参数总表二级操作面板。
 * @author mozhengjie
 * @return QWidget* 参数操作面板控件指针。
 */
QWidget *Widget::createParameterActionPanel()
{
    auto *panel = createLineFrame(QStringLiteral(
        "QFrame { background: #FFF8DA; border: 1px solid #C8B56A; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("参数操作"));
    title->setStyleSheet(QStringLiteral("font-weight: 600; color: #000000;"));
    layout->addWidget(title);

    const QStringList actions = {QStringLiteral("上传全部"), QStringLiteral("下载全部"),
                                 QStringLiteral("上传勾选"), QStringLiteral("下载勾选")};
    for (const QString &action : actions) {
        auto *button = createSecondaryButton(action);
        connect(button, &QPushButton::clicked, this, [this, action]() { handleParameterAction(action); });
        layout->addWidget(button);
    }

    return panel;
}

/**
 * @brief 创建监控总表二级操作面板。
 * @author mozhengjie
 * @return QWidget* 监控操作面板控件指针。
 */
QWidget *Widget::createMonitorActionPanel()
{
    auto *panel = createLineFrame(QStringLiteral(
        "QFrame { background: #FFF8DA; border: 1px solid #C8B56A; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("监控操作"));
    title->setStyleSheet(QStringLiteral("font-weight: 600; color: #000000;"));
    layout->addWidget(title);

    auto *intervalLabel = new QLabel(QStringLiteral("监控间隔 ms"));
    layout->addWidget(intervalLabel);

    monitorIntervalSpinBox_ = new QSpinBox;
    monitorIntervalSpinBox_->setRange(10, 60000);
    monitorIntervalSpinBox_->setValue(500);
    monitorIntervalSpinBox_->setSingleStep(10);
    monitorIntervalSpinBox_->setSuffix(QStringLiteral(" ms"));
    monitorIntervalSpinBox_->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #FFFFFF; border: 1px solid #999999; padding: 4px; }"));
    layout->addWidget(monitorIntervalSpinBox_);

    monitorToggleButton_ = createSecondaryButton(QStringLiteral("启动监控"));
    monitorToggleButton_->setProperty("monitoring", false);
    connect(monitorToggleButton_, &QPushButton::clicked, this, &Widget::toggleMonitorPolling);
    layout->addWidget(monitorToggleButton_);

    return panel;
}

/**
 * @brief 创建故障总表二级操作提示面板。
 * @author mozhengjie
 * @return QWidget* 故障操作提示面板控件指针。
 */
QWidget *Widget::createFaultActionPanel()
{
    auto *panel = createLineFrame(QStringLiteral(
        "QFrame { background: #FFF8DA; border: 1px solid #C8B56A; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("故障操作"));
    title->setStyleSheet(QStringLiteral("font-weight: 600; color: #000000;"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("点击故障总表启动/关闭 1000ms 故障轮询"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #000000;"));
    layout->addWidget(hint);
    layout->addStretch(1);
    return panel;
}

/**
 * @brief 创建示波器二级操作占位面板。
 * @author mozhengjie
 * @return QWidget* 示波器操作占位面板控件指针。
 */
QWidget *Widget::createScopeActionPanel()
{
    auto *panel = createLineFrame(QStringLiteral(
        "QFrame { background: #FFF8DA; border: 1px solid #C8B56A; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("示波器"));
    title->setStyleSheet(QStringLiteral("font-weight: 600; color: #000000;"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("示波器功能待实现"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #000000;"));
    layout->addWidget(hint);
    layout->addStretch(1);
    return panel;
}

/**
 * @brief 创建参数总表页面和表格控件。
 * @author mozhengjie
 * @return QWidget* 参数总表页面控件指针。
 */
QWidget *Widget::createParameterPage()
{
    auto *page = createLineFrame(QStringLiteral(
        "QFrame { background: #CDE8B7; border: none; }"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("参数总表"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600; color: #000000;"));
    titleLayout->addWidget(title);
    titleLayout->addStretch(1);

    parameterSearchEdit_ = new QLineEdit;
    parameterSearchEdit_->setPlaceholderText(QStringLiteral("搜索功能说明，回车定位"));
    parameterSearchEdit_->setClearButtonEnabled(true);
    parameterSearchEdit_->setFixedWidth(260);
    parameterSearchEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #FFFFFF; border: 1px solid #999999; padding: 5px 8px; color: #000000; }"));
    connect(parameterSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text != lastParameterSearchText_) {
            lastParameterMatchedRow_ = -1;
        }
    });
    connect(parameterSearchEdit_, &QLineEdit::returnPressed, this, &Widget::searchParameterTable);
    titleLayout->addWidget(parameterSearchEdit_);
    layout->addLayout(titleLayout);

    parameterModel_ = new ParameterTableModel(this);
    parameterTable_ = new QTableView;
    parameterTable_->setModel(parameterModel_);
    parameterTable_->setItemDelegateForColumn(ParameterTableModel::ValueColumn,
                                             new ParameterValueDelegate(parameterTable_));
    parameterTable_->setAlternatingRowColors(true);
    parameterTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    parameterTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    parameterTable_->setStyleSheet(QStringLiteral(
        "QTableView { background: #FFFFFF; alternate-background-color: #F1F1F1; gridline-color: #B8B8B8; }"
        "QHeaderView::section { background: #EFEFEF; border: 1px solid #B8B8B8; padding: 5px; font-weight: 600; color: #000000; }"
        "QTableView::item { padding: 4px; color: #000000; }"));
    configureFixedTableArea(parameterTable_);
    parameterTable_->setColumnWidth(ParameterTableModel::SelectColumn, 56);
    parameterTable_->setColumnWidth(ParameterTableModel::AddressColumn, 96);
    parameterTable_->setColumnWidth(ParameterTableModel::FunctionColumn, 220);
    parameterTable_->setColumnWidth(ParameterTableModel::ValueColumn, 150);
    parameterTable_->setColumnWidth(ParameterTableModel::DefaultColumn, 110);
    parameterTable_->setColumnWidth(ParameterTableModel::UnitColumn, 120);
    parameterTable_->setColumnWidth(ParameterTableModel::MinimumColumn, 100);
    parameterTable_->setColumnWidth(ParameterTableModel::MaximumColumn, 120);
    parameterTable_->setColumnWidth(ParameterTableModel::AttributionColumn, 70);
    connect(parameterModel_, &ParameterTableModel::parameterValueChanged,
            this, [this](const RegisterDefinition &definition, const QString &newValue) {
        const QString name = definition.functionCn.isEmpty() ? definition.functionEn : definition.functionCn;
        if (writeParameterToServo(definition, newValue)) {
            return;
        }
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(
                QStringLiteral("操作状态：参数 %1 已本地更新为 %2，未下发").arg(name, newValue));
        }
    });
    connect(parameterTable_, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (!parameterModel_ || !parameterTable_ || index.column() != ParameterTableModel::ValueColumn) {
            return;
        }
        if (parameterModel_->isValueEditable(index.row())) {
            parameterTable_->edit(index);
        }
    });
    layout->addWidget(parameterTable_, 1);

    return page;
}

/**
 * @brief 创建监控总表页面和表格控件。
 * @author mozhengjie
 * @return QWidget* 监控总表页面控件指针。
 */
QWidget *Widget::createMonitorPage()
{
    auto *page = createLineFrame(QStringLiteral(
        "QFrame { background: #CDE8B7; border: none; }"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("监控总表"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600; color: #000000;"));
    titleLayout->addWidget(title);
    titleLayout->addStretch(1);

    monitorSearchEdit_ = new QLineEdit;
    monitorSearchEdit_->setPlaceholderText(QStringLiteral("搜索监控名称，回车定位"));
    monitorSearchEdit_->setClearButtonEnabled(true);
    monitorSearchEdit_->setFixedWidth(260);
    monitorSearchEdit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #FFFFFF; border: 1px solid #999999; padding: 5px 8px; color: #000000; }"));
    connect(monitorSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text != lastMonitorSearchText_) {
            lastMonitorMatchedRow_ = -1;
        }
    });
    connect(monitorSearchEdit_, &QLineEdit::returnPressed, this, &Widget::searchMonitorTable);
    titleLayout->addWidget(monitorSearchEdit_);
    layout->addLayout(titleLayout);

    monitorModel_ = new MonitorTableModel(this);
    monitorTable_ = new QTableView;
    monitorTable_->setModel(monitorModel_);
    monitorTable_->setAlternatingRowColors(true);
    monitorTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    monitorTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    monitorTable_->setStyleSheet(QStringLiteral(
        "QTableView { background: #FFFFFF; alternate-background-color: #F1F1F1; gridline-color: #B8B8B8; }"
        "QHeaderView::section { background: #EFEFEF; border: 1px solid #B8B8B8; padding: 5px; font-weight: 600; color: #000000; }"
        "QTableView::item { padding: 4px; color: #000000; }"));
    configureFixedTableArea(monitorTable_);
    monitorTable_->setColumnWidth(MonitorTableModel::SelectColumn, 56);
    monitorTable_->setColumnWidth(MonitorTableModel::AddressColumn, 96);
    monitorTable_->setColumnWidth(MonitorTableModel::NameColumn, 240);
    monitorTable_->setColumnWidth(MonitorTableModel::ValueColumn, 100);
    monitorTable_->setColumnWidth(MonitorTableModel::UnitColumn, 100);
    monitorTable_->setColumnWidth(MonitorTableModel::RemarkColumn, 180);
    layout->addWidget(monitorTable_, 1);

    return page;
}

/**
 * @brief 创建故障总表页面。
 * @author mozhengjie
 * @return QWidget* 故障总表页面控件指针。
 */
QWidget *Widget::createFaultPage()
{
    auto *page = createLineFrame(QStringLiteral(
        "QFrame { background: #CDE8B7; border: none; }"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("故障总表"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600; color: #000000;"));
    titleLayout->addWidget(title);
    titleLayout->addStretch(1);
    layout->addLayout(titleLayout);

    faultModel_ = new FaultTableModel(this);
    faultTable_ = new QTableView;
    faultTable_->setModel(faultModel_);
    faultTable_->setAlternatingRowColors(true);
    faultTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    faultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    faultTable_->setStyleSheet(QStringLiteral(
        "QTableView { background: #FFFFFF; alternate-background-color: #F1F1F1; gridline-color: #B8B8B8; }"
        "QHeaderView::section { background: #EFEFEF; border: 1px solid #B8B8B8; padding: 5px; font-weight: 600; color: #000000; }"
        "QTableView::item { padding: 4px; color: #000000; }"));
    configureFixedTableArea(faultTable_);
    faultTable_->setColumnWidth(FaultTableModel::AddressColumn, 110);
    faultTable_->setColumnWidth(FaultTableModel::BitColumn, 80);
    faultTable_->setColumnWidth(FaultTableModel::NameColumn, 260);
    faultTable_->setColumnWidth(FaultTableModel::ValueColumn, 120);
    faultTable_->setColumnWidth(FaultTableModel::RemarkColumn, 220);
    layout->addWidget(faultTable_, 1);

    return page;
}

/**
 * @brief 创建示波器占位页面。
 * @author mozhengjie
 * @return QWidget* 示波器页面控件指针。
 */
QWidget *Widget::createScopePage()
{
    auto *page = createLineFrame(QStringLiteral(
        "QFrame { background: #CDE8B7; border: none; }"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("示波器"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600; color: #000000;"));
    layout->addWidget(title);

    auto *placeholder = new QLabel(QStringLiteral("示波器窗体"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral(
        "QLabel { background: #FFFFFF; border: 1px solid #B8B8B8; color: #000000; font-size: 24px; }"));
    layout->addWidget(placeholder, 1);
    return page;
}

/**
 * @brief 创建底部运行状态栏。
 * @author mozhengjie
 * @return QWidget* 底部状态栏控件指针。
 */
QWidget *Widget::createBottomStatusBar()
{
    auto *bar = createLineFrame(QStringLiteral(
        "QFrame { background: #F9D7DC; border-top: 1px solid #222; }"));
    bar->setFixedHeight(kBottomBarHeight);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(28);

    connectionStatusLabel_ = new QLabel(QStringLiteral("连接状态：断开"));
    servoStateLabel_ = new QLabel(QStringLiteral("伺服状态：未连接"));
    selectedModelLabel_ = new QLabel(QStringLiteral("当前型号：未选择"));
    parameterProgressBar_ = new QProgressBar;
    parameterProgressBar_->setRange(0, 100);
    parameterProgressBar_->setValue(0);
    parameterProgressBar_->setTextVisible(true);
    parameterProgressBar_->setFormat(QStringLiteral("参数进度：空闲"));
    parameterProgressBar_->setMinimumWidth(220);
    parameterProgressBar_->setMaximumWidth(360);
    parameterProgressBar_->setFixedHeight(18);
    parameterProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #FFFFFF; border: 1px solid #9E9E9E; color: #000000; text-align: center; }"
        "QProgressBar::chunk { background: #00B050; }"));
    operationStatusLabel_ = new QLabel(QStringLiteral("操作状态：空闲"));

    layout->addWidget(connectionStatusLabel_);
    layout->addWidget(servoStateLabel_);
    layout->addWidget(selectedModelLabel_);
    layout->addWidget(parameterProgressBar_);
    layout->addStretch(1);
    layout->addWidget(operationStatusLabel_);
    return bar;
}

/**
 * @brief 创建顶部命令按钮。
 * @author mozhengjie
 * @param text 按钮显示文本。
 * @return QToolButton* 顶部命令按钮指针。
 */
QToolButton *Widget::createCommandButton(const QString &text) const
{
    auto *button = new QToolButton;
    button->setText(text);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumWidth(82);
    button->setFixedHeight(34);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid transparent; padding: 4px 8px; font-size: 14px; }"
        "QToolButton:hover { background: #BEE7E3; border: 1px solid #7DAAA6; }"
        "QToolButton:pressed { background: #9FD3CE; }"));
    return button;
}

/**
 * @brief 创建左侧二级操作按钮。
 * @author mozhengjie
 * @param text 按钮显示文本。
 * @return QPushButton* 二级操作按钮指针。
 */
QPushButton *Widget::createSecondaryButton(const QString &text) const
{
    auto *button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(30);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #FFFFFF; border: 1px solid #9E9E9E; padding: 5px 8px; text-align: left; color: #000000; }"
        "QPushButton:hover { background: #E5F2D8; }"
        "QPushButton:pressed { background: #D4E8C5; }"));
    return button;
}

/**
 * @brief 根据当前屏幕可用区域设置初始窗口大小和居中位置。
 * @author mozhengjie
 */
void Widget::applyInitialWindowGeometry()
{
    const QRect availableGeometry = QGuiApplication::primaryScreen()
                                        ? QGuiApplication::primaryScreen()->availableGeometry()
                                        : QRect(0, 0, 1200, 760);
    const QSize preferredSize(1200, 760);
    const int targetWidth = std::max(minimumWidth(),
                                     std::min(preferredSize.width(), availableGeometry.width() - kWindowMargin));
    const int targetHeight = std::max(minimumHeight(),
                                      std::min(preferredSize.height(), availableGeometry.height() - kWindowMargin));

    resize(targetWidth, targetHeight);

    const int x = availableGeometry.x() + (availableGeometry.width() - width()) / 2;
    const int y = availableGeometry.y() + (availableGeometry.height() - height()) / 2;
    move(std::max(availableGeometry.left(), x), std::max(availableGeometry.top(), y));
}

/**
 * @brief 配置表格为固定行高、整体宽度锁定、列宽可拖拽模式。
 * @author mozhengjie
 * @param table 需要配置的表格控件。
 */
void Widget::configureFixedTableArea(QTableView *table) const
{
    if (!table) {
        return;
    }

    table->setCornerButtonEnabled(false);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *horizontalHeader = table->horizontalHeader();
    horizontalHeader->setSectionsClickable(true);
    horizontalHeader->setSectionsMovable(false);
    horizontalHeader->setStretchLastSection(true);
    horizontalHeader->setMinimumSectionSize(36);
    horizontalHeader->setSectionResizeMode(QHeaderView::Interactive);
    connect(horizontalHeader, &QHeaderView::sectionResized, table, [this, table]() {
        constrainTableColumnsToViewport(table);
    });

    auto *verticalHeader = table->verticalHeader();
    verticalHeader->setVisible(false);
    verticalHeader->setSectionsClickable(false);
    verticalHeader->setSectionsMovable(false);
    verticalHeader->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader->setMinimumSectionSize(30);
    verticalHeader->setDefaultSectionSize(30);
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
}

/**
 * @brief 将表格列宽限制在当前可视区域内。
 * @author mozhengjie
 * @param table 需要限制列宽的表格控件。
 */
void Widget::constrainTableColumnsToViewport(QTableView *table) const
{
    if (!table || !table->model() || !table->isVisible()) {
        return;
    }

    auto *header = table->horizontalHeader();
    if (!header || header->count() <= 0 || header->property("constraining").toBool()) {
        return;
    }

    const int viewportWidth = table->viewport() ? table->viewport()->width() : table->width();
    if (viewportWidth <= 0) {
        return;
    }

    int totalWidth = 0;
    for (int section = 0; section < header->count(); ++section) {
        totalWidth += header->sectionSize(section);
    }

    const int overflow = totalWidth - viewportWidth;
    if (overflow <= 0) {
        return;
    }

    header->setProperty("constraining", true);
    int remainingOverflow = overflow;
    for (int section = header->count() - 1; section >= 0 && remainingOverflow > 0; --section) {
        const int currentWidth = header->sectionSize(section);
        const int minimumWidth = header->minimumSectionSize();
        const int reducibleWidth = std::max(0, currentWidth - minimumWidth);
        if (reducibleWidth <= 0) {
            continue;
        }

        const int reduction = std::min(reducibleWidth, remainingOverflow);
        header->resizeSection(section, currentWidth - reduction);
        remainingOverflow -= reduction;
    }
    header->setProperty("constraining", false);
}

/**
 * @brief 扫描 XML 文件夹并刷新型号下拉框。
 * @author mozhengjie
 */
void Widget::scanXmlModelFiles()
{
    if (!modelSelector_) {
        return;
    }

    const QStringList candidateDirectories = {
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("XML")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../XML")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../../XML")),
        QDir::current().absoluteFilePath(QStringLiteral("XML")),
        QDir(QStringLiteral(APP_SOURCE_DIR)).absoluteFilePath(QStringLiteral("XML"))};

    availableConfigs_.clear();
    for (const QString &directoryPath : candidateDirectories) {
        availableConfigs_ = XmlConfigLoader::scanDirectory(QDir::cleanPath(directoryPath));
        if (!availableConfigs_.isEmpty()) {
            break;
        }
    }

    const QSignalBlocker blocker(modelSelector_);
    modelSelector_->clear();
    modelSelector_->addItem(QStringLiteral("请选择型号 XML"), -1);
    for (int index = 0; index < availableConfigs_.size(); ++index) {
        const DeviceConfig &config = availableConfigs_[index];
        const QString displayName = config.productSeries.isEmpty()
                                    ? config.productName
                                    : QStringLiteral("%1 (%2)").arg(config.productName, config.productSeries);
        modelSelector_->addItem(displayName, index);
    }

    if (availableConfigs_.isEmpty()) {
        connectionStatusLabel_->setText(QStringLiteral("连接状态：未找到 XML 型号文件"));
        refreshParameterTableFromConfig();
        refreshMonitorTableFromConfig();
        refreshFaultTableFromConfig();
        return;
    }

    modelSelector_->setCurrentIndex(1);
    loadDeviceConfig(1);
    updateSelectedModelStatus(modelSelector_->currentText());
}

/**
 * @brief 加载指定下拉索引对应的设备配置。
 * @author mozhengjie
 * @param index 型号下拉框索引。
 */
void Widget::loadDeviceConfig(int index)
{
    if (!modelSelector_) {
        return;
    }

    const int configIndex = modelSelector_->itemData(index).toInt();
    if (configIndex < 0 || configIndex >= availableConfigs_.size()) {
        stopFaultPolling();
        latestFaultRegisterValues_.clear();
        currentConfig_ = DeviceConfig{};
        refreshParameterTableFromConfig();
        refreshMonitorTableFromConfig();
        refreshFaultTableFromConfig();
        return;
    }

    stopFaultPolling();
    latestFaultRegisterValues_.clear();
    currentConfig_ = availableConfigs_[configIndex];
    refreshParameterTableFromConfig();
    refreshMonitorTableFromConfig();
    refreshFaultTableFromConfig();

    if (mainStack_) {
        mainStack_->setCurrentIndex(0);
    }

    if (connectionStatusLabel_) {
        connectionStatusLabel_->setText(QStringLiteral("连接状态：已加载 %1，待连接").arg(currentConfig_.productName));
    }
    if (servoStateLabel_) {
        setServoStateDisplay(QStringLiteral("未连接"), false);
    }
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：待连接"));
    }
}

/**
 * @brief 根据当前 XML 配置刷新参数总表。
 * @author mozhengjie
 */
void Widget::refreshParameterTableFromConfig()
{
    if (!parameterModel_) {
        return;
    }

    lastParameterMatchedRow_ = -1;
    lastParameterSearchText_.clear();
    parameterModel_->setRegisters(currentConfig_.registers);
}

/**
 * @brief 根据当前 XML 配置刷新监控总表。
 * @author mozhengjie
 */
void Widget::refreshMonitorTableFromConfig()
{
    if (!monitorModel_) {
        return;
    }

    lastMonitorMatchedRow_ = -1;
    lastMonitorSearchText_.clear();
    QVector<MonitorDefinition> monitorDefinitions;
    monitorDefinitions.reserve(currentConfig_.monitors.size());
    for (const MonitorDefinition &definition : currentConfig_.monitors) {
        if (!isFaultMonitorDefinition(definition)) {
            monitorDefinitions.append(definition);
        }
    }
    monitorModel_->setMonitors(monitorDefinitions);
}

/**
 * @brief 根据当前 XML 配置刷新故障总表。
 * @author mozhengjie
 */
void Widget::refreshFaultTableFromConfig()
{
    if (!faultModel_) {
        return;
    }

    QVector<MonitorDefinition> faultDefinitions;
    faultDefinitions.reserve(currentConfig_.monitors.size());
    for (const MonitorDefinition &definition : currentConfig_.monitors) {
        if (isFaultMonitorDefinition(definition)) {
            faultDefinitions.append(definition);
        }
    }
    faultModel_->setFaults(faultDefinitions);
}

/**
 * @brief 根据左侧导航当前项切换主显示页面和二级操作面板。
 * @author mozhengjie
 */
void Widget::selectMainPageForCurrentTreeItem()
{
    if (!navigationTree_ || !mainStack_ || !navigationTree_->currentItem()) {
        return;
    }

    const int pageIndex = navigationTree_->currentItem()->data(0, Qt::UserRole).toInt();
    if (pageIndex == kJogActionIndex || pageIndex == kPositionActionIndex) {
        stopFaultPolling();
        return;
    }
    if (pageIndex != kFaultPageIndex) {
        stopFaultPolling();
    }
    if (pageIndex >= 0 && pageIndex < mainStack_->count()) {
        mainStack_->setCurrentIndex(pageIndex);
    }
    if (secondaryActionStack_ && pageIndex >= 0 && pageIndex < secondaryActionStack_->count()) {
        secondaryActionStack_->setCurrentIndex(pageIndex);
    }
}

/**
 * @brief 响应左侧导航点击并处理故障总表轮询开关。
 * @author mozhengjie
 * @param item 被点击的导航项。
 * @param column 被点击的列号。
 */
void Widget::handleNavigationItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (!item || !mainStack_) {
        return;
    }

    const int pageIndex = item->data(0, Qt::UserRole).toInt();
    const bool faultPageClicked = pageIndex == kFaultPageIndex;
    const bool faultPollingActive = faultPollTimer_ && faultPollTimer_->isActive();

    if (pageIndex == kJogActionIndex) {
        stopFaultPolling();
        showToolPlaceholderDialog(QStringLiteral("点动运行"));
        return;
    }

    if (pageIndex == kPositionActionIndex) {
        stopFaultPolling();
        showToolPlaceholderDialog(QStringLiteral("定位运行"));
        return;
    }

    if (!faultPageClicked) {
        stopFaultPolling();
    }

    if (pageIndex >= 0 && pageIndex < mainStack_->count()) {
        mainStack_->setCurrentIndex(pageIndex);
    }
    if (secondaryActionStack_ && pageIndex >= 0 && pageIndex < secondaryActionStack_->count()) {
        secondaryActionStack_->setCurrentIndex(pageIndex);
    }

    if (!faultPageClicked) {
        return;
    }

    if (faultPollingActive) {
        stopFaultPolling();
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：故障轮询已停止"));
        }
        return;
    }

    startFaultPolling();
}

/**
 * @brief 显示左侧工具入口的独立占位弹窗。
 * @author mozhengjie
 * @param title 弹窗标题。
 */
void Widget::showToolPlaceholderDialog(const QString &title)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setMinimumSize(360, 220);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #FFFFFF; }"
        "QLabel { background: #FFFFFF; color: #000000; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #A8A8A8; padding: 5px 16px; color: #000000; }"
        "QPushButton:hover { background: #F3F3F3; }"
        "QPushButton:pressed { background: #E8E8E8; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *placeholder = new QLabel(QStringLiteral("%1窗体").arg(title));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    layout->addWidget(placeholder, 1);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"));
    closeButton->setFixedWidth(90);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    dialog.exec();
}

/**
 * @brief 切换保存参数下拉菜单显示状态。
 * @author mozhengjie
 */
void Widget::toggleSaveParameterMenu()
{
    if (!saveParameterButton_) {
        return;
    }

    if (!saveParameterMenu_) {
        saveParameterMenu_ = new QMenu(this);
        saveParameterMenu_->setStyleSheet(QStringLiteral(
            "QMenu { background: #FFFFFF; border: 1px solid #A8A8A8; color: #000000; }"
            "QMenu::item { background: #FFFFFF; color: #000000; padding: 6px 24px; }"
            "QMenu::item:selected { background: #E5F2D8; color: #000000; }"));

        QAction *userParameterAction = saveParameterMenu_->addAction(QStringLiteral("用户参数"));
        QAction *motorParameterAction = saveParameterMenu_->addAction(QStringLiteral("电机参数"));
        connect(userParameterAction, &QAction::triggered, this, [this]() { sendSaveParameterCommand(false); });
        connect(motorParameterAction, &QAction::triggered, this, [this]() { sendSaveParameterCommand(true); });
    }

    if (saveParameterMenu_->isVisible()) {
        saveParameterMenu_->hide();
        return;
    }

    saveParameterMenu_->popup(saveParameterButton_->mapToGlobal(QPoint(0, saveParameterButton_->height())));
}

/**
 * @brief 发送保存参数命令。
 * @author mozhengjie
 * @param motorParameter 是否保存电机参数，false 表示用户参数。
 */
void Widget::sendSaveParameterCommand(bool motorParameter)
{
    if (saveParameterMenu_) {
        saveParameterMenu_->hide();
    }

    const quint16 commandValue = motorParameter ? 99 : 1;
    const QString requestPrefix = motorParameter ? QStringLiteral("save-motor") : QStringLiteral("save-user");
    if (writeControlRegister(90, commandValue, requestPrefix) && operationStatusLabel_) {
        operationStatusLabel_->setText(motorParameter
                                           ? QStringLiteral("操作状态：正在保存电机参数")
                                           : QStringLiteral("操作状态：正在保存用户参数"));
    }
}

/**
 * @brief 发送故障复位命令。
 * @author mozhengjie
 */
void Widget::sendFaultResetCommand()
{
    if (writeControlRegister(45, 1, QStringLiteral("fault-reset")) && operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：正在执行故障复位"));
    }
}

/**
 * @brief 读取伺服状态后执行恢复出厂流程。
 * @author mozhengjie
 */
void Widget::requestFactoryResetCommand()
{
    if (requestServoStateCheck(QStringLiteral("factory-reset-check")) && operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：正在校验恢复出厂条件"));
    }
}

/**
 * @brief 读取伺服状态后执行电机复位流程。
 * @author mozhengjie
 */
void Widget::requestMotorResetCommand()
{
    if (requestServoStateCheck(QStringLiteral("motor-reset-check")) && operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：正在校验电机复位条件"));
    }
}

/**
 * @brief 写入单个控制寄存器。
 * @author mozhengjie
 * @param address 寄存器地址。
 * @param value 写入值。
 * @param requestPrefix 请求标识前缀。
 * @return bool 请求成功发出返回 true。
 */
bool Widget::writeControlRegister(int address, quint16 value, const QString &requestPrefix)
{
    if (!ensureModbusReady(commandOperationName(requestPrefix))) {
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2").arg(requestPrefix).arg(++modbusRequestSerial_);
    const bool requestSent = modbusClient_->writeHoldingRegisters(address, QVector<quint16>{value}, requestTag);
    if (requestSent) {
        ++activeHighPriorityModbusRequests_;
    }
    return requestSent;
}

/**
 * @brief 读取寄存器 187 以校验伺服运行状态。
 * @author mozhengjie
 * @param requestPrefix 请求标识前缀。
 * @return bool 请求成功发出返回 true。
 */
bool Widget::requestServoStateCheck(const QString &requestPrefix)
{
    if (!ensureModbusReady(commandOperationName(requestPrefix))) {
        return false;
    }

    const QString requestTag = QStringLiteral("%1:%2").arg(requestPrefix).arg(++modbusRequestSerial_);
    const bool requestSent = modbusClient_->readHoldingRegisters(187, 1, requestTag);
    if (requestSent) {
        ++activeHighPriorityModbusRequests_;
    }
    return requestSent;
}

/**
 * @brief 显示 2 秒自动关闭提示框。
 * @author mozhengjie
 * @param message 提示文本。
 */
void Widget::showAutoCloseMessage(const QString &message)
{
    auto *messageBox = new QMessageBox(QMessageBox::Information,
                                       QStringLiteral("提示"),
                                       message,
                                       QMessageBox::Ok,
                                       this);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setModal(false);
    QTimer::singleShot(2000, messageBox, &QMessageBox::accept);
    messageBox->show();
}

/**
 * @brief 更新底部状态栏中的当前电机型号文本。
 * @author mozhengjie
 * @param modelName 当前下拉框显示的型号名称。
 */
void Widget::updateSelectedModelStatus(const QString &modelName)
{
    if (!selectedModelLabel_) {
        return;
    }

    bool ok = false;
    const int configIndex = modelSelector_ ? modelSelector_->currentData().toInt(&ok) : -1;
    const bool hasModel = ok && configIndex >= 0;
    selectedModelLabel_->setText(hasModel
                                     ? QStringLiteral("当前型号：%1").arg(modelName)
                                     : QStringLiteral("当前型号：未选择"));
}

/**
 * @brief 显示通讯设置对话框并保存用户配置。
 * @author mozhengjie
 */
void Widget::showCommunicationSettings()
{
    if (modbusClient_ && modbusClient_->isConnected()) {
        QMessageBox::information(this, QStringLiteral("通讯设置"),
                                 QStringLiteral("请先断开连接，再修改通讯参数。"));
        return;
    }

    CommunicationSettingsDialog dialog(communicationConfig_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    communicationConfig_ = dialog.settings();
    if (connectionStatusLabel_) {
        connectionStatusLabel_->setText(QStringLiteral("连接状态：通讯参数已设置，%1")
                                            .arg(communicationConfig_.summaryText()));
    }
}

/**
 * @brief 根据当前连接状态执行连接或断开操作。
 * @author mozhengjie
 */
void Widget::toggleModbusConnection()
{
    if (!modbusClient_) {
        return;
    }

    if (modbusClient_->isConnected()) {
        modbusClient_->closeDevice();
        return;
    }

    if (!currentConfig_.isValid()) {
        QMessageBox::warning(this, QStringLiteral("连接失败"), QStringLiteral("请先选择电机型号 XML。"));
        return;
    }

    if (communicationConfig_.portName.trimmed().isEmpty()) {
        showCommunicationSettings();
        if (communicationConfig_.portName.trimmed().isEmpty()) {
            if (connectionStatusLabel_) {
                connectionStatusLabel_->setText(QStringLiteral("连接状态：请先完成通讯设置"));
            }
            return;
        }
    }

    modbusClient_->openDevice(communicationConfig_);
}

/**
 * @brief 刷新 Modbus 连接状态显示。
 * @author mozhengjie
 * @param connected 是否已连接。
 * @param statusText 状态文本。
 */
void Widget::handleConnectionStatusChanged(bool connected, const QString &statusText)
{
    if (connectionStatusLabel_) {
        connectionStatusLabel_->setText(QStringLiteral("连接状态：%1").arg(statusText));
    }
    if (connectionToggleButton_) {
        connectionToggleButton_->setText(connected ? QStringLiteral("断开连接") : QStringLiteral("连接/断开"));
    }

    if (connected) {
        setServoStateDisplay(QStringLiteral("等待状态读取"), false);
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：连接成功，自动上传全部参数"));
        }
        if (mainStack_) {
            mainStack_->setCurrentIndex(0);
        }
        if (servoStateTimer_) {
            servoStateTimer_->stop();
        }
        startParameterUpload(false, true);
        return;
    }

    if (statusText.contains(QStringLiteral("正在连接")) || statusText.contains(QStringLiteral("正在断开"))) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：%1").arg(statusText));
        }
        return;
    }

    setServoStateDisplay(QStringLiteral("未连接"), false);
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：已断开"));
    }
    if (monitorTimer_ && monitorTimer_->isActive()) {
        monitorTimer_->stop();
    }
    if (servoStateTimer_ && servoStateTimer_->isActive()) {
        servoStateTimer_->stop();
    }
    stopFaultPolling();
    pendingParameterUploadQueue_.clear();
    pendingParameterDownloadQueue_.clear();
    pendingParameterReadMap_.clear();
    pendingMonitorReadMap_.clear();
    pendingFaultReadMap_.clear();
    latestFaultRegisterValues_.clear();
    activeHighPriorityModbusRequests_ = 0;
    parameterUploadIsAutomatic_ = false;
    updateParameterTransferProgress(0, 0, QStringLiteral("参数进度"));
    if (monitorToggleButton_) {
        monitorToggleButton_->setProperty("monitoring", false);
        monitorToggleButton_->setText(QStringLiteral("启动监控"));
    }
}

/**
 * @brief 显示 Modbus 错误信息。
 * @author mozhengjie
 * @param errorText 错误文本。
 */
void Widget::handleModbusError(const QString &errorText)
{
    if (connectionStatusLabel_) {
        connectionStatusLabel_->setText(QStringLiteral("连接状态：错误 - %1").arg(errorText));
    }
}

/**
 * @brief 刷新参数写入结果状态。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param success 写入是否成功。
 * @param message 写入结果描述。
 * @param requestTag Modbus 写入请求标识。
 */
void Widget::handleParameterWriteCompleted(int startAddress,
                                           bool success,
                                           const QString &message,
                                           const QString &requestTag)
{
    if (requestTag.startsWith(QStringLiteral("parameter-single:"))
        || requestTag.startsWith(QStringLiteral("parameter-download:"))
        || requestTag.startsWith(QStringLiteral("fault-reset:"))
        || requestTag.startsWith(QStringLiteral("save-user:"))
        || requestTag.startsWith(QStringLiteral("save-motor:"))
        || requestTag.startsWith(QStringLiteral("factory-reset:"))
        || requestTag.startsWith(QStringLiteral("motor-reset:"))) {
        activeHighPriorityModbusRequests_ = std::max(0, activeHighPriorityModbusRequests_ - 1);
    }

    if (requestTag.startsWith(QStringLiteral("fault-reset:"))) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                               ? QStringLiteral("操作状态：故障复位完成")
                                               : QStringLiteral("操作状态：故障复位失败，%1").arg(message));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("save-user:"))
        || requestTag.startsWith(QStringLiteral("save-motor:"))) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                               ? QStringLiteral("操作状态：参数保存完成")
                                               : QStringLiteral("操作状态：参数保存失败，%1").arg(message));
        }
        if (success) {
            showAutoCloseMessage(QStringLiteral("保存成功！"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("factory-reset:"))) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                               ? QStringLiteral("操作状态：恢复出厂命令已下发")
                                               : QStringLiteral("操作状态：恢复出厂失败，%1").arg(message));
        }
        if (success) {
            showAutoCloseMessage(QStringLiteral("4S后再复位电机。"));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("motor-reset:"))) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                               ? QStringLiteral("操作状态：电机复位命令已下发")
                                               : QStringLiteral("操作状态：电机复位失败，%1").arg(message));
        }
        if (success) {
            showAutoCloseMessage(QStringLiteral("复位成功！"));
        }
        return;
    }

    if (parameterModel_) {
        parameterModel_->markParameterSendState(startAddress, success);
    }

    if (requestTag.startsWith(QStringLiteral("parameter-download:"))) {
        ++parameterDownloadFinished_;
        updateParameterTransferProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                           ? QStringLiteral("操作状态：下载参数 %1/%2 完成")
                                                 .arg(parameterDownloadFinished_)
                                                 .arg(parameterDownloadTotal_)
                                           : QStringLiteral("操作状态：下载参数 %1 失败，%2").arg(startAddress).arg(message));
        }
        startNextParameterDownload();
        return;
    }

    if (!operationStatusLabel_) {
        return;
    }

    operationStatusLabel_->setText(success
                                   ? QStringLiteral("操作状态：寄存器 %1 下发完成").arg(startAddress)
                                   : QStringLiteral("操作状态：寄存器 %1 下发失败，%2").arg(startAddress).arg(message));
}

/**
 * @brief 将已编辑参数值转换为寄存器并下发。
 * @author mozhengjie
 * @param definition 参数定义。
 * @param newValue 新参数值。
 * @return bool 请求成功发出返回 true。
 */
bool Widget::writeParameterToServo(const RegisterDefinition &definition, const QString &newValue)
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        return false;
    }

    bool addressOk = false;
    const int startAddress = definition.modbusAddr.toInt(&addressOk);
    if (!addressOk) {
        handleModbusError(QStringLiteral("参数地址 %1 无法转换为 Modbus 地址").arg(definition.modbusAddr));
        return false;
    }

    QVector<quint16> registers;
    if (!convertParameterValueToRegisters(definition, newValue, &registers)) {
        handleModbusError(QStringLiteral("参数值 %1 无法转换为 16 位寄存器").arg(newValue));
        return false;
    }

    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：正在下发寄存器 %1").arg(startAddress));
    }
    const QString requestTag = QStringLiteral("parameter-single:%1:%2").arg(startAddress).arg(++modbusRequestSerial_);
    const bool requestSent = modbusClient_->writeHoldingRegisters(startAddress, registers, requestTag);
    if (requestSent) {
        ++activeHighPriorityModbusRequests_;
    }
    return requestSent;
}

/**
 * @brief 处理 Modbus 读取结果并分发给参数表或监控表。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param values 读取到的 16 位寄存器值。
 * @param success 读取是否成功。
 * @param message 读取结果描述。
 * @param requestTag 请求标识。
 */
void Widget::handleRegisterReadCompleted(int startAddress,
                                         const QVector<quint16> &values,
                                         bool success,
                                         const QString &message,
                                         const QString &requestTag)
{
    if (requestTag.startsWith(QStringLiteral("parameter-upload:"))
        || requestTag.startsWith(QStringLiteral("monitor:"))
        || requestTag.startsWith(QStringLiteral("fault-page:"))
        || requestTag.startsWith(QStringLiteral("factory-reset-check:"))
        || requestTag.startsWith(QStringLiteral("motor-reset-check:"))) {
        activeHighPriorityModbusRequests_ = std::max(0, activeHighPriorityModbusRequests_ - 1);
    }

    if (requestTag.startsWith(QStringLiteral("factory-reset-check:"))
        || requestTag.startsWith(QStringLiteral("motor-reset-check:"))) {
        if (!success || values.isEmpty()) {
            if (operationStatusLabel_) {
                operationStatusLabel_->setText(QStringLiteral("操作状态：伺服状态读取失败，%1").arg(message));
            }
            return;
        }

        if (values.first() == kServoRunStateValue) {
            showAutoCloseMessage(QStringLiteral("电机使能中，请先断使能！"));
            if (operationStatusLabel_) {
                operationStatusLabel_->setText(QStringLiteral("操作状态：电机使能中，命令未下发"));
            }
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
            if (!servoAlarmStateActive_) {
                setServoStateDisplay(QStringLiteral("伺服报警（故障读取中）"), true);
            }
        } else {
            setServoStateDisplay(servoSystemStateText(stateValue), false);
        }
        if (alarmActive && !hasHighPriorityModbusWork()) {
            requestFaultRegisters(QStringLiteral("servo-fault"), false);
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("fault-page:"))
        || requestTag.startsWith(QStringLiteral("servo-fault:"))) {
        pendingFaultReadMap_.take(requestTag);
        if (success) {
            updateFaultRegisterSnapshot(startAddress, values);
            if (requestTag.startsWith(QStringLiteral("fault-page:")) && operationStatusLabel_) {
                operationStatusLabel_->setText(QStringLiteral("操作状态：故障寄存器 %1 已刷新").arg(startAddress));
            }
        } else if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：故障寄存器 %1 读取失败，%2").arg(startAddress).arg(message));
        }
        return;
    }

    if (requestTag.startsWith(QStringLiteral("parameter-upload:"))) {
        const RegisterDefinition definition = pendingParameterReadMap_.take(requestTag);
        ++parameterUploadFinished_;
        updateParameterTransferProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                                    ? QStringLiteral("自动上传")
                                                                                    : QStringLiteral("上传"));
        if (success && parameterModel_) {
            parameterModel_->updateRegisterValue(startAddress, parameterValueFromRegisters(definition, values), true);
        }
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(success
                                           ? QStringLiteral("操作状态：上传参数 %1/%2 完成")
                                                 .arg(parameterUploadFinished_)
                                                 .arg(parameterUploadTotal_)
                                           : QStringLiteral("操作状态：上传参数 %1 失败，%2").arg(startAddress).arg(message));
        }
        startNextParameterUpload();
        return;
    }

    if (requestTag.startsWith(QStringLiteral("monitor:"))) {
        const MonitorDefinition definition = pendingMonitorReadMap_.take(requestTag);
        if (success && monitorModel_) {
            monitorModel_->updateMonitorValue(definition, monitorValueFromRegisters(definition, values));
        } else if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：监控地址 %1 读取失败，%2").arg(startAddress).arg(message));
        }
    }
}

/**
 * @brief 执行参数总表二级操作。
 * @author mozhengjie
 * @param action 操作名称。
 */
void Widget::handleParameterAction(const QString &action)
{
    if (action == QStringLiteral("上传全部")) {
        startParameterUpload(false);
    } else if (action == QStringLiteral("上传勾选")) {
        startParameterUpload(true);
    } else if (action == QStringLiteral("下载全部")) {
        startParameterDownload(false);
    } else if (action == QStringLiteral("下载勾选")) {
        startParameterDownload(true);
    }
}

/**
 * @brief 启动参数上传队列。
 * @author mozhengjie
 * @param checkedOnly 是否只上传勾选参数。
 * @param automatic 是否为连接成功后的自动上传流程。
 */
void Widget::startParameterUpload(bool checkedOnly, bool automatic)
{
    if (!ensureModbusReady(checkedOnly ? QStringLiteral("上传勾选") : QStringLiteral("上传全部"))
        || !parameterModel_) {
        return;
    }

    parameterUploadIsAutomatic_ = automatic;
    if (automatic && servoStateTimer_) {
        servoStateTimer_->stop();
    }

    const QVector<RegisterDefinition> source = checkedOnly ? parameterModel_->checkedRegisters()
                                                           : parameterModel_->allRegisters();
    pendingParameterUploadQueue_.clear();
    pendingParameterReadMap_.clear();
    for (const RegisterDefinition &definition : source) {
        if (isTransferableRegister(definition)) {
            pendingParameterUploadQueue_.append(definition);
        }
    }

    parameterUploadTotal_ = pendingParameterUploadQueue_.size();
    parameterUploadFinished_ = 0;
    updateParameterTransferProgress(0, parameterUploadTotal_, automatic
                                                           ? QStringLiteral("自动上传")
                                                           : QStringLiteral("上传"));
    if (parameterUploadTotal_ == 0) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(automatic ? QStringLiteral("操作状态：自动上传无可上传参数")
                                                     : QStringLiteral("操作状态：没有可上传参数"));
        }
        parameterUploadIsAutomatic_ = false;
        if (automatic) {
            startServoStatePolling();
        }
        return;
    }

    if (operationStatusLabel_) {
        operationStatusLabel_->setText(automatic
                                           ? QStringLiteral("操作状态：自动上传 %1 个参数")
                                                 .arg(parameterUploadTotal_)
                                           : QStringLiteral("操作状态：开始上传 %1 个参数")
                                                 .arg(parameterUploadTotal_));
    }
    startNextParameterUpload();
}

/**
 * @brief 上传队列读取下一个参数。
 * @author mozhengjie
 */
void Widget::startNextParameterUpload()
{
    while (!pendingParameterUploadQueue_.isEmpty()) {
        const RegisterDefinition definition = pendingParameterUploadQueue_.takeFirst();
        int startAddress = 0;
        if (!parseSingleAddress(definition.modbusAddr, &startAddress)) {
            ++parameterUploadFinished_;
            updateParameterTransferProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                                        ? QStringLiteral("自动上传")
                                                                                        : QStringLiteral("上传"));
            continue;
        }

        const QString requestTag = QStringLiteral("parameter-upload:%1:%2")
                                       .arg(startAddress)
                                       .arg(++modbusRequestSerial_);
        pendingParameterReadMap_.insert(requestTag, definition);
        if (modbusClient_->readHoldingRegisters(startAddress, registerCountForParameter(definition), requestTag)) {
            ++activeHighPriorityModbusRequests_;
            return;
        }

        pendingParameterReadMap_.remove(requestTag);
        ++parameterUploadFinished_;
        updateParameterTransferProgress(parameterUploadFinished_, parameterUploadTotal_, parameterUploadIsAutomatic_
                                                                                    ? QStringLiteral("自动上传")
                                                                                    : QStringLiteral("上传"));
    }

    const bool shouldStartServoStatePolling = parameterUploadIsAutomatic_
                                             || (modbusClient_ && modbusClient_->isConnected()
                                                 && servoStateTimer_ && !servoStateTimer_->isActive());
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(parameterUploadIsAutomatic_
                                           ? QStringLiteral("操作状态：自动上传完成，共 %1 个")
                                                 .arg(parameterUploadFinished_)
                                           : QStringLiteral("操作状态：参数上传完成，共 %1 个")
                                                 .arg(parameterUploadFinished_));
    }
    parameterUploadIsAutomatic_ = false;
    if (shouldStartServoStatePolling) {
        startServoStatePolling();
    }
}

/**
 * @brief 启动参数下载队列。
 * @author mozhengjie
 * @param checkedOnly 是否只下载勾选参数。
 */
void Widget::startParameterDownload(bool checkedOnly)
{
    if (!ensureModbusReady(checkedOnly ? QStringLiteral("下载勾选") : QStringLiteral("下载全部"))
        || !parameterModel_) {
        return;
    }

    const QVector<RegisterDefinition> source = checkedOnly ? parameterModel_->checkedRegisters()
                                                           : parameterModel_->allRegisters();
    pendingParameterDownloadQueue_.clear();
    for (const RegisterDefinition &definition : source) {
        if (isTransferableRegister(definition)
            && definition.rwAttribution.trimmed().compare(QStringLiteral("RW"), Qt::CaseInsensitive) == 0) {
            pendingParameterDownloadQueue_.append(definition);
        }
    }

    parameterDownloadTotal_ = pendingParameterDownloadQueue_.size();
    parameterDownloadFinished_ = 0;
    updateParameterTransferProgress(0, parameterDownloadTotal_, QStringLiteral("下载"));
    if (parameterDownloadTotal_ == 0) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：没有可下载参数"));
        }
        return;
    }

    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：开始下载 %1 个参数").arg(parameterDownloadTotal_));
    }
    startNextParameterDownload();
}

/**
 * @brief 下载队列写入下一个参数。
 * @author mozhengjie
 */
void Widget::startNextParameterDownload()
{
    while (!pendingParameterDownloadQueue_.isEmpty()) {
        const RegisterDefinition definition = pendingParameterDownloadQueue_.takeFirst();
        int startAddress = 0;
        QVector<quint16> registers;
        if (!parseSingleAddress(definition.modbusAddr, &startAddress)
            || !convertParameterValueToRegisters(definition, definition.parameter, &registers)) {
            ++parameterDownloadFinished_;
            updateParameterTransferProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
            continue;
        }

        const QString requestTag = QStringLiteral("parameter-download:%1:%2")
                                       .arg(startAddress)
                                       .arg(++modbusRequestSerial_);
        if (modbusClient_->writeHoldingRegisters(startAddress, registers, requestTag)) {
            ++activeHighPriorityModbusRequests_;
            return;
        }
        ++parameterDownloadFinished_;
        updateParameterTransferProgress(parameterDownloadFinished_, parameterDownloadTotal_, QStringLiteral("下载"));
    }

    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：参数下载完成，共 %1 个").arg(parameterDownloadFinished_));
    }
}

/**
 * @brief 启动或关闭监控轮询。
 * @author mozhengjie
 */
void Widget::toggleMonitorPolling()
{
    if (!monitorTimer_ || !monitorToggleButton_) {
        return;
    }

    if (monitorTimer_->isActive()) {
        monitorTimer_->stop();
        monitorToggleButton_->setProperty("monitoring", false);
        monitorToggleButton_->setText(QStringLiteral("启动监控"));
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：监控已关闭"));
        }
        return;
    }

    if (!ensureModbusReady(QStringLiteral("启动监控")) || !monitorModel_) {
        return;
    }

    if (monitorModel_->checkedMonitors().isEmpty()) {
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：请先勾选监控项"));
        }
        return;
    }

    const int intervalMs = monitorIntervalSpinBox_ ? monitorIntervalSpinBox_->value() : 500;
    monitorTimer_->start(intervalMs);
    monitorToggleButton_->setProperty("monitoring", true);
    monitorToggleButton_->setText(QStringLiteral("关闭监控"));
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：监控中，间隔 %1 ms").arg(intervalMs));
    }
    pollSelectedMonitors();
}

/**
 * @brief 读取当前勾选监控项。
 * @author mozhengjie
 */
void Widget::pollSelectedMonitors()
{
    if (!modbusClient_ || !modbusClient_->isConnected() || !monitorModel_) {
        if (monitorTimer_) {
            monitorTimer_->stop();
        }
        return;
    }

    const QVector<MonitorDefinition> monitors = monitorModel_->checkedMonitors();
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
            continue;
        }
        ++activeHighPriorityModbusRequests_;
    }
}

/**
 * @brief 启动故障总表轮询。
 * @author mozhengjie
 */
void Widget::startFaultPolling()
{
    if (!faultPollTimer_) {
        return;
    }

    if (!ensureModbusReady(QStringLiteral("故障轮询"))) {
        return;
    }

    faultPollTimer_->start(kFaultPollIntervalMs);
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：故障轮询中，间隔 %1 ms").arg(kFaultPollIntervalMs));
    }
    pollFaultRegisters();
}

/**
 * @brief 停止故障总表轮询。
 * @author mozhengjie
 */
void Widget::stopFaultPolling()
{
    if (faultPollTimer_ && faultPollTimer_->isActive()) {
        faultPollTimer_->stop();
    }
}

/**
 * @brief 轮询读取故障寄存器 150 和 390。
 * @author mozhengjie
 */
void Widget::pollFaultRegisters()
{
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        stopFaultPolling();
        return;
    }

    if (!pendingFaultReadMap_.isEmpty()) {
        return;
    }

    requestFaultRegisters(QStringLiteral("fault-page"), true);
}

/**
 * @brief 发起故障寄存器读取请求。
 * @author mozhengjie
 * @param requestPrefix 请求标识前缀。
 * @param highPriority 是否计入用户高优先级 Modbus 操作。
 */
void Widget::requestFaultRegisters(const QString &requestPrefix, bool highPriority)
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
            continue;
        }

        if (highPriority) {
            ++activeHighPriorityModbusRequests_;
        }
    }
}

/**
 * @brief 根据故障寄存器原始值刷新故障表和伺服报警显示。
 * @author mozhengjie
 * @param startAddress 寄存器地址。
 * @param values 读取到的寄存器值。
 */
void Widget::updateFaultRegisterSnapshot(int startAddress, const QVector<quint16> &values)
{
    if (values.isEmpty()) {
        return;
    }

    const quint16 registerValue = values.first();
    latestFaultRegisterValues_.insert(startAddress, registerValue);
    if (faultModel_) {
        faultModel_->updateFaultRegisterValue(startAddress, registerValue);
    }
    refreshServoAlarmFaultText();
}

/**
 * @brief 设置底部伺服状态文本和报警底色。
 * @author mozhengjie
 * @param stateText 状态文本。
 * @param alarmActive 是否处于报警状态。
 */
void Widget::setServoStateDisplay(const QString &stateText, bool alarmActive)
{
    if (!servoStateLabel_) {
        servoAlarmStateActive_ = alarmActive;
        return;
    }

    const QString displayText = QStringLiteral("伺服状态：%1").arg(stateText);
    const QString displayStyle = alarmActive
                                     ? QStringLiteral("QLabel { background: #FF4D4F; color: #000000; padding: 2px 6px; }")
                                     : QStringLiteral("QLabel { background: transparent; color: #000000; padding: 0; }");
    if (servoAlarmStateActive_ == alarmActive
        && servoStateLabel_->text() == displayText
        && servoStateLabel_->styleSheet() == displayStyle) {
        return;
    }

    servoAlarmStateActive_ = alarmActive;
    servoStateLabel_->setText(displayText);
    servoStateLabel_->setStyleSheet(displayStyle);
}

/**
 * @brief 使用当前故障 bit 列表刷新伺服报警状态附加说明。
 * @author mozhengjie
 */
void Widget::refreshServoAlarmFaultText()
{
    if (!servoAlarmStateActive_ || !faultModel_) {
        return;
    }

    const QStringList activeFaults = faultModel_->activeFaultNames();
    const QString faultText = activeFaults.isEmpty()
                                  ? QStringLiteral("未解析到故障位")
                                  : activeFaults.join(QStringLiteral("、"));
    setServoStateDisplay(QStringLiteral("伺服报警（%1）").arg(faultText), true);
}

/**
 * @brief 低优先级读取伺服系统状态寄存器。
 * @author mozhengjie
 */
void Widget::pollServoSystemState()
{
    if (!modbusClient_ || !modbusClient_->isConnected() || hasHighPriorityModbusWork()) {
        return;
    }

    const QString requestTag = QStringLiteral("servo-state:%1").arg(++modbusRequestSerial_);
    if (!modbusClient_->readHoldingRegisters(187, 1, requestTag) && servoStateLabel_) {
        setServoStateDisplay(QStringLiteral("状态读取失败"), false);
    }
}

/**
 * @brief 启动伺服系统状态轮询定时器。
 * @author mozhengjie
 */
void Widget::startServoStatePolling()
{
    if (!modbusClient_ || !modbusClient_->isConnected() || !servoStateTimer_) {
        return;
    }

    if (!servoStateTimer_->isActive()) {
        servoStateTimer_->start(4000);
    }
    pollServoSystemState();
}

/**
 * @brief 判断当前是否存在高优先级 Modbus 操作。
 * @author mozhengjie
 * @return bool 存在用户读写、批量传输或监控读取时返回 true。
 */
bool Widget::hasHighPriorityModbusWork() const
{
    return activeHighPriorityModbusRequests_ > 0
           || !pendingParameterUploadQueue_.isEmpty()
           || !pendingParameterDownloadQueue_.isEmpty()
           || !pendingParameterReadMap_.isEmpty()
           || !pendingMonitorReadMap_.isEmpty()
           || !pendingFaultReadMap_.isEmpty();
}

/**
 * @brief 判断当前是否具备执行 Modbus 操作的条件。
 * @author mozhengjie
 * @param operationName 操作名称。
 * @return bool 可执行返回 true。
 */
bool Widget::ensureModbusReady(const QString &operationName)
{
    if (!currentConfig_.isValid()) {
        if (connectionStatusLabel_) {
            connectionStatusLabel_->setText(QStringLiteral("连接状态：%1失败，请先选择电机型号 XML").arg(operationName));
        }
        return false;
    }
    if (!modbusClient_ || !modbusClient_->isConnected()) {
        if (connectionStatusLabel_) {
            connectionStatusLabel_->setText(QStringLiteral("连接状态：%1失败，请先连接伺服电机").arg(operationName));
        }
        return false;
    }
    return true;
}

/**
 * @brief 搜索参数总表功能说明并循环定位匹配行。
 * @author mozhengjie
 */
void Widget::searchParameterTable()
{
    if (!parameterSearchEdit_) {
        return;
    }

    const QString keyword = parameterSearchEdit_->text().trimmed();
    if (keyword != lastParameterSearchText_) {
        lastParameterMatchedRow_ = -1;
        lastParameterSearchText_ = keyword;
    }

    if (!searchTableAndScroll(parameterTable_, ParameterTableModel::FunctionColumn, keyword, &lastParameterMatchedRow_)
        && operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：参数总表未找到 %1").arg(keyword));
    }
}

/**
 * @brief 搜索监控总表监控名称并循环定位匹配行。
 * @author mozhengjie
 */
void Widget::searchMonitorTable()
{
    if (!monitorSearchEdit_) {
        return;
    }

    const QString keyword = monitorSearchEdit_->text().trimmed();
    if (keyword != lastMonitorSearchText_) {
        lastMonitorMatchedRow_ = -1;
        lastMonitorSearchText_ = keyword;
    }

    if (!searchTableAndScroll(monitorTable_, MonitorTableModel::NameColumn, keyword, &lastMonitorMatchedRow_)
        && operationStatusLabel_) {
        operationStatusLabel_->setText(QStringLiteral("操作状态：监控总表未找到 %1").arg(keyword));
    }
}

/**
 * @brief 在指定表格列中循环搜索并滚动定位到匹配行。
 * @author mozhengjie
 * @param table 目标表格。
 * @param textColumn 搜索列号。
 * @param keyword 搜索关键字。
 * @param lastMatchedRow 上一次匹配行号。
 * @return bool 找到匹配行返回 true。
 */
bool Widget::searchTableAndScroll(QTableView *table, int textColumn, const QString &keyword, int *lastMatchedRow)
{
    if (!table || !table->model() || !lastMatchedRow || keyword.trimmed().isEmpty()) {
        return false;
    }

    QAbstractItemModel *model = table->model();
    const int rowCount = model->rowCount();
    if (rowCount <= 0) {
        return false;
    }

    const int startRow = (*lastMatchedRow >= 0 && *lastMatchedRow < rowCount) ? *lastMatchedRow + 1 : 0;
    for (int offset = 0; offset < rowCount; ++offset) {
        const int row = (startRow + offset) % rowCount;
        const QModelIndex index = model->index(row, textColumn);
        const QString cellText = model->data(index, Qt::DisplayRole).toString();
        if (!cellText.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }

        *lastMatchedRow = row;
        table->setCurrentIndex(index);
        table->selectRow(row);
        table->scrollTo(index, QAbstractItemView::PositionAtTop);
        if (operationStatusLabel_) {
            operationStatusLabel_->setText(QStringLiteral("操作状态：已定位到第 %1 行").arg(row + 1));
        }
        return true;
    }

    *lastMatchedRow = -1;
    return false;
}

/**
 * @brief 更新参数上传/下载进度条。
 * @author mozhengjie
 * @param finished 已完成数量。
 * @param total 总数量。
 * @param prefix 进度文本前缀。
 */
void Widget::updateParameterTransferProgress(int finished, int total, const QString &prefix)
{
    if (!parameterProgressBar_) {
        return;
    }

    if (total <= 0) {
        parameterProgressBar_->setRange(0, 100);
        parameterProgressBar_->setValue(0);
        parameterProgressBar_->setFormat(QStringLiteral("%1：空闲").arg(prefix));
        return;
    }

    const int boundedFinished = std::clamp(finished, 0, total);
    parameterProgressBar_->setRange(0, total);
    parameterProgressBar_->setValue(boundedFinished);
    parameterProgressBar_->setFormat(QStringLiteral("%1：%v/%m").arg(prefix));
}


