#include "widgets/positionrunwidget.h"

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>
#include <QResizeEvent>

namespace {
constexpr int kInputHeight = 20;
constexpr int kInputMinWidth = 52;
constexpr int kInputPreferredWidth = 108;
constexpr int kButtonHeight = 22;
constexpr int kButtonMinWidth = 56;
constexpr int kButtonPreferredWidth = 108;
constexpr int kMinimumAdaptiveSpacing = 2;
constexpr int kMaximumAdaptiveSpacing = 14;

/**
 * @brief Removes expanding spacer items that create large blank blocks inside compact control groups.
 * @author mozhengjie
 * @param layout Layout tree to normalize.
 */
void removeExpandingSpacers(QLayout *layout)
{
    if (!layout) {
        return;
    }

    for (int index = layout->count() - 1; index >= 0; --index) {
        QLayoutItem *item = layout->itemAt(index);
        if (!item) {
            continue;
        }
        if (QLayout *childLayout = item->layout()) {
            removeExpandingSpacers(childLayout);
        }

        QSpacerItem *spacer = item->spacerItem();
        if (!spacer) {
            continue;
        }

        const QSizePolicy::Policy verticalPolicy = spacer->sizePolicy().verticalPolicy();
        const bool expandingSpacer = verticalPolicy == QSizePolicy::Expanding ||
                                     verticalPolicy == QSizePolicy::MinimumExpanding;
        if (expandingSpacer) {
            delete layout->takeAt(index);
        }
    }
}

/**
 * @brief Applies adaptive margins and spacing to nested layouts.
 * @author mozhengjie
 * @param layout Layout tree to update.
 * @param margin Outer margin.
 * @param spacing Item spacing.
 */
void applyAdaptiveSpacing(QLayout *layout, int margin, int spacing)
{
    if (!layout) {
        return;
    }

    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(spacing);
    for (int index = 0; index < layout->count(); ++index) {
        QLayoutItem *item = layout->itemAt(index);
        if (item && item->layout()) {
            applyAdaptiveSpacing(item->layout(), 0, spacing);
        }
    }
}
} // namespace

/**
 * @brief 构造定位运行面板并建立三块可拖拽调整尺寸的区域。
 * @author mozhengjie
 * @param parent 父控件指针。
 */
PositionRunWidget::PositionRunWidget(QWidget *parent)
    : QWidget(parent)
{
    rootLayout_ = new QHBoxLayout(this);
    rootLayout_->setContentsMargins(3, 3, 3, 3);
    rootLayout_->setSpacing(0);

    step1Group_ = createStep1Group();
    step2Group_ = createStep2Group();
    positionDisplayGroup_ = createPositionDisplayGroup();
    removeExpandingSpacers(step1Group_->layout());
    removeExpandingSpacers(step2Group_->layout());
    removeExpandingSpacers(positionDisplayGroup_->layout());
    rebuildLayout();
    applyStyle();
    updateSliderRangeFromLimits();
    updateResponsiveMetrics();
}

/**
 * @brief Switches the position panel layout according to the dock area.
 * @author mozhengjie
 * @param mode Target layout mode.
 */
void PositionRunWidget::setLayoutMode(LayoutMode mode)
{
    if (layoutMode_ == mode) {
        return;
    }

    layoutMode_ = mode;
    rebuildLayout();
    updateResponsiveMetrics();
}

/**
 * @brief Updates one or more input boxes bound to the same Modbus register.
 * @author mozhengjie
 * @param address Modbus register address.
 * @param value Value displayed in all bound editors.
 */
void PositionRunWidget::setRegisterValue(int address, qint64 value)
{
    const QVector<QLineEdit *> edits = registerEdits_.value(address);
    for (QLineEdit *edit : edits) {
        if (!edit) {
            continue;
        }

        QSignalBlocker blocker(edit);
        edit->setText(QString::number(value));
    }
}

/**
 * @brief Updates the enable button from Pn44 without emitting a write request.
 * @author mozhengjie
 * @param enabled true means the motor is enabled.
 */
