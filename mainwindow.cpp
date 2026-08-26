#include "mainwindow.h"

#include "app/appcontroller.h"
#include "core/communicationconfig.h"
#include "dialogs/communicationsettingsdialog.h"
#include "widgets/jogrunwidget.h"
#include "widgets/positionrunwidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {
constexpr int kInitialWindowWidth = 1180;
constexpr int kInitialWindowHeight = 720;
constexpr int kMinimumWindowWidth = 800;
constexpr int kMinimumWindowHeight = 520;
constexpr int kTopBarPreferredHeight = 52;
constexpr int kTopBarMinimumHeight = 44;
constexpr int kTopBarMaximumHeight = 76;
constexpr int kLeftPanelPreferredWidth = 220;
constexpr int kLeftPanelMinimumWidth = 170;
constexpr int kStatusBarPreferredHeight = 28;
constexpr int kStatusBarMinimumHeight = 24;
constexpr int kRunDockMinimumWidth = 80;
constexpr int kRunDockMinimumHeight = 60;
constexpr int kRunDockDefaultWidth = 437;
constexpr int kRunDockDefaultHeight = 259;

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
 * @brief Marks a shell button as active or inactive for stylesheet refresh.
 * @author mozhengjie
 * @param button Button to update.
 * @param active Whether the button is active.
 */
void setButtonActive(QPushButton *button, bool active)
{
    if (!button) {
        return;
    }

    button->setProperty("active", active);
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

/**
 * @brief Refreshes combo-box line-edit alignment when the combo is editable.
 * @author mozhengjie
 * @param combo Combo box to adjust.
 */
void centerEditableComboText(QComboBox *combo)
{
    if (!combo || !combo->lineEdit()) {
        return;
    }

    combo->lineEdit()->setAlignment(Qt::AlignCenter);
}

/**
 * @brief Converts horizontal text into one-character-per-line vertical tab text.
 * @author mozhengjie
 * @param text Source text.
 * @return QString Vertical display text.
 */
QString verticalTabText(const QString &text)
{
    QString result;
    result.reserve(text.size() * 2);
    for (const QChar character : text) {
        if (!result.isEmpty()) {
            result.append(QChar('\n'));
        }
        result.append(character);
    }
    return result;
}

/**
 * @brief Lightweight dock title bar; a click collapses a dock and drag events bubble to QDockWidget.
 * @author mozhengjie
 */
class RunDockTitleBar final : public QWidget
{
public:
    /**
     * @brief Builds a title bar used by run-panel docks.
     * @author mozhengjie
     * @param title Visible dock title.
     * @param dock Dock controlled by this title bar.
     * @param collapseCallback Callback invoked on docked single click.
     * @param closeCallback Callback invoked by the title-bar close button.
     * @param parent Parent widget.
     */
    RunDockTitleBar(const QString &title,
                    QDockWidget *dock,
                    std::function<void()> collapseCallback,
                    std::function<void()> closeCallback,
                    QWidget *parent = nullptr)
        : QWidget(parent)
        , dock_(dock)
        , collapseCallback_(std::move(collapseCallback))
        , closeCallback_(std::move(closeCallback))
    {
        setObjectName(QStringLiteral("runDockTitleBar"));
        setCursor(Qt::ArrowCursor);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(7, 2, 5, 2);
        layout->setSpacing(4);

        auto *label = new QLabel(title, this);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setStyleSheet(QStringLiteral("QLabel { color: #000000; background: transparent; font-size: 11px; font-weight: bold; }"));
        layout->addWidget(label);
        layout->addStretch(1);
        auto *closeButton = new QToolButton(this);
        closeButton->setText(QStringLiteral("X"));
        closeButton->setFixedSize(18, 18);
        closeButton->setCursor(Qt::PointingHandCursor);
        closeButton->setStyleSheet(QStringLiteral(
            "QToolButton { background: #FFFFFF; border: 1px solid #9E9E9E; color: #000000; font-size: 10px; padding: 0px; }"
            "QToolButton:hover { background: #FADDE1; border-color: #C78B91; }"));
        connect(closeButton, &QToolButton::clicked, this, [this]() {
            if (closeCallback_) {
                closeCallback_();
            }
        });
        layout->addWidget(closeButton);
        setStyleSheet(QStringLiteral("#runDockTitleBar { background: #D9F2F0; border: 1px solid #7DAAA6; }"));
    }

protected:
    /**
     * @brief Records the mouse press point and leaves dragging to QDockWidget.
     * @author mozhengjie
     * @param event Mouse press event.
     */
    void mousePressEvent(QMouseEvent *event) override
    {
        pressed_ = event->button() == Qt::LeftButton;
        dragExceeded_ = false;
        pressGlobalPosition_ = event->globalPosition();
        event->ignore();
    }

    /**
     * @brief Tracks drag distance so a dock drag is not mistaken for a collapse click.
     * @author mozhengjie
     * @param event Mouse move event.
     */
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (pressed_) {
            const QPointF delta = event->globalPosition() - pressGlobalPosition_;
            if (delta.manhattanLength() > QApplication::startDragDistance()) {
                dragExceeded_ = true;
            }
        }
        event->ignore();
    }

    /**
     * @brief Collapses the dock on a docked single click; drag releases continue to QDockWidget.
     * @author mozhengjie
     * @param event Mouse release event.
     */
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const bool collapseClick = pressed_ &&
                                   !dragExceeded_ &&
                                   event->button() == Qt::LeftButton &&
                                   dock_ &&
                                   !dock_->isFloating();
        pressed_ = false;

        if (collapseClick && collapseCallback_) {
            collapseCallback_();
            event->accept();
            return;
        }
        event->ignore();
    }

