#ifndef PARAMETERTABLEMODEL_H
#define PARAMETERTABLEMODEL_H

#include "../core/deviceconfig.h"

#include <QAbstractTableModel>
#include <QVector>

/**
 * @brief 参数总表数据模型，负责将 XML 寄存器定义映射到表格视图。
 * @author mozhengjie
 */
class ParameterTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        SelectColumn = 0,
        AddressColumn,
        FunctionColumn,
        ValueColumn,
        DefaultColumn,
        UnitColumn,
        MinimumColumn,
        MaximumColumn,
        AttributionColumn,
        ColumnCount
    };

    enum EditDataRole {
        LocalEditRole = Qt::UserRole + 100,
        SubmitEditRole,
        RowNumberRole,
        SelectedRole,
        AddressRole,
        FunctionRole,
        ValueTextRole,
        RawValueRole,
        DefaultValueRole,
        UnitRole,
        MinimumRole,
        MaximumRole,
        AttributionRole,
        EditableRole,
        PendingSendRole,
        BackgroundColorRole,
        ComboBoxRole,
        ComboOptionsRole
    };

    /**
     * @brief 构造参数总表模型。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit ParameterTableModel(QObject *parent = nullptr);

    /**
     * @brief 设置 XML 寄存器定义列表。
     * @author mozhengjie
     * @param registers XML 解析后的寄存器定义。
     */
    void setRegisters(const QVector<RegisterDefinition> &registers);

    /**
     * @brief 获取已勾选的寄存器定义。
     * @author mozhengjie
     * @return QVector<RegisterDefinition> 已勾选寄存器列表。
     */
    QVector<RegisterDefinition> checkedRegisters() const;

    /**
     * @brief 获取全部寄存器定义，parameter 字段为当前表格值。
     * @author mozhengjie
     * @return QVector<RegisterDefinition> 全部寄存器列表。
     */
    QVector<RegisterDefinition> allRegisters() const;

    /**
     * @brief 获取指定行的 XML 参数定义。
     * @author mozhengjie
     * @param row 参数行号。
     * @return const RegisterDefinition* 有效行返回参数定义指针，否则返回空指针。
     */
    const RegisterDefinition *registerAt(int row) const;

    /**
     * @brief 获取指定行的当前参数值。
     * @author mozhengjie
     * @param row 参数行号。
     * @return QString 当前参数值。
     */
    QString currentValueAt(int row) const;

    /**
     * @brief 判断指定行参数值是否允许编辑。
     * @author mozhengjie
     * @param row 参数行号。
     * @return bool 可编辑返回 true。
     */
    bool isValueEditable(int row) const;

    /**
     * @brief 根据寄存器地址更新参数发送状态。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param sent 是否已成功发送。
     * @return bool 找到对应参数并更新状态返回 true。
     */
    bool markParameterSendState(int startAddress, bool sent);

    /**
     * @brief 按寄存器地址更新参数当前值。
     * @author mozhengjie
     * @param startAddress 起始寄存器地址。
     * @param value 新参数值。
     * @param sent 是否已同步到伺服。
     * @return bool 找到对应参数并更新成功返回 true。
     */
    bool updateRegisterValue(int startAddress, const QString &value, bool sent);

    /**
     * @brief 设置指定行参数是否被勾选，用于 QML 表格复选框交互。
     * @author mozhengjie
     * @param row 参数行号。
     * @param checked 是否勾选。
     * @return bool 设置成功返回 true。
     */
    Q_INVOKABLE bool setRowChecked(int row, bool checked);

    /**
     * @brief 本地修改参数值但不下发，离开编辑框时保持浅黄色未发送状态。
     * @author mozhengjie
     * @param row 参数行号。
     * @param value 新参数值。
     * @return bool 修改成功返回 true。
     */
    Q_INVOKABLE bool editLocalValue(int row, const QString &value);

    /**
     * @brief 确认参数值并触发下发请求，通常由回车键调用。
     * @author mozhengjie
     * @param row 参数行号。
     * @param value 新参数值。
     * @return bool 提交成功返回 true。
     */
    Q_INVOKABLE bool submitValue(int row, const QString &value);

    /**
     * @brief 循环查找功能说明中包含关键字的下一行。
     * @author mozhengjie
     * @param keyword 搜索关键字。
     * @param startRow 起始行，查找会从下一行开始。
     * @return int 匹配行号，未找到返回 -1。
     */
    Q_INVOKABLE int findNextFunctionRow(const QString &keyword, int startRow) const;

    /**
     * @brief 返回模型行数。
     * @author mozhengjie
     * @param parent 父索引。
     * @return int 行数。
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 返回模型列数。
     * @author mozhengjie
     * @param parent 父索引。
     * @return int 列数。
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 返回指定单元格数据。
     * @author mozhengjie
     * @param index 单元格索引。
     * @param role 数据角色。
     * @return QVariant 单元格数据。
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief 返回 QML 可访问的数据角色名称。
     * @author mozhengjie
     * @return QHash<int, QByteArray> 角色编号与名称映射。
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 设置指定单元格数据。
     * @author mozhengjie
     * @param index 单元格索引。
     * @param value 新数据。
     * @param role 数据角色。
     * @return bool 设置成功返回 true。
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /**
     * @brief 返回指定单元格交互标志。
     * @author mozhengjie
     * @param index 单元格索引。
     * @return Qt::ItemFlags 交互标志。
     */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /**
     * @brief 返回表头数据。
     * @author mozhengjie
     * @param section 表头区段。
     * @param orientation 表头方向。
     * @param role 数据角色。
     * @return QVariant 表头数据。
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    /**
     * @brief 参数值通过回车确认需要发送时发出。
     * @author mozhengjie
     * @param definition 已更新当前值的参数定义。
     * @param newValue 新参数值。
     */
    void parameterValueChanged(const RegisterDefinition &definition, const QString &newValue);

private:
    struct ParameterRow
    {
        RegisterDefinition definition;
        QString currentValue;
        bool checked = false;
        bool pendingSend = false;
    };

    /**
     * @brief 判断参数定义是否满足可写入条件。
     * @author mozhengjie
     * @param definition XML 参数定义。
     * @return bool 满足单地址 RW 条件返回 true。
     */
    static bool isEditableDefinition(const RegisterDefinition &definition);

    /**
     * @brief 校验并归一化用户输入的参数值。
     * @author mozhengjie
     * @param definition XML 参数定义。
     * @param inputValue 用户输入值。
     * @param normalizedValue 校验后的标准值。
     * @return bool 校验成功返回 true。
     */
    static bool normalizeInputValue(const RegisterDefinition &definition,
                                    const QString &inputValue,
                                    QString *normalizedValue);

    QVector<ParameterRow> rows_;
};

#endif // PARAMETERTABLEMODEL_H
