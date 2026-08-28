#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QSize>
#include <QSet>
#include <QVector>

class AppController;
class QComboBox;
class QDockWidget;
class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QQuickWidget;
class QSpinBox;
class QToolButton;
class QWidget;
class PositionRunWidget;

/**
 * @brief Servo debug main window; keeps fixed shell areas outside the run-panel dock host.
 * @author mozhengjie
 */
class MainWindow final : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(int activePage READ activePage NOTIFY activePageChanged)

public:
    /**
     * @brief Builds the fixed top/left/bottom shell and the central dock workspace.
     * @author mozhengjie
     * @param appController Shared application controller for UI and Modbus operations.
     * @param parent Parent widget.
     */
    explicit MainWindow(AppController *appController, QWidget *parent = nullptr);

    /**
     * @brief Returns the currently selected central page index.
     * @author mozhengjie
     * @return Current page index used by the central QML content.
     */
    int activePage() const;

    /**
     * @brief Switches the central content page and updates page-specific controller state.
     * @author mozhengjie
     * @param pageIndex Target page index: 0 parameter, 1 monitor, 2 fault, 3 oscilloscope.
     */
    Q_INVOKABLE void setActivePage(int pageIndex);

    /**
     * @brief Shows and activates the position-run dock panel.
     * @author mozhengjie
     */
    Q_INVOKABLE void showPositionDock();

    /**
     * @brief Shows and activates the jog-run dock panel.
     * @author mozhengjie
     */
    Q_INVOKABLE void showJogDock();

signals:
    /**
     * @brief Emitted when the central page index changes.
     * @author mozhengjie
     */
    void activePageChanged();

protected:
    /**
     * @brief Repositions collapsed dock tabs when the central dock host changes size.
     * @author mozhengjie
     * @param watched Watched object.
     * @param event Event to process.
     * @return true when the event is fully handled.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *createShellWidget();
    QWidget *createTopBar();
    QWidget *createLeftPanel();
    QWidget *createBottomStatusBar();
    QMainWindow *createContentDockHost();
    QQuickWidget *createQuickWidget(const QUrl &sourceUrl, const QString &objectName);
    QDockWidget *createRunDock(const QString &title, QWidget *contentWidget, const QString &objectName);
    QPushButton *createShellButton(const QString &text);
    void configureShellStyle();
    void configureConnections();
    void configureInitialWindowGeometry();
    void openCommunicationSettings();
    void showTransientMessage(const QString &title, const QString &message);
    void refreshModelSelector();
    void refreshCurrentModelSelection();
    void refreshPageButtons();
    void refreshPageActionGroups();
    void refreshConnectionButton();
    void refreshMonitorControls();
    void refreshStatusLabels();
    void refreshProgressBar();
    void refreshServoStateLabel();
    void activateDock(QDockWidget *dock);
    void showRunDock(QDockWidget *dock);
    void closeRunDock(QDockWidget *dock);
    void restoreDockDefaultFloatingSize(QDockWidget *dock);
    void applyRunDockDefaultEmbeddedSize(QDockWidget *dock, Qt::DockWidgetArea area);
    void placeDockForUserOpen(QDockWidget *dock);
    void handleRunDockTopLevelChanged(QDockWidget *dock, bool floating);
    void handleRunDockLocationChanged(QDockWidget *dock, Qt::DockWidgetArea area);
    void updatePositionDockLayout();
    void collapseRunDock(QDockWidget *dock);
    void restoreCollapsedDock(QDockWidget *dock);
    QToolButton *ensureCollapsedTab(QDockWidget *dock, Qt::DockWidgetArea area);
    void positionCollapsedTabs();

    AppController *appController_ = nullptr;
    QMainWindow *contentDockHost_ = nullptr;
    QQuickWidget *centralQuickWidget_ = nullptr;
    QDockWidget *positionDock_ = nullptr;
    QDockWidget *jogDock_ = nullptr;
    PositionRunWidget *positionRunWidget_ = nullptr;
    QComboBox *modelSelector_ = nullptr;
    QPushButton *connectionButton_ = nullptr;
    QWidget *parameterActionsWidget_ = nullptr;
    QWidget *monitorActionsWidget_ = nullptr;
    QSpinBox *monitorIntervalSpinBox_ = nullptr;
    QPushButton *monitorToggleButton_ = nullptr;
    QLabel *connectionStatusLabel_ = nullptr;
    QLabel *selectedModelStatusLabel_ = nullptr;
    QLabel *servoStateLabel_ = nullptr;
    QLabel *operationStatusLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QHash<QDockWidget *, QToolButton *> collapsedTabs_;
    QHash<QDockWidget *, Qt::DockWidgetArea> collapsedAreas_;
    QHash<QDockWidget *, QSize> collapsedDockSizes_;
    QSet<QDockWidget *> suppressEmbeddedDefaultResizeDocks_;
    QVector<QPushButton *> pageButtons_;
    int activePage_ = 0;
};

#endif // MAINWINDOW_H