private:
    QDockWidget *dock_ = nullptr;
    std::function<void()> collapseCallback_;
    std::function<void()> closeCallback_;
    QPointF pressGlobalPosition_;
    bool pressed_ = false;
    bool dragExceeded_ = false;
};
} // namespace

/**
 * @brief Builds the fixed application shell and nests run docks inside the central workspace only.
 * @author mozhengjie
 * @param appController Shared application controller for model, communication and status operations.
 * @param parent Parent widget.
 */
MainWindow::MainWindow(AppController *appController, QWidget *parent)
    : QMainWindow(parent)
    , appController_(appController)
{
    setWindowTitle(QStringLiteral("伺服调试软件"));
    resize(kInitialWindowWidth, kInitialWindowHeight);
    setMinimumSize(kMinimumWindowWidth, kMinimumWindowHeight);
    setAnimated(false);

    configureShellStyle();
    setCentralWidget(createShellWidget());
    configureConnections();
    refreshModelSelector();
    refreshCurrentModelSelection();
    refreshPageButtons();
    refreshPageActionGroups();
    refreshConnectionButton();
    refreshMonitorControls();
    refreshStatusLabels();
    refreshProgressBar();
    refreshServoStateLabel();
    configureInitialWindowGeometry();
}

/**
 * @brief Returns the active central QML page index.
 * @author mozhengjie
 * @return Active page index.
 */
int MainWindow::activePage() const
{
    return activePage_;
}

/**
 * @brief Switches the central content page and synchronizes page-dependent polling state.
 * @author mozhengjie
 * @param pageIndex Target page index.
 */
void MainWindow::setActivePage(int pageIndex)
{
    const int boundedIndex = qBound(0, pageIndex, 3);
    const bool changed = activePage_ != boundedIndex;
    activePage_ = boundedIndex;
    appController_->setActivePage(activePage_);

    if (changed) {
        emit activePageChanged();
    }
    refreshPageButtons();
    refreshPageActionGroups();
}

/**
 * @brief Shows and raises the position-run dock panel.
 * @author mozhengjie
 */
void MainWindow::showPositionDock()
{
    showRunDock(positionDock_);
}

/**
 * @brief Shows and raises the jog-run dock panel.
 * @author mozhengjie
 */
void MainWindow::showJogDock()
{
    showRunDock(jogDock_);
}

/**
 * @brief Creates the outer fixed shell around the central dock host.
 * @author mozhengjie
 * @return Shell widget assigned as QMainWindow central widget.
 */
QWidget *MainWindow::createShellWidget()
{
    auto *shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("mainShell"));

    auto *topBar = createTopBar();
    auto *leftPanel = createLeftPanel();
    contentDockHost_ = createContentDockHost();
    auto *bottomStatusBar = createBottomStatusBar();

    auto *middleSplitter = new QSplitter(Qt::Horizontal, shell);
    middleSplitter->setObjectName(QStringLiteral("middleSplitter"));
    middleSplitter->setHandleWidth(6);
    middleSplitter->addWidget(leftPanel);
    middleSplitter->addWidget(contentDockHost_);
    middleSplitter->setStretchFactor(0, 0);
    middleSplitter->setStretchFactor(1, 1);
    middleSplitter->setSizes({kLeftPanelPreferredWidth, kInitialWindowWidth - kLeftPanelPreferredWidth});

    auto *verticalSplitter = new QSplitter(Qt::Vertical, shell);
    verticalSplitter->setObjectName(QStringLiteral("workSplitter"));
    verticalSplitter->setHandleWidth(6);
    verticalSplitter->addWidget(topBar);
    verticalSplitter->addWidget(middleSplitter);
    verticalSplitter->addWidget(bottomStatusBar);
    verticalSplitter->setStretchFactor(0, 0);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setStretchFactor(2, 0);
    verticalSplitter->setSizes({kTopBarPreferredHeight,
                                kInitialWindowHeight - kTopBarPreferredHeight - kStatusBarPreferredHeight,
                                kStatusBarPreferredHeight});

    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(verticalSplitter);
    return shell;
}

/**
 * @brief Creates the fixed top toolbar; run docks do not resize this area.
 * @author mozhengjie
 * @return Top toolbar widget.
 */