void PositionRunWidget::setEnableState(bool enabled)
{
    if (!enableButton_) {
        return;
    }

    QSignalBlocker blocker(enableButton_);
    enableButton_->setChecked(enabled);
    enableButton_->setText(enabled ? QStringLiteral("使能") : QStringLiteral("失能"));
    enableButton_->style()->unpolish(enableButton_);
    enableButton_->style()->polish(enableButton_);
    enableButton_->update();
}

/**
 * @brief Displays the signed 32-bit current position and keeps the slider in range.
 * @author mozhengjie
 * @param value Signed 32-bit position value combined from Pn449/Pn450.
 */
void PositionRunWidget::setCurrentPosition(qint32 value)
{
    currentPosition_ = qBound(negativeLimit_, int(value), positiveLimit_);
    if (currentPositionEdit_) {
        QSignalBlocker blocker(currentPositionEdit_);
        currentPositionEdit_->setText(QString::number(value));
    }
    if (positionSlider_) {
        positionSlider_->setValue(currentPosition_);
    }
}

/**
 * @brief Marks whether current-position polling is paused by changing the display tooltip.
 * @author mozhengjie
 * @param paused true when polling is paused.
 */
void PositionRunWidget::setCurrentPositionPollingPaused(bool paused)
{
    if (!currentPositionEdit_) {
        return;
    }

    currentPositionEdit_->setToolTip(paused
                                         ? QStringLiteral("当前位置轮询已暂停，单击恢复")
                                         : QStringLiteral("单击暂停当前位置轮询"));
}

/**
 * @brief Updates adaptive spacing when the panel geometry changes.
 * @author mozhengjie
 * @param event Resize event.
 */
void PositionRunWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveMetrics();
}

/**
 * @brief Handles clicks on the read-only current-position editor.
 * @author mozhengjie
 * @param watched Watched object.
 * @param event Incoming event.
 * @return bool true when the click is handled.
 */
bool PositionRunWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == currentPositionEdit_ && event->type() == QEvent::MouseButtonPress) {
        emit currentPositionPollingToggleRequested();
        return true;
    }

    if (watched == step1ReverseButton_ || watched == step1ForwardButton_) {
        return handleStepJogButtonEvent(qobject_cast<QPushButton *>(watched), event);
    }

    return QWidget::eventFilter(watched, event);
}

/**
 * @brief Handles press/release/double-click lock behavior for step1 jog buttons.
 * @author mozhengjie
 * @param button Jog button receiving the event.
 * @param event Incoming mouse or focus event.
 * @return bool true when the event is consumed by jog logic.
 */
bool PositionRunWidget::handleStepJogButtonEvent(QPushButton *button, QEvent *event)
{
    if (!button || !event) {
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }

        if (lockedStepJogButton_ == button) {
            stopStepJogButton(button);
            return true;
        }

        if (lockedStepJogButton_ && lockedStepJogButton_ != button) {
            stopStepJogButton(lockedStepJogButton_);
        }
        startStepJogButton(button);
        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }

        if (lockedStepJogButton_ && lockedStepJogButton_ != button) {
            stopStepJogButton(lockedStepJogButton_);
        }
        lockedStepJogButton_ = button;
        pressedStepJogButton_ = nullptr;
        setStepJogButtonActive(button, true);
        emit positionStepJogCommandRequested(stepJogCommandForButton(button));
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }

        if (lockedStepJogButton_ == button) {
            return true;
        }
        if (pressedStepJogButton_ == button) {
            stopStepJogButton(button);
            return true;
        }
        return true;
    }

    if ((event->type() == QEvent::Hide || event->type() == QEvent::FocusOut)
        && (pressedStepJogButton_ == button || lockedStepJogButton_ == button)) {
        stopStepJogButton(button);
        return false;
    }

    return false;
}

/**
 * @brief Returns the Pn59 command value for a step1 jog direction button.
 * @author mozhengjie
 * @param button Direction button.
 * @return int 4 for forward, 3 for reverse, 6 for stop/default.
 */
