#ifndef POSITIONRUNWIDGET_H
#define POSITIONRUNWIDGET_H

#include <QWidget>

#include <QHash>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QEvent;
class QPushButton;
class QSlider;
class QSplitter;
class QHBoxLayout;
class QVBoxLayout;
class QGridLayout;

/**
 * @brief 定位运行 QWidget 面板，供 QDockWidget 浮动/嵌入时直接承载。
 * @author mozhengjie
 */
class PositionRunWidget final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造定位运行面板并初始化 step1、step2 和位置动态展示区域。
     * @author mozhengjie
     * @param parent 父控件指针。
     */
    explicit PositionRunWidget(QWidget *parent = nullptr);

    enum class LayoutMode {
        FloatingDefault,
        DockedBottom,
        DockedRight
    };

    void setLayoutMode(LayoutMode mode);
    void setRegisterValue(int address, qint64 value);
    void setEnableState(bool enabled);
    void setCurrentPosition(qint32 value);
    void setCurrentPositionPollingPaused(bool paused);

signals:
    void positionRegisterWriteRequested(int address, qint64 value);
    void positionEnableWriteRequested(bool enabled);
    void positionCycleModeChanged(int modeIndex);
    void positionDirectionChanged(bool reverse);
    void positionRunPauseChanged(bool paused);
    void positionStepJogCommandRequested(int commandValue);
    void currentPositionPollingToggleRequested();

protected:
    /**
     * @brief Updates responsive spacing when the panel is resized.
     * @author mozhengjie
     * @param event Resize event.
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Catches clicks on the current-position display to pause or resume polling.
     * @author mozhengjie
     * @param watched Watched object.
     * @param event Event to process.
     * @return bool true when the click is consumed.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *createNumericEdit(const QString &text = QString(), bool readOnly = false);
    QPushButton *createCommandButton(const QString &text);
    QPushButton *createToggleButton(const QString &firstText, const QString &secondText);
    QPushButton *createCycleButton(const QStringList &states);
    void bindRegisterEdit(QLineEdit *edit, int address);
    bool handleStepJogButtonEvent(QPushButton *button, QEvent *event);
    int stepJogCommandForButton(QPushButton *button) const;
    void startStepJogButton(QPushButton *button);
    void stopStepJogButton(QPushButton *button);
    void setStepJogButtonActive(QPushButton *button, bool active);
    QWidget *createStep1Group();
    QWidget *createStep2Group();
    QWidget *createPositionDisplayGroup();
    void applyStyle();
    void rebuildLayout();
    void detachLayoutGroups();
    void updateResponsiveMetrics();
    void updateSliderRangeFromLimits();

    QHBoxLayout *rootLayout_ = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QVBoxLayout *step1Layout_ = nullptr;
    QVBoxLayout *step2Layout_ = nullptr;
    QVBoxLayout *positionDisplayLayout_ = nullptr;
    QGridLayout *step1Grid_ = nullptr;
    QGridLayout *step2Grid_ = nullptr;
    QHBoxLayout *step1ButtonLayout_ = nullptr;
    QHBoxLayout *step2ButtonLayout_ = nullptr;
    QHBoxLayout *limitLayout_ = nullptr;
    QWidget *step1Group_ = nullptr;
    QWidget *step2Group_ = nullptr;
    QWidget *positionDisplayGroup_ = nullptr;
    QLineEdit *currentPositionEdit_ = nullptr;
    QLineEdit *negativeLimitEdit_ = nullptr;
    QLineEdit *positiveLimitEdit_ = nullptr;
    QSlider *positionSlider_ = nullptr;
    QPushButton *enableButton_ = nullptr;
    QPushButton *step1ReverseButton_ = nullptr;
    QPushButton *step1ForwardButton_ = nullptr;
    QPushButton *cycleModeButton_ = nullptr;
    QPushButton *directionButton_ = nullptr;
    QPushButton *runPauseButton_ = nullptr;
    QPushButton *pressedStepJogButton_ = nullptr;
    QPushButton *lockedStepJogButton_ = nullptr;
    QHash<int, QVector<QLineEdit *>> registerEdits_;
    LayoutMode layoutMode_ = LayoutMode::FloatingDefault;
    int negativeLimit_ = -200000;
    int positiveLimit_ = 200000;
    int currentPosition_ = 0;
};

#endif // POSITIONRUNWIDGET_H