QWidget *MainWindow::createTopBar()
{
    auto *topBar = new QWidget(this);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setMinimumHeight(kTopBarMinimumHeight);
    topBar->setMaximumHeight(kTopBarMaximumHeight);

    auto *layout = new QHBoxLayout(topBar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *communicationButton = createShellButton(QStringLiteral("通讯设置"));
    connect(communicationButton, &QPushButton::clicked, this, &MainWindow::openCommunicationSettings);
    layout->addWidget(communicationButton);

    connectionButton_ = createShellButton(QStringLiteral("连接/断开"));
    connect(connectionButton_, &QPushButton::clicked, appController_, &AppController::toggleConnection);
    layout->addWidget(connectionButton_);

    layout->addStretch(1);

    auto *saveButton = createShellButton(QStringLiteral("保存参数"));
    auto *saveMenu = new QMenu(saveButton);
    saveMenu->setObjectName(QStringLiteral("saveParameterMenu"));
    saveMenu->addAction(QStringLiteral("用户参数"), appController_, &AppController::saveUserParameters);
    saveMenu->addAction(QStringLiteral("电机参数"), appController_, &AppController::saveMotorParameters);
    saveButton->setMenu(saveMenu);
    layout->addWidget(saveButton);

    auto *openLoopButton = createShellButton(QStringLiteral("开环调试"));
    connect(openLoopButton, &QPushButton::clicked, this, [this]() {
        showTransientMessage(QStringLiteral("开环调试"), QStringLiteral("开环调试将在后续阶段接入。"));
    });
    layout->addWidget(openLoopButton);

    auto *zeroButton = createShellButton(QStringLiteral("电机调零"));
    connect(zeroButton, &QPushButton::clicked, this, [this]() {
        showTransientMessage(QStringLiteral("电机调零"), QStringLiteral("电机调零将在后续阶段接入。"));
    });
    layout->addWidget(zeroButton);

    auto *faultResetButton = createShellButton(QStringLiteral("故障复位"));
    connect(faultResetButton, &QPushButton::clicked, appController_, &AppController::sendFaultResetCommand);
    layout->addWidget(faultResetButton);

    auto *factoryResetButton = createShellButton(QStringLiteral("恢复出厂"));
    connect(factoryResetButton, &QPushButton::clicked, appController_, &AppController::requestFactoryResetCommand);
    layout->addWidget(factoryResetButton);

    auto *motorResetButton = createShellButton(QStringLiteral("电机复位"));
    connect(motorResetButton, &QPushButton::clicked, appController_, &AppController::requestMotorResetCommand);
    layout->addWidget(motorResetButton);
    return topBar;
}

/**
 * @brief Creates the fixed left navigation and page action area.
 * @author mozhengjie
 * @return Left-side widget.
 */
QWidget *MainWindow::createLeftPanel()
{
    auto *leftPanel = new QWidget(this);
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setMinimumWidth(kLeftPanelMinimumWidth);

    auto *layout = new QVBoxLayout(leftPanel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("选择电机型号"), leftPanel);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(titleLabel);

    modelSelector_ = new QComboBox(leftPanel);
    modelSelector_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    centerEditableComboText(modelSelector_);
    connect(modelSelector_, qOverload<int>(&QComboBox::activated), appController_, &AppController::setCurrentModelIndex);
    layout->addWidget(modelSelector_);

    auto *hintLabel = new QLabel(QStringLiteral("型号下方可选择总表和运行工具"), leftPanel);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    const QStringList pageNames = {QStringLiteral("参数总表"),
                                   QStringLiteral("监控总表"),
                                   QStringLiteral("故障总表"),
                                   QStringLiteral("示波器"),
                                   QStringLiteral("点动运行"),
                                   QStringLiteral("定位运行")};
    for (int i = 0; i < pageNames.size(); ++i) {
        auto *button = createShellButton(pageNames.at(i));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(button);
        pageButtons_.append(button);

        if (i == 4) {
            connect(button, &QPushButton::clicked, this, &MainWindow::showJogDock);
        } else if (i == 5) {
            connect(button, &QPushButton::clicked, this, &MainWindow::showPositionDock);
        } else {
            connect(button, &QPushButton::clicked, this, [this, i]() { setActivePage(i); });
        }
    }

    auto *separator = new QFrame(leftPanel);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    parameterActionsWidget_ = new QWidget(leftPanel);
    auto *parameterLayout = new QVBoxLayout(parameterActionsWidget_);
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(6);
    auto *parameterTitle = new QLabel(QStringLiteral("参数操作"), parameterActionsWidget_);
    parameterTitle->setObjectName(QStringLiteral("sectionTitle"));
    parameterLayout->addWidget(parameterTitle);
    auto *uploadAllButton = createShellButton(QStringLiteral("上传全部"));
    auto *downloadAllButton = createShellButton(QStringLiteral("下载全部"));
    auto *uploadCheckedButton = createShellButton(QStringLiteral("上传勾选"));
    auto *downloadCheckedButton = createShellButton(QStringLiteral("下载勾选"));
    connect(uploadAllButton, &QPushButton::clicked, appController_, &AppController::uploadAllParameters);
    connect(downloadAllButton, &QPushButton::clicked, appController_, &AppController::downloadAllParameters);
    connect(uploadCheckedButton, &QPushButton::clicked, appController_, &AppController::uploadCheckedParameters);
    connect(downloadCheckedButton, &QPushButton::clicked, appController_, &AppController::downloadCheckedParameters);
    for (QPushButton *button : {uploadAllButton, downloadAllButton, uploadCheckedButton, downloadCheckedButton}) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        parameterLayout->addWidget(button);
    }
    layout->addWidget(parameterActionsWidget_);

    monitorActionsWidget_ = new QWidget(leftPanel);
    auto *monitorLayout = new QVBoxLayout(monitorActionsWidget_);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(6);
    auto *monitorTitle = new QLabel(QStringLiteral("监控操作"), monitorActionsWidget_);
    monitorTitle->setObjectName(QStringLiteral("sectionTitle"));
    monitorLayout->addWidget(monitorTitle);
    monitorLayout->addWidget(new QLabel(QStringLiteral("监控间隔 ms"), monitorActionsWidget_));
    monitorIntervalSpinBox_ = new QSpinBox(monitorActionsWidget_);
    monitorIntervalSpinBox_->setRange(10, 60000);
    monitorIntervalSpinBox_->setSingleStep(10);
    monitorIntervalSpinBox_->setAlignment(Qt::AlignCenter);
    connect(monitorIntervalSpinBox_, qOverload<int>(&QSpinBox::valueChanged), appController_, &AppController::setMonitorIntervalMs);
    monitorLayout->addWidget(monitorIntervalSpinBox_);
    monitorToggleButton_ = createShellButton(QStringLiteral("启动监控"));
    monitorToggleButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(monitorToggleButton_, &QPushButton::clicked, appController_, &AppController::toggleMonitorPolling);
    monitorLayout->addWidget(monitorToggleButton_);
    layout->addWidget(monitorActionsWidget_);

    layout->addStretch(1);
    return leftPanel;
}

