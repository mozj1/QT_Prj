#ifndef PARAMETERVALUEDELEGATE_H
#define PARAMETERVALUEDELEGATE_H

#include <QStyledItemDelegate>

/**
 * @brief 参数值编辑委托，根据 XML 参数类型创建文本框或下拉框编辑器。
 * @author mozhengjie
 */
class ParameterValueDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /**
     * @brief 构造参数值编辑委托。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit ParameterValueDelegate(QObject *parent = nullptr);

    /**
     * @brief 绘制参数值单元格，确保未发送黄色背景不被行选中底色覆盖。
     * @author mozhengjie
     * @param painter 目标绘制器。
     * @param option 单元格样式选项。
     * @param index 单元格索引。
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 创建参数值单元格编辑器。
     * @author mozhengjie
     * @param parent 编辑器父控件。
     * @param option 单元格样式选项。
     * @param index 单元格索引。
     * @return QWidget* 编辑器控件指针。
     */
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    /**
     * @brief 将模型当前值写入编辑器。
     * @author mozhengjie
     * @param editor 编辑器控件。
     * @param index 单元格索引。
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief 将编辑器内容提交回模型。
     * @author mozhengjie
     * @param editor 编辑器控件。
     * @param model 目标数据模型。
     * @param index 单元格索引。
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    /**
     * @brief 同步编辑器几何区域到当前单元格。
     * @author mozhengjie
     * @param editor 编辑器控件。
     * @param option 单元格样式选项。
     * @param index 单元格索引。
     */
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    /**
     * @brief 拦截编辑器事件，失焦保存本地值，回车提交发送。
     * @author mozhengjie
     * @param object 事件来源对象。
     * @param event 事件对象。
     * @return bool 事件已处理返回 true。
     */
    bool eventFilter(QObject *object, QEvent *event) override;
};

#endif // PARAMETERVALUEDELEGATE_H