int PositionRunWidget::stepJogCommandForButton(QPushButton *button) const
{
    if (button == step1ForwardButton_) {
        return 4;
    }
    if (button == step1ReverseButton_) {
        return 3;
    }
    return 6;
}

/**
 * @brief Starts a step1 jog action and marks the active button in light green.
 * @author mozhengjie
 * @param button Button to activate.
 */
void PositionRunWidget::startStepJogButton(QPushButton *button)
{
    if (!button) {
        return;
    }

    if (pressedStepJogButton_ && pressedStepJogButton_ != button) {
        stopStepJogButton(pressedStepJogButton_);
    }
    pressedStepJogButton_ = button;
    setStepJogButtonActive(button, true);
    emit positionStepJogCommandRequested(stepJogCommandForButton(button));
}

/**
 * @brief Stops step1 jog by writing Pn59 stop command and restoring button style.
 * @author mozhengjie
 * @param button Button to release.
 */
void PositionRunWidget::stopStepJogButton(QPushButton *button)
{
    if (!button) {
        return;
    }

    if (pressedStepJogButton_ == button) {
        pressedStepJogButton_ = nullptr;
    }
    if (lockedStepJogButton_ == button) {
        lockedStepJogButton_ = nullptr;
    }
    setStepJogButtonActive(button, false);
    emit positionStepJogCommandRequested(6);
}

/**
 * @brief Applies or clears the visual active state for a step1 jog button.
 * @author mozhengjie
 * @param button Button to restyle.
 * @param active true for pressed/locked state.
 */