/**
 * @brief Creates the fixed bottom status area; run docks do not resize this area.
 * @author mozhengjie
 * @return Bottom status widget.
 */
QWidget *MainWindow::createBottomStatusBar()
{
    auto *statusBar = new QWidget(this);
    statusBar->setObjectName(QStringLiteral("bottomStatusBar"));
    statusBar->setMinimumHeight(kStatusBarMinimumHeight);
    statusBar->setMaximumHeight(kStatusBarPreferredHeight + 8);

    auto *layout = new QHBoxLayout(statusBar);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(16);

    connectionStatusLabel_ = new QLabel(statusBar);
    selectedModelStatusLabel_ = new QLabel(statusBar);
    progressBar_ = new QProgressBar(statusBar);
    servoStateLabel_ = new QLabel(statusBar);
    operationStatusLabel_ = new QLabel(statusBar);

    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setFixedWidth(320);

    servoStateLabel_->setObjectName(QStringLiteral("servoStateLabel"));
    servoStateLabel_->setMinimumHeight(22);
    servoStateLabel_->setAlignment(Qt::AlignCenter);
    operationStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(connectionStatusLabel_);
    layout->addWidget(selectedModelStatusLabel_);
    layout->addWidget(progressBar_);
    layout->addStretch(1);
    layout->addWidget(servoStateLabel_);
    layout->addWidget(operationStatusLabel_);
    return statusBar;
}

/**
 * @brief Creates the inner QMainWindow that owns only the central QML content and run docks.
 * @author mozhengjie
 * @return Dock host embedded in the outer shell middle area.
 */
QMainWindow *MainWindow::createContentDockHost()
{
    auto *dockHost = new QMainWindow(this);
    contentDockHost_ = dockHost;
    dockHost->setObjectName(QStringLiteral("contentDockHost"));
    dockHost->setDockNestingEnabled(true);
    dockHost->setAnimated(false);
    dockHost->installEventFilter(this);

    centralQuickWidget_ = createQuickWidget(QUrl(QStringLiteral("qrc:/ModbusApp/qml/Main.qml")),
                                            QStringLiteral("centralContentQuickWidget"));
    dockHost->setCentralWidget(centralQuickWidget_);

    positionDock_ = createRunDock(QStringLiteral("定位运行"),
                                  positionRunWidget_ = new PositionRunWidget(dockHost),
                                  QStringLiteral("positionRunDock"));
    jogDock_ = createRunDock(QStringLiteral("点动运行"),
                             new JogRunWidget(dockHost),
                             QStringLiteral("jogRunDock"));

    dockHost->addDockWidget(Qt::RightDockWidgetArea, positionDock_);
    dockHost->addDockWidget(Qt::RightDockWidgetArea, jogDock_);
    dockHost->splitDockWidget(positionDock_, jogDock_, Qt::Vertical);
    positionDock_->hide();
    jogDock_->hide();
    return dockHost;
}

/**
 * @brief Creates the QML central content widget in content-only mode.
 * @author mozhengjie
 * @param sourceUrl QML resource URL.
 * @param objectName Object name for diagnostics.
 * @return Configured QQuickWidget.
 */
QQuickWidget *MainWindow::createQuickWidget(const QUrl &sourceUrl, const QString &objectName)
{
    auto *quickWidget = new QQuickWidget(this);
    quickWidget->setObjectName(objectName);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setClearColor(Qt::white);
    quickWidget->setMinimumSize(300, 220);
    quickWidget->rootContext()->setContextProperty(QStringLiteral("appController"), appController_);
    quickWidget->rootContext()->setContextProperty(QStringLiteral("uiController"), this);
    quickWidget->setSource(sourceUrl);
    if (QQuickItem *rootObject = quickWidget->rootObject()) {
        rootObject->setProperty("contentOnly", true);
    }
    return quickWidget;
}

/**
 * @brief Creates a pure QWidget dock panel for stable floating and cross-screen dragging.
 * @author mozhengjie
 * @param title Dock title.
 * @param contentWidget Dock content widget.
 * @param objectName Object name for diagnostics.
 * @return Configured QDockWidget.
 */
