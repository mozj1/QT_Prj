#ifndef WIDGET_H
#define WIDGET_H

#include "core/communicationconfig.h"
#include "core/deviceconfig.h"
#include "models/faulttablemodel.h"
#include "models/monitortablemodel.h"
#include "models/parametertablemodel.h"

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QVector>

class QComboBox;
class QDialog;
class QDockWidget;
class QLabel;
class QLineEdit;
class QMenu;
class ModbusClient;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableView;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class QWidget;

class Widget final : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造伺服调试软件主窗口。
     * @author mozhengjie
     * @param parent 父窗口指针，默认为空。
     */
    explicit Widget(QWidget *parent = nullptr);

private:
    /**
     * @brief 创建顶部命令栏。
     * @author mozhengjie
     * @return QWidget* 顶部命令栏控件指针。
     */
    QWidget *createTopCommandBar();

    /**
     * @brief 创建左侧型号选择和导航区域。
     * @author mozhengjie
     * @return QWidget* 左侧面板控件指针。
     */
    QWidget *createLeftPanel();

    /**
     * @brief 创建中间主显示区域。
     * @author mozhengjie
     * @return QWidget* 主显示区域控件指针。
     */
    QWidget *createMainArea();

    /**
     * @brief 创建底部运行状态栏。
     * @author mozhengjie
     * @return QWidget* 底部状态栏控件指针。
     */
    QWidget *createBottomStatusBar();

    /**
     * @brief 创建参数总表二级操作面板。
     * @author mozhengjie
     * @return QWidget* 参数操作面板控件指针。
     */
    QWidget *createParameterActionPanel();

    /**
     * @brief 创建监控总表二级操作面板。
     * @author mozhengjie
     * @return QWidget* 监控操作面板控件指针。
     */
    QWidget *createMonitorActionPanel();

    /**
     * @brief 创建故障总表二级操作提示面板。
     * @author mozhengjie
     * @return QWidget* 故障操作提示面板控件指针。
     */
    QWidget *createFaultActionPanel();

    /**
     * @brief 创建示波器二级操作占位面板。
     * @author mozhengjie
     * @return QWidget* 示波器操作占位面板控件指针。
     */
    QWidget *createScopeActionPanel();

    /**
     * @brief 创建参数总表页面。
     * @author mozhengjie
     * @return QWidget* 参数总表页面控件指针。
     */
    QWidget *createParameterPage();

    /**
     * @brief 创建监控总表页面。
     * @author mozhengjie
     * @return QWidget* 监控总表页面控件指针。
     */
    QWidget *createMonitorPage();

    /**
     * @brief 创建故障总表页面。
     * @author mozhengjie
     * @return QWidget* 故障总表页面控件指针。
     */
    QWidget *createFaultPage();

    /**
     * @brief 创建示波器占位页面。
     * @author mozhengjie
     * @return QWidget* 示波器页面控件指针。
     */
    QWidget *createScopePage();

    /**
     * @brief 创建顶部命令按钮。
     * @author mozhengjie
     * @param text 按钮显示文本。
     * @return QToolButton* 顶部命令按钮指针。
     */
    QToolButton *createCommandButton(const QString &text) const;

    /**
     * @brief 创建左侧二级操作按钮。
     * @author mozhengjie
     * @param text 按钮显示文本。
     * @return QPushButton* 二级操作按钮指针。
     */
    QPushButton *createSecondaryButton(const QString &text) const;

    /**
     * @brief 根据当前屏幕可用区域设置初始窗口大小和居中位置。
     * @author mozhengjie
     */
    void applyInitialWindowGeometry();

    /**
     * @brief 配置表格为固定行高、整体宽度锁定、列宽可拖拽模式。
     * @author mozhengjie
     * @param table 需要配置的表格控件。
     */
    void configureFixedTableArea(QTableView *table) const;

    /**
     * @brief 将表格列宽限制在当前可视区域内。
     * @author mozhengjie
     * @param table 需要限制列宽的表格控件。
     */
    void constrainTableColumnsToViewport(QTableView *table) const;

    /**
     * @brief 扫描 XML 文件夹并刷新型号下拉框。
     * @author mozhengjie
     */
    void scanXmlModelFiles();

    /**
     * @brief 加载指定下拉索引对应的设备配置。
     * @author mozhengjie
     * @param index 型号下拉框索引。
     */
    void loadDeviceConfig(int index);

    /**
     * @brief 根据当前 XML 配置刷新参数总表。
     * @author mozhengjie
     */
    void refreshParameterTableFromConfig();

    /**
     * @brief 根据当前 XML 配置刷新监控总表。
     * @author mozhengjie
     */
    void refreshMonitorTableFromConfig();

    /**
     * @brief 根据当前 XML 配置刷新故障总表。
     * @author mozhengjie
     */
    void refreshFaultTableFromConfig();

    /**
     * @brief 根据左侧导航当前项切换主显示页面和二级操作面板。
     * @author mozhengjie
     */
    void selectMainPageForCurrentTreeItem();

    /**
     * @brief 响应左侧导航点击并处理故障总表轮询开关。
     * @author mozhengjie
     * @param item 被点击的导航项。
     * @param column 被点击的列号。
     */
    void handleNavigationItemClicked(QTreeWidgetItem *item, int column);

    /**
     * @brief 显示左侧工具入口的独立占位弹窗。
     * @author mozhengjie
     * @param title 弹窗标题。
     */
    void showToolPlaceholderDialog(const QString &title);

    /**
     * @brief 显示可停靠的运行工具窗体。
     * @author mozhengjie
     * @param title 运行工具窗体标题。
     */
    void showDockableRunWindow(const QString &title);

    /**
     * @brief 将已停靠的运行工具窗体收起为主显示区边缘标签。
     * @author mozhengjie
     * @param title 运行工具窗体标题。
     */
    void collapseRunDockWindow(const QString &title);

    /**
     * @brief 从边缘标签恢复运行工具停靠窗体。
     * @author mozhengjie
     * @param title 运行工具窗体标题。
     */
    void restoreRunDockWindow(const QString &title);

    /**
     * @brief 创建运行工具窗体内部收起工具栏。
     * @author mozhengjie
     * @param title 运行工具窗体标题。
     * @return QWidget* 收起工具栏控件。
     */
    QWidget *createRunDockToolbar(const QString &title);

    /**
     * @brief 创建停靠窗体收起后的边缘标签。
     * @author mozhengjie
     * @param title 运行工具窗体标题。
     * @param area 收起前的停靠区域。
     * @return QDockWidget* 边缘标签停靠窗体。
     */
    QDockWidget *createCollapsedRunDock(const QString &title, Qt::DockWidgetArea area);

    /**
     * @brief 显示定位运行独立调试窗体。
     * @author mozhengjie
     */
    void showPositionRunDialog();

    /**
     * @brief 创建点动运行可停靠窗体内容。
     * @author mozhengjie
     * @return QWidget* 点动运行内容控件。
     */
    QWidget *createJogRunPanel();

    /**
     * @brief 创建定位运行可停靠窗体内容。
     * @author mozhengjie
     * @return QWidget* 定位运行内容控件。
     */
    QWidget *createPositionRunPanel();

    /**
     * @brief 切换保存参数下拉菜单显示状态。
     * @author mozhengjie
     */
    void toggleSaveParameterMenu();

    /**
     * @brief 发送保存参数命令。
     * @author mozhengjie
     * @param motorParameter 是否保存电机参数，false 表示用户参数。
     */
    void sendSaveParameterCommand(bool motorParameter);

    /**
     * @brief 发送故障复位命令。
     * @author mozhengjie
     */
    void sendFaultResetCommand();

    /**
     * @brief 读取伺服状态后执行恢复出厂流程。
     * @author mozhengjie
     */
    void requestFactoryResetCommand();

    /**
     * @brief 读取伺服状态后执行电机复位流程。
     * @author mozhengjie
     */
    void requestMotorResetCommand();

    /**
     * @brief 写入单个控制寄存器。
     * @author mozhengjie
     * @param address 寄存器地址。
     * @param value 写入值。
     * @param requestPrefix 请求标识前缀。
     * @return bool 请求成功发出返回 true。
     */
    bool writeControlRegister(int address, quint16 value, const QString &requestPrefix);

    /**
     * @brief 读取寄存器 187 以校验伺服运行状态。
     * @author mozhengjie
     * @param requestPrefix 请求标识前缀。
     * @return bool 请求成功发出返回 true。
     */
    bool requestServoStateCheck(const QString &requestPrefix);

    /**
     * @brief 显示 2 秒自动关闭提示框。
     * @author mozhengjie
     * @param message 提示文本。
     */
    void showAutoCloseMessage(const QString &message);

    /**
     * @brief 更新底部状态栏中的当前电机型号文本。
     * @author mozhengjie
     * @param modelName 当前下拉框显示的型号名称。
     */
    void updateSelectedModelStatus(const QString &modelName);

    /**
     * @brief 显示通讯设置对话框并保存用户配置。
     * @author mozhengjie
     */
    void showCommunicationSettings();

    /**
     * @brief 根据当前连接状态执行连接或断开操作。
     * @author mozhengjie
     */
    void toggleModbusConnection();

    /**
     * @brief 刷新 Modbus 连接状态显示。
     * @author mozhengjie
     * @param connected 是否已连接。
     * @param statusText 状态文本。
     */
    void handleConnectionStatusChanged(bool connected, const QString &statusText);

    /**
     * @brief 显示 Modbus 错误信息。
     * @author mozhengjie
     * @param errorText 错误文本。
     */
    void handleModbusError(const QString &errorText);

    /**
     * @brief 刷新参数写入结果状态。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param success 写入是否成功。
     * @param message 写入结果描述。
     * @param requestTag Modbus 写入请求标识。
     */
    void handleParameterWriteCompleted(int startAddress,
                                       bool success,
                                       const QString &message,
                                       const QString &requestTag);

    /**
     * @brief 处理 Modbus 读取结果并分发给参数表或监控表。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param values 读取到的 16 位寄存器值。
     * @param success 读取是否成功。
     * @param message 读取结果描述。
     * @param requestTag 请求标识。
     */
    void handleRegisterReadCompleted(int startAddress,
                                     const QVector<quint16> &values,
                                     bool success,
                                     const QString &message,
                                     const QString &requestTag);

    /**
     * @brief 将已编辑参数值转换为寄存器并下发。
     * @author mozhengjie
     * @param definition 参数定义。
     * @param newValue 新参数值。
     * @return bool 请求成功发出返回 true。
     */
    bool writeParameterToServo(const RegisterDefinition &definition, const QString &newValue);

    /**
     * @brief 执行参数总表二级操作。
     * @author mozhengjie
     * @param action 操作名称。
     */
    void handleParameterAction(const QString &action);

    /**
     * @brief 启动参数上传队列。
     * @author mozhengjie
     * @param checkedOnly 是否只上传勾选参数。
     * @param automatic 是否为连接成功后的自动上传流程。
     */
    void startParameterUpload(bool checkedOnly, bool automatic = false);

    /**
     * @brief 上传队列读取下一个参数。
     * @author mozhengjie
     */
    void startNextParameterUpload();

    /**
     * @brief 启动参数下载队列。
     * @author mozhengjie
     * @param checkedOnly 是否只下载勾选参数。
     */
    void startParameterDownload(bool checkedOnly);

    /**
     * @brief 下载队列写入下一个参数。
     * @author mozhengjie
     */
    void startNextParameterDownload();

    /**
     * @brief 启动或关闭监控轮询。
     * @author mozhengjie
     */
    void toggleMonitorPolling();

    /**
     * @brief 读取当前勾选监控项。
     * @author mozhengjie
     */
    void pollSelectedMonitors();

    /**
     * @brief 低优先级读取伺服系统状态寄存器。
     * @author mozhengjie
     */
    void pollServoSystemState();

    /**
     * @brief 启动伺服系统状态轮询定时器。
     * @author mozhengjie
     */
    void startServoStatePolling();

    /**
     * @brief 启动故障总表轮询。
     * @author mozhengjie
     */
    void startFaultPolling();

    /**
     * @brief 停止故障总表轮询。
     * @author mozhengjie
     */
    void stopFaultPolling();

    /**
     * @brief 轮询读取故障寄存器 150 和 390。
     * @author mozhengjie
     */
    void pollFaultRegisters();

    /**
     * @brief 发起故障寄存器读取请求。
     * @author mozhengjie
     * @param requestPrefix 请求标识前缀。
     * @param highPriority 是否计入用户高优先级 Modbus 操作。
     */
    void requestFaultRegisters(const QString &requestPrefix, bool highPriority);

    /**
     * @brief 根据故障寄存器原始值刷新故障表和伺服报警显示。
     * @author mozhengjie
     * @param startAddress 寄存器地址。
     * @param values 读取到的寄存器值。
     */
    void updateFaultRegisterSnapshot(int startAddress, const QVector<quint16> &values);

    /**
     * @brief 设置底部伺服状态文本和报警底色。
     * @author mozhengjie
     * @param stateText 状态文本。
     * @param alarmActive 是否处于报警状态。
     */
    void setServoStateDisplay(const QString &stateText, bool alarmActive);

    /**
     * @brief 使用当前故障 bit 列表刷新伺服报警状态附加说明。
     * @author mozhengjie
     */
    void refreshServoAlarmFaultText();

    /**
     * @brief 判断当前是否存在高优先级 Modbus 操作。
     * @author mozhengjie
     * @return bool 存在用户读写或监控请求返回 true。
     */
    bool hasHighPriorityModbusWork() const;

    /**
     * @brief 判断当前是否具备执行 Modbus 操作的条件。
     * @author mozhengjie
     * @param operationName 操作名称。
     * @return bool 可执行返回 true。
     */
    bool ensureModbusReady(const QString &operationName);

    /**
     * @brief 搜索参数总表功能说明并循环定位匹配行。
     * @author mozhengjie
     */
    void searchParameterTable();

    /**
     * @brief 搜索监控总表监控名称并循环定位匹配行。
     * @author mozhengjie
     */
    void searchMonitorTable();

    /**
     * @brief 在指定表格列中循环搜索并滚动定位到匹配行。
     * @author mozhengjie
     * @param table 目标表格。
     * @param textColumn 搜索列号。
     * @param keyword 搜索关键字。
     * @param lastMatchedRow 上一次匹配行号。
     * @return bool 找到匹配行返回 true。
     */
    bool searchTableAndScroll(QTableView *table, int textColumn, const QString &keyword, int *lastMatchedRow);

    /**
     * @brief 更新参数上传/下载进度条。
     * @author mozhengjie
     * @param finished 已完成数量。
     * @param total 总数量。
     * @param prefix 进度文本前缀。
     */
    void updateParameterTransferProgress(int finished, int total, const QString &prefix);

    // 第一阶段只搭建主界面骨架；后续阶段会把这些控件接入 XML 解析、
    // Modbus 连接状态和参数/监控表格模型。
    QComboBox *modelSelector_ = nullptr;
    QTreeWidget *navigationTree_ = nullptr;
    QStackedWidget *secondaryActionStack_ = nullptr;
    QMainWindow *mainDockWindow_ = nullptr;
    QStackedWidget *mainStack_ = nullptr;
    QTableView *parameterTable_ = nullptr;
    QTableView *monitorTable_ = nullptr;
    QTableView *faultTable_ = nullptr;
    QLineEdit *parameterSearchEdit_ = nullptr;
    QLineEdit *monitorSearchEdit_ = nullptr;
    ParameterTableModel *parameterModel_ = nullptr;
    MonitorTableModel *monitorModel_ = nullptr;
    FaultTableModel *faultModel_ = nullptr;
    QToolButton *connectionToggleButton_ = nullptr;
    QToolButton *saveParameterButton_ = nullptr;
    QMenu *saveParameterMenu_ = nullptr;
    QPushButton *monitorToggleButton_ = nullptr;
    QSpinBox *monitorIntervalSpinBox_ = nullptr;
    QTimer *monitorTimer_ = nullptr;
    QTimer *servoStateTimer_ = nullptr;
    QTimer *faultPollTimer_ = nullptr;
    QLabel *connectionStatusLabel_ = nullptr;
    QLabel *servoStateLabel_ = nullptr;
    QLabel *operationStatusLabel_ = nullptr;
    QLabel *selectedModelLabel_ = nullptr;
    QProgressBar *parameterProgressBar_ = nullptr;
    QPointer<QDockWidget> jogRunDock_;
    QPointer<QDockWidget> positionRunDock_;
    QPointer<QDockWidget> collapsedJogRunDock_;
    QPointer<QDockWidget> collapsedPositionRunDock_;
    Qt::DockWidgetArea jogRunLastDockArea_ = Qt::RightDockWidgetArea;
    Qt::DockWidgetArea positionRunLastDockArea_ = Qt::RightDockWidgetArea;
    QSize jogRunLastDockSize_;
    QSize positionRunLastDockSize_;
    ModbusClient *modbusClient_ = nullptr;
    CommunicationConfig communicationConfig_;
    QVector<RegisterDefinition> pendingParameterUploadQueue_;
    QVector<RegisterDefinition> pendingParameterDownloadQueue_;
    QHash<QString, RegisterDefinition> pendingParameterReadMap_;
    QHash<QString, MonitorDefinition> pendingMonitorReadMap_;
    QHash<QString, int> pendingFaultReadMap_;
    QHash<int, quint16> latestFaultRegisterValues_;
    int parameterUploadTotal_ = 0;
    int parameterUploadFinished_ = 0;
    int parameterDownloadTotal_ = 0;
    int parameterDownloadFinished_ = 0;
    int modbusRequestSerial_ = 0;
    int activeHighPriorityModbusRequests_ = 0;
    bool parameterUploadIsAutomatic_ = false;
    bool servoAlarmStateActive_ = false;
    QString lastParameterSearchText_;
    QString lastMonitorSearchText_;
    int lastParameterMatchedRow_ = -1;
    int lastMonitorMatchedRow_ = -1;
    QVector<DeviceConfig> availableConfigs_;
    DeviceConfig currentConfig_;
};

#endif // WIDGET_H