void PositionRunWidget::setStepJogButtonActive(QPushButton *button, bool active)
{
    if (!button) {
        return;
    }

    button->setDown(active);
    button->setProperty("jogActive", active);
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

/**
 * @brief 创建居中显示的数值输入框。
 * @author mozhengjie
 * @param text 初始文本。
 * @param readOnly 是否只读。
 * @return QLineEdit* 数值输入框。
 */
QLineEdit *PositionRunWidget::createNumericEdit(const QString &text, bool readOnly)
{
    auto *edit = new QLineEdit(text, this);
    edit->setAlignment(Qt::AlignCenter);
    edit->setReadOnly(readOnly);
    edit->setMinimumSize(kInputMinWidth, kInputHeight);
    edit->setFixedHeight(kInputHeight);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setStyleSheet(QStringLiteral("QLineEdit { background: #FFFFFF; color: #000000; border: 1px solid #000000; font-family: '宋体'; }"
                                       "QLineEdit[readOnly=\"true\"] { background: #F7F7F7; }"));
    return edit;
}

/**
 * @brief 创建普通命令按钮。
 * @author mozhengjie
 * @param text 按钮文本。
 * @return QPushButton* 命令按钮。
 */
QPushButton *PositionRunWidget::createCommandButton(const QString &text)
{
    auto *button = new QPushButton(text, this);
    button->setMinimumSize(kButtonMinWidth, kButtonHeight);
    button->setMaximumWidth(kButtonPreferredWidth * 2);
    button->setFixedHeight(kButtonHeight);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

/**
 * @brief 创建单按钮双状态切换控件。
 * @author mozhengjie
 * @param firstText 默认状态文本。
 * @param secondText 选中状态文本。
 * @return QPushButton* 可切换按钮。
 */
QPushButton *PositionRunWidget::createToggleButton(const QString &firstText, const QString &secondText)
{
    QPushButton *button = createCommandButton(firstText);
    button->setCheckable(true);
    connect(button, &QPushButton::toggled, button, [button, firstText, secondText](bool checked) {
        button->setText(checked ? secondText : firstText);
    });
    return button;
}

/**
 * @brief 创建单按钮多状态循环切换控件，点击后按传入顺序切换显示文本。
 * @author mozhengjie
 * @param states 状态文本列表，首个状态为默认状态。
 * @return QPushButton* 多状态循环按钮。
 */
QPushButton *PositionRunWidget::createCycleButton(const QStringList &states)
{
    if (states.isEmpty()) {
        return createCommandButton(QString());
    }

    QPushButton *button = createCommandButton(states.first());
    button->setCheckable(true);
    button->setChecked(false);
    button->setProperty("cycleIndex", 0);
    connect(button, &QPushButton::clicked, button, [button, states]() {
        const int nextIndex = (button->property("cycleIndex").toInt() + 1) % states.size();
        button->setProperty("cycleIndex", nextIndex);
        button->setText(states.at(nextIndex));
        button->setChecked(nextIndex != 0);
    });
    return button;
}

/**
 * @brief Binds an editor to a Modbus register and emits a write request on Enter.
 * @author mozhengjie
 * @param edit Numeric editor to bind.
 * @param address Modbus register address.
 */
void PositionRunWidget::bindRegisterEdit(QLineEdit *edit, int address)
{
    if (!edit) {
        return;
    }

    registerEdits_[address].append(edit);
    connect(edit, &QLineEdit::returnPressed, this, [this, edit, address]() {
        bool ok = false;
        const qint64 value = edit->text().trimmed().toLongLong(&ok);
        if (!ok) {
            return;
        }

        setRegisterValue(address, value);
        emit positionRegisterWriteRequested(address, value);
    });
}

/**
 * @brief 创建 step1 点动参数区域。
 * @author mozhengjie
 * @return QWidget* step1 分组控件。
 */
QWidget *PositionRunWidget::createStep1Group()
{
    auto *group = new QGroupBox(QStringLiteral("step1"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(16, 16, 16, 13);
    layout->setSpacing(9);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(9);
    const QStringList labels = {
        QStringLiteral("位置点动速度："),
        QStringLiteral("位置点动加速度："),
        QStringLiteral("位置点动减速度：")
    };
    for (int row = 0; row < labels.size(); ++row) {
        auto *label = new QLabel(labels.at(row), group);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        QLineEdit *edit = createNumericEdit();
        bindRegisterEdit(edit, 80 + row);
        grid->addWidget(edit, row, 1);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    // step1 与 step2 保持同类留白策略，用固定缓冲行拉开“位置点动减速度”和底部按钮区。
    // 该缓冲不会随自适应布局被清除，窗口压缩时仍能避免输入框与按钮贴得过近。
    layout->addSpacing(7);
    layout->addStretch(1);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(14);
    enableButton_ = createToggleButton(QStringLiteral("失能"), QStringLiteral("使能"));
    enableButton_->setObjectName(QStringLiteral("step1EnableToggleButton"));
    connect(enableButton_, &QPushButton::toggled, this, &PositionRunWidget::positionEnableWriteRequested);
    buttonLayout->addWidget(enableButton_);
    buttonLayout->addStretch(1);
    step1ReverseButton_ = createCommandButton(QStringLiteral("反向"));
    step1ReverseButton_->installEventFilter(this);
    buttonLayout->addWidget(step1ReverseButton_);
    buttonLayout->addStretch(1);
    step1ForwardButton_ = createCommandButton(QStringLiteral("正向"));
    step1ForwardButton_->installEventFilter(this);
    buttonLayout->addWidget(step1ForwardButton_);
    layout->addLayout(buttonLayout);
    group->setMinimumSize(160, 100);
    return group;
}

/**
 * @brief 创建 step2 定位运行参数区域。
 * @author mozhengjie
 * @return QWidget* step2 分组控件。
 */
QWidget *PositionRunWidget::createStep2Group()
{
    auto *group = new QGroupBox(QStringLiteral("step2"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(16, 16, 16, 13);
    layout->setSpacing(7);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(7);
    const QStringList labels = {
        QStringLiteral("运行距离："),
        QStringLiteral("运行速度（rpm）："),
        QStringLiteral("运行加速度："),
        QStringLiteral("运行减速度："),
        QStringLiteral("等待时间：")
    };
    for (int row = 0; row < labels.size(); ++row) {
        auto *label = new QLabel(labels.at(row), group);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        const QVector<int> addresses = {83, 80, 81, 82, 85};
        QLineEdit *edit = createNumericEdit();
        bindRegisterEdit(edit, addresses.at(row));
        grid->addWidget(edit, row, 1);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addSpacing(7);
    layout->addStretch(1);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(13);
    cycleModeButton_ = createCycleButton({QStringLiteral("单次"),
                                          QStringLiteral("往返"),
                                          QStringLiteral("连续")});
    connect(cycleModeButton_, &QPushButton::clicked, this, [this]() {
        emit positionCycleModeChanged(cycleModeButton_->property("cycleIndex").toInt());
    });
    buttonLayout->addWidget(cycleModeButton_);
    buttonLayout->addStretch(1);
    directionButton_ = createToggleButton(QStringLiteral("正向"), QStringLiteral("反向"));
    connect(directionButton_, &QPushButton::toggled, this, &PositionRunWidget::positionDirectionChanged);
    buttonLayout->addWidget(directionButton_);
    buttonLayout->addStretch(1);
    runPauseButton_ = createToggleButton(QStringLiteral("运行"), QStringLiteral("暂停"));
    connect(runPauseButton_, &QPushButton::toggled, this, &PositionRunWidget::positionRunPauseChanged);
    buttonLayout->addWidget(runPauseButton_);
    layout->addLayout(buttonLayout);
    group->setMinimumSize(160, 120);
    return group;
}

/**
 * @brief 创建位置动态展示区域。
 * @author mozhengjie
 * @return QWidget* 位置动态展示分组控件。
 */
QWidget *PositionRunWidget::createPositionDisplayGroup()
{
    auto *group = new QGroupBox(QStringLiteral("位置动态展示"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(11);
    layout->addStretch(1);

    auto *currentLabel = new QLabel(QStringLiteral("当前位置"), group);
    currentLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(currentLabel);

    currentPositionEdit_ = createNumericEdit(QString::number(currentPosition_), true);
    currentPositionEdit_->setFixedWidth(kInputPreferredWidth);
    currentPositionEdit_->setCursor(Qt::PointingHandCursor);
    currentPositionEdit_->installEventFilter(this);
    setCurrentPositionPollingPaused(false);
    layout->addWidget(currentPositionEdit_, 0, Qt::AlignHCenter);

    positionSlider_ = new QSlider(Qt::Horizontal, group);
    positionSlider_->setEnabled(false);
    positionSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(positionSlider_);
    layout->addStretch(1);

    auto *limitLayout = new QHBoxLayout();
    limitLayout->setSpacing(22);

    auto *negativeLayout = new QVBoxLayout();
    auto *negativeLabel = new QLabel(QStringLiteral("负极限位置"), group);
    negativeLabel->setAlignment(Qt::AlignCenter);
    negativeLimitEdit_ = createNumericEdit(QString::number(negativeLimit_));
    negativeLimitEdit_->setValidator(new QIntValidator(INT_MIN, INT_MAX, negativeLimitEdit_));
    negativeLayout->addWidget(negativeLabel);
    negativeLayout->addWidget(negativeLimitEdit_);
    limitLayout->addLayout(negativeLayout);

    auto *positiveLayout = new QVBoxLayout();
    auto *positiveLabel = new QLabel(QStringLiteral("正极限位置"), group);
    positiveLabel->setAlignment(Qt::AlignCenter);
    positiveLimitEdit_ = createNumericEdit(QString::number(positiveLimit_));
    positiveLimitEdit_->setValidator(new QIntValidator(INT_MIN, INT_MAX, positiveLimitEdit_));
    positiveLayout->addWidget(positiveLabel);
    positiveLayout->addWidget(positiveLimitEdit_);
    limitLayout->addLayout(positiveLayout);
    layout->addLayout(limitLayout);
    layout->addStretch(1);

    connect(negativeLimitEdit_, &QLineEdit::editingFinished, this, &PositionRunWidget::updateSliderRangeFromLimits);
    connect(positiveLimitEdit_, &QLineEdit::editingFinished, this, &PositionRunWidget::updateSliderRangeFromLimits);

    group->setMinimumSize(160, 150);
    return group;
}

/**
 * @brief 应用定位运行面板的统一灰白色工业风样式。
 * @author mozhengjie
 */
void PositionRunWidget::applyStyle()
{
    setStyleSheet(QStringLiteral(
        "PositionRunWidget { background: #F4F4F4; color: #000000; font-family: '宋体'; font-size: 11px; }"
        "QGroupBox { background-color: #F4F4F4; color: #000000; border: 1px solid #000000; margin-top: 9px; font-size: 11px; font-weight: 500; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 7px; background-color: #F4F4F4; color: #000000; }"
        "QLabel { color: #000000; background: transparent; font-family: '宋体'; font-size: 11px; }"
        "QLineEdit { font-family: '宋体'; font-size: 11px; }"
        "QPushButton { background: #FFFFFF; color: #000000; border: 1px solid #000000; padding: 1px 7px; font-family: '宋体'; font-size: 11px; }"
        "QPushButton:hover { background: #E5F2D8; }"
        "QPushButton:pressed { background: #D4E8C5; }"
        "QPushButton:checked { background: #D8EFC8; border-color: #7DAAA6; }"
        "QPushButton[jogActive=\"true\"] { background: #D8EFC8; border-color: #7DAAA6; }"
        "QPushButton#step1EnableToggleButton { background: #FADDE1; border-color: #C78B91; }"
        "QPushButton#step1EnableToggleButton:checked { background: #D8EFC8; border-color: #7DAAA6; }"
        "QSlider::groove:horizontal { height: 4px; background: #FFFFFF; border: 1px solid #000000; }"
        "QSlider::handle:horizontal { width: 8px; background: #000000; margin: -6px 0; }"
        "QSplitter::handle { background: #B8B8B8; }"
    ));
}

/**
 * @brief 根据正负极限输入框刷新当前位置滑动条范围。
 * @author mozhengjie
 */
/**
 * @brief Rebuilds splitter orientation and widget order without recreating input controls.
 * @author mozhengjie
 */
void PositionRunWidget::rebuildLayout()
{
    detachLayoutGroups();

    if (layoutMode_ == LayoutMode::DockedBottom) {
        // 底部嵌入时只放宽高度压缩边界：原最小高度 50px，按 0.7 倍调整为 35px。
        step1Group_->setMinimumSize(70, 35);
        step2Group_->setMinimumSize(70, 35);
        positionDisplayGroup_->setMinimumSize(80, 35);
        mainSplitter_ = new QSplitter(Qt::Horizontal, this);
        mainSplitter_->addWidget(step1Group_);
        mainSplitter_->addWidget(step2Group_);
        mainSplitter_->addWidget(positionDisplayGroup_);
        mainSplitter_->setStretchFactor(0, 1);
        mainSplitter_->setStretchFactor(1, 1);
        mainSplitter_->setStretchFactor(2, 1);
        mainSplitter_->setSizes({85, 85, 270});
        // 面板整体底部嵌入最小高度同步由 70px 降为 49px，避免外层先卡住拖动。
        setMinimumSize(180, 49);
    } else if (layoutMode_ == LayoutMode::DockedRight) {
        // 右侧嵌入时只放宽宽度压缩边界：原最小宽度 50px，按 0.7 倍调整为 35px。
        step1Group_->setMinimumSize(35, 60);
        step2Group_->setMinimumSize(35, 80);
        positionDisplayGroup_->setMinimumSize(35, 80);
        mainSplitter_ = new QSplitter(Qt::Vertical, this);
        mainSplitter_->addWidget(positionDisplayGroup_);
        mainSplitter_->addWidget(step2Group_);
        mainSplitter_->addWidget(step1Group_);
        mainSplitter_->setStretchFactor(0, 2);
        mainSplitter_->setStretchFactor(1, 2);
        mainSplitter_->setStretchFactor(2, 1);
        mainSplitter_->setSizes({100, 90, 70});
        // 面板整体右侧嵌入最小宽度同步由 80px 降为 56px，避免外层先卡住拖动。
        setMinimumSize(56, 135);
    } else {
        step1Group_->setMinimumSize(130, 110);
        step2Group_->setMinimumSize(130, 150);
        positionDisplayGroup_->setMinimumSize(160, 130);
        mainSplitter_ = new QSplitter(Qt::Horizontal, this);
        auto *leftSplitter = new QSplitter(Qt::Vertical, mainSplitter_);
        leftSplitter->addWidget(step1Group_);
        leftSplitter->addWidget(step2Group_);
        leftSplitter->setStretchFactor(0, 1);
        leftSplitter->setStretchFactor(1, 1);
        leftSplitter->setCollapsible(0, false);
        leftSplitter->setCollapsible(1, false);

        mainSplitter_->addWidget(leftSplitter);
        mainSplitter_->addWidget(positionDisplayGroup_);
        mainSplitter_->setStretchFactor(0, 3);
        mainSplitter_->setStretchFactor(1, 2);
        leftSplitter->setSizes({120, 150});
        mainSplitter_->setSizes({120, 230});
        setMinimumSize(260, 180);
    }

    for (int index = 0; index < mainSplitter_->count(); ++index) {
        mainSplitter_->setCollapsible(index, false);
    }
    rootLayout_->addWidget(mainSplitter_);
    mainSplitter_->show();
}

/**
 * @brief Detaches persistent group widgets before replacing the splitter tree.
 * @author mozhengjie
 */
void PositionRunWidget::detachLayoutGroups()
{
    for (QWidget *group : {step1Group_, step2Group_, positionDisplayGroup_}) {
        if (group) {
            group->setParent(this);
            group->show();
        }
    }

    if (!mainSplitter_) {
        return;
    }

    rootLayout_->removeWidget(mainSplitter_);
    mainSplitter_->deleteLater();
    mainSplitter_ = nullptr;
}

/**
 * @brief Updates margins and spacing from the current group sizes to prevent crowding and large blanks.
 * @author mozhengjie
 */
void PositionRunWidget::updateResponsiveMetrics()
{
    const auto updateGroup = [](QWidget *group) {
        if (!group || !group->layout()) {
            return;
        }

        const int shortSide = qMax(1, qMin(group->width(), group->height()));
        const int spacing = qBound(kMinimumAdaptiveSpacing,
                                   shortSide / 24,
                                   kMaximumAdaptiveSpacing);
        const int margin = qBound(3, shortSide / 18, 12);
        applyAdaptiveSpacing(group->layout(), margin, spacing);
    };

    updateGroup(step1Group_);
    updateGroup(step2Group_);
    updateGroup(positionDisplayGroup_);

    if (rootLayout_) {
        rootLayout_->setContentsMargins(2, 2, 2, 2);
        rootLayout_->setSpacing(0);
    }
    if (mainSplitter_) {
        mainSplitter_->setHandleWidth(4);
    }
}

void PositionRunWidget::updateSliderRangeFromLimits()
{
    bool negativeOk = false;
    bool positiveOk = false;
    const int nextNegative = negativeLimitEdit_->text().toInt(&negativeOk);
    const int nextPositive = positiveLimitEdit_->text().toInt(&positiveOk);

    if (negativeOk && positiveOk && nextNegative < nextPositive) {
        negativeLimit_ = nextNegative;
        positiveLimit_ = nextPositive;
    }

    negativeLimitEdit_->setText(QString::number(negativeLimit_));
    positiveLimitEdit_->setText(QString::number(positiveLimit_));
    currentPosition_ = qBound(negativeLimit_, currentPosition_, positiveLimit_);
    currentPositionEdit_->setText(QString::number(currentPosition_));
    positionSlider_->setRange(negativeLimit_, positiveLimit_);
    positionSlider_->setValue(currentPosition_);
}