QDockWidget *MainWindow::createRunDock(const QString &title, QWidget *contentWidget, const QString &objectName)
{
    auto *dock = new QDockWidget(title, contentDockHost_);
    dock->setObjectName(objectName);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable |
                      QDockWidget::DockWidgetClosable);
    dock->setTitleBarWidget(new RunDockTitleBar(title, dock, [this, dock]() {
        collapseRunDock(dock);
    }, [dock]() {
        dock->hide();
    }, dock));

    connect(dock, &QDockWidget::topLevelChanged, this, [this, dock](bool floating) {
        handleRunDockTopLevelChanged(dock, floating);
    });
    connect(dock, &QDockWidget::dockLocationChanged, this, [this, dock](Qt::DockWidgetArea area) {
        handleRunDockLocationChanged(dock, area);
    });

    contentWidget->setParent(dock);
    contentWidget->setMinimumSize(kRunDockMinimumWidth, kRunDockMinimumHeight);
    contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dock->setWidget(contentWidget);
    dock->setMinimumSize(kRunDockMinimumWidth, kRunDockMinimumHeight);
    dock->resize(kRunDockDefaultWidth, kRunDockDefaultHeight);
    return dock;
}

/**
 * @brief Creates a shell push button with a shared visual style.
 * @author mozhengjie
 * @param text Button text.
 * @return Configured push button.
 */
QPushButton *MainWindow::createShellButton(const QString &text)
{
    auto *button = new QPushButton(text, this);
    button->setMinimumHeight(30);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("active", false);
    return button;
}

/**
 * @brief Applies unified light theme styles to shell widgets and docks.
 * @author mozhengjie
 */
void MainWindow::configureShellStyle()
{
    setStyleSheet(QStringLiteral(
        "QWidget { color: #000000; font-size: 14px; }"
        "#mainShell, #contentDockHost { background: #FFFFFF; }"
        "#topBar { background: #D9F2F0; border: 1px solid #7DAAA6; }"
        "#leftPanel { background: #FFF8DA; border: 1px solid #C8B56A; }"
        "#bottomStatusBar { background: #FADDE1; border: 1px solid #C78B91; }"
        "QLabel#sectionTitle { font-weight: bold; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #9E9E9E; padding: 4px 10px; color: #000000; }"
        "QPushButton:hover { background: #E5F2D8; border-color: #7DAAA6; }"
        "QPushButton:pressed { background: #D4E8C5; }"
        "QPushButton[active=\"true\"] { background: #D8EFC8; border-color: #7DAAA6; }"
        "QPushButton::menu-indicator { image: none; width: 0px; }"
        "QMenu { background: #FFFFFF; border: 1px solid #A8A8A8; color: #000000; }"
        "QMenu::item { padding: 6px 22px; color: #000000; background: #FFFFFF; }"
        "QMenu::item:selected { background: #DDEAF7; }"
        "QComboBox, QSpinBox { background: #FFFFFF; border: 1px solid #B8B8B8; padding: 4px 6px; color: #000000; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #000000; selection-background-color: #DDEAF7; }"
        "QSplitter::handle { background: #A8A8A8; }"
        "QSplitter::handle:hover { background: #7DAAA6; }"
        "QDockWidget { background: #F4F4F4; color: #000000; titlebar-close-icon: none; titlebar-normal-icon: none; }"
        "QDockWidget::title { background: #D9F2F0; border: 1px solid #7DAAA6; padding: 4px; text-align: left; color: #000000; }"
        "QMainWindow::separator { background: #A8A8A8; width: 4px; height: 4px; }"
        "QMainWindow::separator:hover { background: #7DAAA6; }"
        "QProgressBar { background: #FFFFFF; border: 1px solid #9E9E9E; color: #000000; text-align: center; }"
        "QProgressBar::chunk { background: #00B050; }"));
}

/**
 * @brief Connects controller signals to shell refresh slots.
 * @author mozhengjie
 */
void MainWindow::configureConnections()
{
    connect(appController_, &AppController::modelNamesChanged, this, &MainWindow::refreshModelSelector);
    connect(appController_, &AppController::currentModelIndexChanged, this, &MainWindow::refreshCurrentModelSelection);
    connect(appController_, &AppController::connectedChanged, this, &MainWindow::refreshConnectionButton);
    connect(appController_, &AppController::connectionStatusChanged, this, &MainWindow::refreshStatusLabels);
    connect(appController_, &AppController::operationStatusChanged, this, &MainWindow::refreshStatusLabels);
    connect(appController_, &AppController::selectedModelStatusChanged, this, &MainWindow::refreshStatusLabels);
    connect(appController_, &AppController::progressChanged, this, &MainWindow::refreshProgressBar);
    connect(appController_, &AppController::monitorIntervalMsChanged, this, &MainWindow::refreshMonitorControls);
    connect(appController_, &AppController::monitorPollingActiveChanged, this, &MainWindow::refreshMonitorControls);
    connect(appController_, &AppController::servoStateChanged, this, &MainWindow::refreshServoStateLabel);
}

/**
 * @brief Centers the main window on the primary screen after layout creation.
 * @author mozhengjie
 */
