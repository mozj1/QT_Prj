#include "parametervaluedelegate.h"

#include "../models/parametertablemodel.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>

namespace {
/**
 * @brief 生成枚举参数下拉显示文本。
 * @author mozhengjie
 * @param option XML 枚举菜单项。
 * @return QString 下拉框显示文本。
 */
QString optionDisplayText(const MenuOption &option)
{
    const QString optionName = option.nameCn.isEmpty() ? option.nameEn : option.nameCn;
    return optionName.isEmpty() ? option.parameter : QStringLiteral("%1-%2").arg(option.parameter, optionName);
}
} // namespace

/**
 * @brief 构造参数值编辑委托。
 * @author mozhengjie
 * @param parent 父对象指针。
 */
ParameterValueDelegate::ParameterValueDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

/**
 * @brief 绘制参数值单元格，确保未发送黄色背景不被行选中底色覆盖。
 * @author mozhengjie
 * @param painter 目标绘制器。
 * @param option 单元格样式选项。
 * @param index 单元格索引。
 */
void ParameterValueDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    const QVariant backgroundData = index.data(Qt::BackgroundRole);
    if (index.column() == ParameterTableModel::ValueColumn && backgroundData.canConvert<QBrush>()) {
        const QBrush backgroundBrush = qvariant_cast<QBrush>(backgroundData);
        if (backgroundBrush.color() == QColor(QStringLiteral("#FFF4B8"))) {
            QStyleOptionViewItem paintOption(option);
            paintOption.state &= ~QStyle::State_Selected;
            paintOption.palette.setBrush(QPalette::Base, backgroundBrush);
            paintOption.palette.setBrush(QPalette::Text, QBrush(QColor(QStringLiteral("#000000"))));
            QStyledItemDelegate::paint(painter, paintOption, index);
            return;
        }
    }

    QStyledItemDelegate::paint(painter, option, index);
}

/**
 * @brief 创建参数值单元格编辑器。
 * @author mozhengjie
 * @param parent 编辑器父控件。
 * @param option 单元格样式选项。
 * @param index 单元格索引。
 * @return QWidget* 编辑器控件指针。
 */
QWidget *ParameterValueDelegate::createEditor(QWidget *parent,
                                             const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    Q_UNUSED(option)

    const auto *parameterModel = qobject_cast<const ParameterTableModel *>(index.model());
    if (!parameterModel || index.column() != ParameterTableModel::ValueColumn
        || !parameterModel->isValueEditable(index.row())) {
        return nullptr;
    }

    const RegisterDefinition *definition = parameterModel->registerAt(index.row());
    if (!definition) {
        return nullptr;
    }

    if (definition->isComboBox() && !definition->menuOptions.isEmpty()) {
        auto *comboBox = new QComboBox(parent);
        comboBox->setStyleSheet(QStringLiteral(
            "QComboBox { background: #FFFFFF; border: 1px solid #777777; color: #000000; }"
            "QComboBox QAbstractItemView { color: #000000; background: #FFFFFF; }"));
        for (const MenuOption &menuOption : definition->menuOptions) {
            comboBox->addItem(optionDisplayText(menuOption), menuOption.parameter);
        }
        comboBox->installEventFilter(const_cast<ParameterValueDelegate *>(this));
        return comboBox;
    }

    auto *lineEdit = new QLineEdit(parent);
    lineEdit->setAlignment(Qt::AlignCenter);
    lineEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #FFFFFF; border: 1px solid #777777; color: #000000; }"));
    lineEdit->installEventFilter(const_cast<ParameterValueDelegate *>(this));
    return lineEdit;
}

/**
 * @brief 将模型当前值写入编辑器。
 * @author mozhengjie
 * @param editor 编辑器控件。
 * @param index 单元格索引。
 */
void ParameterValueDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QString currentValue = index.model()->data(index, Qt::EditRole).toString();

    if (auto *comboBox = qobject_cast<QComboBox *>(editor)) {
        const int currentIndex = comboBox->findData(currentValue);
        comboBox->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
        return;
    }

    if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
        lineEdit->setText(currentValue);
        lineEdit->selectAll();
    }
}

/**
 * @brief 将编辑器内容提交回模型。
 * @author mozhengjie
 * @param editor 编辑器控件。
 * @param model 目标数据模型。
 * @param index 单元格索引。
 */
void ParameterValueDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    const int editRole = editor->property("submitOnEnter").toBool()
                             ? ParameterTableModel::SubmitEditRole
                             : ParameterTableModel::LocalEditRole;

    if (auto *comboBox = qobject_cast<QComboBox *>(editor)) {
        model->setData(index, comboBox->currentData().toString(), editRole);
        return;
    }

    if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
        model->setData(index, lineEdit->text().trimmed(), editRole);
    }
}

/**
 * @brief 同步编辑器几何区域到当前单元格。
 * @author mozhengjie
 * @param editor 编辑器控件。
 * @param option 单元格样式选项。
 * @param index 单元格索引。
 */
void ParameterValueDelegate::updateEditorGeometry(QWidget *editor,
                                                 const QStyleOptionViewItem &option,
                                                 const QModelIndex &index) const
{
    Q_UNUSED(index)

    if (editor) {
        editor->setGeometry(option.rect);
    }
}

/**
 * @brief 拦截编辑器事件，失焦保存本地值，回车提交发送。
 * @author mozhengjie
 * @param object 事件来源对象。
 * @param event 事件对象。
 * @return bool 事件已处理返回 true。
 */
bool ParameterValueDelegate::eventFilter(QObject *object, QEvent *event)
{
    auto *editor = qobject_cast<QWidget *>(object);
    if (!editor) {
        return QStyledItemDelegate::eventFilter(object, event);
    }

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            editor->setProperty("submitOnEnter", true);
            emit commitData(editor);
            emit closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
            return true;
        }
    }

    if (event->type() == QEvent::FocusOut) {
        if (auto *comboBox = qobject_cast<QComboBox *>(editor)) {
            if (comboBox->view() && comboBox->view()->isVisible()) {
                return false;
            }
        }
        editor->setProperty("submitOnEnter", false);
        emit commitData(editor);
        emit closeEditor(editor, QAbstractItemDelegate::NoHint);
        return true;
    }

    return QStyledItemDelegate::eventFilter(object, event);
}