void MainWindow::configureInitialWindowGeometry()
{
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect availableGeometry = screen->availableGeometry();
        move(availableGeometry.center() - rect().center());
    }
}

/**
 * @brief Opens communication settings and forwards accepted values to the controller.
 * @author mozhengjie
 */
void MainWindow::openCommunicationSettings()
{
    appController_->refreshSerialPorts();

    CommunicationConfig config;
    config.portName = appController_->portName();
    config.slaveAddress = appController_->slaveAddress();
    config.baudRate = appController_->baudRate();
    config.responseTimeoutMs = appController_->responseTimeoutMs();
    config.retryCount = appController_->retryCount();
    applyFormatText(appController_->serialFormat(), &config);

    CommunicationSettingsDialog dialog(config, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const CommunicationConfig nextConfig = dialog.settings();
    appController_->setCommunicationSettings(nextConfig.portName,
                                             nextConfig.slaveAddress,
                                             nextConfig.baudRate,
                                             nextConfig.formatText(),
                                             nextConfig.responseTimeoutMs,
                                             nextConfig.retryCount);
}

/**
 * @brief Shows a short-lived white message box for features not yet connected to Modbus commands.
 * @author mozhengjie
 * @param title Message title.
 * @param message Message body.
 */
void MainWindow::showTransientMessage(const QString &title, const QString &message)
{
    auto *box = new QMessageBox(QMessageBox::Information, title, message, QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setStyleSheet(QStringLiteral(
        "QMessageBox { background: #FFFFFF; }"
        "QLabel { color: #000000; background: transparent; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #A8A8A8; padding: 4px 12px; color: #000000; }"));
    box->show();
    QTimer::singleShot(2000, box, &QMessageBox::accept);
}

/**
 * @brief Reloads the motor-model combo box from the controller.
 * @author mozhengjie
 */
void MainWindow::refreshModelSelector()
{
    if (!modelSelector_) {
        return;
    }

    QSignalBlocker blocker(modelSelector_);
    modelSelector_->clear();
    modelSelector_->addItems(appController_->modelNames());
    modelSelector_->setCurrentIndex(appController_->currentModelIndex());
}

/**
 * @brief Refreshes only the selected motor-model index.
 * @author mozhengjie
 */
void MainWindow::refreshCurrentModelSelection()
{
    if (!modelSelector_) {
        return;
    }

    QSignalBlocker blocker(modelSelector_);
    modelSelector_->setCurrentIndex(appController_->currentModelIndex());
}

/**
 * @brief Updates active highlighting for central page buttons.
 * @author mozhengjie
 */
void MainWindow::refreshPageButtons()
{
    for (int i = 0; i < pageButtons_.size(); ++i) {
        setButtonActive(pageButtons_.at(i), i == activePage_ && i < 4);
    }
}

/**
 * @brief Shows page-specific action controls in the fixed left panel.
 * @author mozhengjie
 */
void MainWindow::refreshPageActionGroups()
{
    if (parameterActionsWidget_) {
        parameterActionsWidget_->setVisible(activePage_ == 0);
    }
    if (monitorActionsWidget_) {
        monitorActionsWidget_->setVisible(activePage_ == 1);
    }
}

/**
 * @brief Updates the connection button text from the controller connection state.
 * @author mozhengjie
 */
void MainWindow::refreshConnectionButton()
{
    if (!connectionButton_) {
        return;
    }

    connectionButton_->setText(appController_->isConnected() ? QStringLiteral("断开连接") : QStringLiteral("连接/断开"));
    setButtonActive(connectionButton_, appController_->isConnected());
}

/**
 * @brief Synchronizes monitor interval and start/stop state controls.
 * @author mozhengjie
 */
void MainWindow::refreshMonitorControls()
{
    if (monitorIntervalSpinBox_) {
        QSignalBlocker blocker(monitorIntervalSpinBox_);
        monitorIntervalSpinBox_->setValue(appController_->monitorIntervalMs());
    }

    if (monitorToggleButton_) {
        monitorToggleButton_->setText(appController_->monitorPollingActive()
                                          ? QStringLiteral("关闭监控")
                                          : QStringLiteral("启动监控"));
        setButtonActive(monitorToggleButton_, appController_->monitorPollingActive());
    }
}

/**
 * @brief Updates fixed bottom status text labels from controller properties.
 * @author mozhengjie
 */
void MainWindow::refreshStatusLabels()
{
    if (connectionStatusLabel_) {
        connectionStatusLabel_->setText(appController_->connectionStatus());
    }
    if (selectedModelStatusLabel_) {
        selectedModelStatusLabel_->setText(appController_->selectedModelStatus());
    }
    if (operationStatusLabel_) {
        operationStatusLabel_->setText(appController_->operationStatus());
    }
}

/**
 * @brief Updates the parameter-operation progress bar in the fixed bottom status area.
 * @author mozhengjie
 */
void MainWindow::refreshProgressBar()
{
    if (!progressBar_) {
        return;
    }

    progressBar_->setMaximum(qMax(1, appController_->progressMaximum()));
    progressBar_->setValue(qBound(0, appController_->progressValue(), progressBar_->maximum()));
    progressBar_->setFormat(appController_->progressText());
}

/**
 * @brief Updates servo-state text and applies a red alarm background when required.
 * @author mozhengjie
 */
void MainWindow::refreshServoStateLabel()
{
    if (!servoStateLabel_) {
        return;
    }

    servoStateLabel_->setText(appController_->servoStateText());
    const bool alarm = appController_->servoAlarmActive();
    servoStateLabel_->setStyleSheet(alarm
                                        ? QStringLiteral("QLabel#servoStateLabel { background: #FF4D4F; border: 1px solid #C00000; padding: 1px 6px; color: #000000; }")
                                        : QStringLiteral("QLabel#servoStateLabel { background: transparent; border: 1px solid transparent; padding: 1px 6px; color: #000000; }"));
}

/**
 * @brief Handles central dock-host resize events used by collapsed edge tabs.
 * @author mozhengjie
 * @param watched Watched object.
 * @param event Event being processed.
 * @return true if the event is consumed.
 */
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == contentDockHost_ && event->type() == QEvent::Resize) {
        positionCollapsedTabs();
    }
    return QMainWindow::eventFilter(watched, event);
}

/**
 * @brief Shows a run dock; hidden docks open as floating windows with default geometry.
 * @author mozhengjie
 * @param dock Dock widget to show.
 */
void MainWindow::showRunDock(QDockWidget *dock)
{
    if (!dock) {
        return;
    }

    if (collapsedTabs_.contains(dock)) {
        restoreCollapsedDock(dock);
        return;
    }

    if (!dock->isVisible()) {
        dock->show();
        dock->setFloating(true);
        QTimer::singleShot(0, this, [this, dock]() {
            if (!dock) {
                return;
            }
            restoreDockDefaultFloatingSize(dock);
            placeDockForUserOpen(dock);
            activateDock(dock);
        });
        return;
    }
    activateDock(dock);
}

/**
 * @brief Restores the run dock to the configured default floating size.
 * @author mozhengjie
 * @param dock Dock widget to resize.
 */
void MainWindow::restoreDockDefaultFloatingSize(QDockWidget *dock)
{
    if (!dock) {
        return;
    }

    dock->resize(kRunDockDefaultWidth, kRunDockDefaultHeight);
}

/**
 * @brief Places a user-opened floating dock near the main window and inside the active screen.
 * @author mozhengjie
 * @param dock Dock widget to place.
 */
void MainWindow::placeDockForUserOpen(QDockWidget *dock)
{
    if (!dock || !dock->isFloating()) {
        return;
    }

    QScreen *screen = windowHandle() ? windowHandle()->screen() : QApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    QPoint target = frameGeometry().topLeft() + QPoint(80, 80);
    target.setX(qBound(available.left(), target.x(), qMax(available.left(), available.right() - dock->width())));
    target.setY(qBound(available.top(), target.y(), qMax(available.top(), available.bottom() - dock->height())));
    dock->move(target);
}

/**
 * @brief Handles floating state transitions for run docks.
 * @author mozhengjie
 * @param dock Dock widget that changed state.
 * @param floating True when the dock became a top-level floating window.
 */
void MainWindow::handleRunDockTopLevelChanged(QDockWidget *dock, bool floating)
{
    if (!dock) {
        return;
    }

    if (floating) {
        if (dock == positionDock_ && positionRunWidget_) {
            positionRunWidget_->setLayoutMode(PositionRunWidget::LayoutMode::FloatingDefault);
        }
        QTimer::singleShot(0, this, [this, dock]() {
            if (dock && dock->isFloating()) {
                restoreDockDefaultFloatingSize(dock);
            }
        });
        return;
    }

    QTimer::singleShot(0, this, [this, dock]() {
        handleRunDockLocationChanged(dock, contentDockHost_->dockWidgetArea(dock));
    });
}

/**
 * @brief Updates run dock layout when it is embedded into a supported dock area.
 * @author mozhengjie
 * @param dock Dock widget that moved.
 * @param area New dock area.
 */
void MainWindow::handleRunDockLocationChanged(QDockWidget *dock, Qt::DockWidgetArea area)
{
    if (!dock || dock->isFloating()) {
        return;
    }

    if (area == Qt::RightDockWidgetArea || area == Qt::BottomDockWidgetArea) {
        collapsedAreas_[dock] = area;
    }
    updatePositionDockLayout();
}

/**
 * @brief Applies the correct position-run panel layout for the current dock area.
 * @author mozhengjie
 */
void MainWindow::updatePositionDockLayout()
{
    if (!positionDock_ || !positionRunWidget_) {
        return;
    }

    if (positionDock_->isFloating()) {
        positionRunWidget_->setLayoutMode(PositionRunWidget::LayoutMode::FloatingDefault);
        return;
    }

    const Qt::DockWidgetArea area = contentDockHost_->dockWidgetArea(positionDock_);
    if (area == Qt::BottomDockWidgetArea) {
        positionRunWidget_->setLayoutMode(PositionRunWidget::LayoutMode::DockedBottom);
    } else if (area == Qt::RightDockWidgetArea) {
        positionRunWidget_->setLayoutMode(PositionRunWidget::LayoutMode::DockedRight);
    } else {
        positionRunWidget_->setLayoutMode(PositionRunWidget::LayoutMode::FloatingDefault);
    }
}

/**
 * @brief Collapses a docked run panel to a thin edge tab inside the central dock host.
 * @author mozhengjie
 * @param dock Dock widget to collapse.
 */
void MainWindow::collapseRunDock(QDockWidget *dock)
{
    if (!dock || dock->isFloating() || !dock->isVisible()) {
        return;
    }

    const Qt::DockWidgetArea area = contentDockHost_->dockWidgetArea(dock);
    if (area != Qt::RightDockWidgetArea && area != Qt::BottomDockWidgetArea) {
        return;
    }

    collapsedAreas_[dock] = area;
    collapsedDockSizes_[dock] = dock->size();
    dock->hide();

    QToolButton *tab = ensureCollapsedTab(dock, area);
    tab->show();
    tab->raise();
    positionCollapsedTabs();
}

/**
 * @brief Restores a collapsed run panel to its last embedded area.
 * @author mozhengjie
 * @param dock Dock widget to restore.
 */
void MainWindow::restoreCollapsedDock(QDockWidget *dock)
{
    if (!dock) {
        return;
    }

    const Qt::DockWidgetArea area = collapsedAreas_.value(dock, Qt::RightDockWidgetArea);
    if (area == Qt::RightDockWidgetArea || area == Qt::BottomDockWidgetArea) {
        contentDockHost_->addDockWidget(area, dock);
    }

    if (QToolButton *tab = collapsedTabs_.take(dock)) {
        tab->deleteLater();
    }
    dock->setFloating(false);
    dock->show();
    dock->raise();

    const QSize restoreSize = collapsedDockSizes_.value(dock, QSize(kRunDockDefaultWidth, kRunDockDefaultHeight));
    if (area == Qt::RightDockWidgetArea) {
        contentDockHost_->resizeDocks({dock}, {qMax(kRunDockMinimumWidth, restoreSize.width())}, Qt::Horizontal);
    } else if (area == Qt::BottomDockWidgetArea) {
        contentDockHost_->resizeDocks({dock}, {qMax(kRunDockMinimumHeight, restoreSize.height())}, Qt::Vertical);
    }
    updatePositionDockLayout();
    positionCollapsedTabs();
}

/**
 * @brief Creates or returns the edge tab used to restore a collapsed run dock.
 * @author mozhengjie
 * @param dock Dock represented by the tab.
 * @param area Dock area used to choose tab orientation.
 * @return QToolButton* Collapsed edge tab.
 */
QToolButton *MainWindow::ensureCollapsedTab(QDockWidget *dock, Qt::DockWidgetArea area)
{
    if (QToolButton *existingTab = collapsedTabs_.value(dock, nullptr)) {
        existingTab->setProperty("dockArea", int(area));
        return existingTab;
    }

    auto *tab = new QToolButton(contentDockHost_);
    tab->setText(dock->windowTitle());
    tab->setProperty("tabText", dock->windowTitle());
    tab->setProperty("dockArea", int(area));
    tab->setCursor(Qt::PointingHandCursor);
    tab->setStyleSheet(QStringLiteral(
        "QToolButton { background: #D9F2F0; border: 1px solid #7DAAA6; color: #000000; font-size: 11px; padding: 2px; }"
        "QToolButton:hover { background: #E5F2D8; }"));
    connect(tab, &QToolButton::clicked, this, [this, dock]() {
        restoreCollapsedDock(dock);
    });
    collapsedTabs_.insert(dock, tab);
    return tab;
}

/**
 * @brief Positions all collapsed edge tabs along the central dock host borders.
 * @author mozhengjie
 */
void MainWindow::positionCollapsedTabs()
{
    if (!contentDockHost_) {
        return;
    }

    int rightOffset = 8;
    int bottomOffset = 8;
    const int tabThickness = 28;
    const int tabLength = 118;
    const int spacing = 6;

    for (auto it = collapsedTabs_.begin(); it != collapsedTabs_.end(); ++it) {
        QDockWidget *dock = it.key();
        QToolButton *tab = it.value();
        if (!dock || !tab) {
            continue;
        }

        const Qt::DockWidgetArea area = collapsedAreas_.value(dock, Qt::RightDockWidgetArea);
        const QString tabText = tab->property("tabText").toString();
        if (area == Qt::BottomDockWidgetArea) {
            tab->setToolButtonStyle(Qt::ToolButtonTextOnly);
            tab->setText(tabText);
            tab->setGeometry(bottomOffset,
                             qMax(0, contentDockHost_->height() - tabThickness),
                             tabLength,
                             tabThickness);
            bottomOffset += tabLength + spacing;
        } else {
            tab->setToolButtonStyle(Qt::ToolButtonTextOnly);
            tab->setText(verticalTabText(tabText));
            tab->setGeometry(qMax(0, contentDockHost_->width() - tabThickness),
                             rightOffset,
                             tabThickness,
                             tabLength);
            rightOffset += tabLength + spacing;
        }
        tab->raise();
    }
}

/**
 * @brief Shows a dock and gives it focus; docking is constrained to the inner central host.
 * @author mozhengjie
 * @param dock Dock widget to activate.
 */
void MainWindow::activateDock(QDockWidget *dock)
{
    if (!dock) {
        return;
    }

    dock->show();
    dock->raise();
    if (dock->isFloating()) {
        dock->activateWindow();
    }
}
