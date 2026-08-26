#include "parametertablemodel.h"

#include <QBrush>
#include <QColor>
#include <QStringList>

namespace {
/**
 * @brief 获取寄存器优先显示名称。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @return QString 中文优先、英文兜底的名称。
 */
QString displayName(const RegisterDefinition &definition)
{
    return definition.functionCn.isEmpty() ? definition.functionEn : definition.functionCn;
}

/**
 * @brief 生成枚举参数显示文本。
 * @author mozhengjie
 * @param option XML 枚举菜单项。
 * @return QString 参数值和菜单名称组合后的文本。
 */
QString optionDisplayText(const MenuOption &option)
{
    const QString optionName = option.nameCn.isEmpty() ? option.nameEn : option.nameCn;
    return optionName.isEmpty() ? option.parameter : QStringLiteral("%1-%2").arg(option.parameter, optionName);
}

/**
 * @brief 获取参数当前值显示文本。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @param currentValue 当前参数值。
 * @return QString 文本框值或“参数值-菜单名称”。
 */
QString displayCurrentValue(const RegisterDefinition &definition, const QString &currentValue)
{
    if (!definition.isComboBox()) {
        return currentValue;
    }

    for (const MenuOption &option : definition.menuOptions) {
        if (option.parameter == currentValue) {
            return optionDisplayText(option);
        }
    }
    return currentValue;
}

/**
 * @brief 生成枚举型参数的提示文本。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @return QString 枚举提示文本。
 */
QString comboToolTip(const RegisterDefinition &definition)
{
    QStringList options;
    for (const MenuOption &option : definition.menuOptions) {
        options.append(optionDisplayText(option));
    }
    return options.join(QStringLiteral("\n"));
}

/**
 * @brief 严格解析数值文本。
 * @author mozhengjie
 * @param text 原始文本。
 * @param value 输出数值。
 * @return bool 解析成功返回 true。
 */
bool parseNumber(const QString &text, double *value)
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
 * @brief 判断参数名称是否为保留项。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @return bool 保留项返回 true。
 */
bool isReservedDefinition(const RegisterDefinition &definition)
{
    return definition.functionCn.contains(QStringLiteral("保留"))
           || definition.functionEn.contains(QStringLiteral("reserve"), Qt::CaseInsensitive)
           || definition.functionEn.contains(QStringLiteral("reserved"), Qt::CaseInsensitive);
}

/**
 * @brief 校验普通文本框参数是否在 XML 数值范围内。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @param inputValue 输入值。
 * @return bool 无范围或范围内返回 true。
 */
bool isTextValueInRange(const RegisterDefinition &definition, const QString &inputValue)
{
    double minimum = 0.0;
    double maximum = 0.0;
    const bool hasMinimum = parseNumber(definition.minimum, &minimum);
    const bool hasMaximum = parseNumber(definition.maximum, &maximum);
    if (!hasMinimum && !hasMaximum) {
        return true;
    }

    double current = 0.0;
    if (!parseNumber(inputValue, &current)) {
        return false;
    }

    if (hasMinimum && current < minimum) {
        return false;
    }
    if (hasMaximum && current > maximum) {
        return false;
    }
    return true;
}

/**
 * @brief 将枚举输入值匹配到 XML 菜单参数值。
 * @author mozhengjie
 * @param definition 寄存器定义。
 * @param inputValue 输入值。
 * @param normalizedValue 标准化后的菜单参数值。
 * @return bool 匹配成功返回 true。
 */
bool normalizeComboValue(const RegisterDefinition &definition,
                         const QString &inputValue,
                         QString *normalizedValue)
{
    if (!normalizedValue) {
        return false;
    }

    const QString trimmedValue = inputValue.trimmed();
    for (const MenuOption &option : definition.menuOptions) {
        const QString optionName = option.nameCn.isEmpty() ? option.nameEn : option.nameCn;
        if (trimmedValue == option.parameter || trimmedValue == optionDisplayText(option)
            || trimmedValue == option.nameCn || trimmedValue == option.nameEn || trimmedValue == optionName) {
            *normalizedValue = option.parameter;
            return true;
        }
    }
    return false;
}
} // namespace

/**
 * @brief 构造参数总表模型。
 * @author mozhengjie
 * @param parent 父对象指针。
 */
ParameterTableModel::ParameterTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

/**
 * @brief 设置 XML 寄存器定义列表。
 * @author mozhengjie
 * @param registers XML 解析后的寄存器定义。
 */
void ParameterTableModel::setRegisters(const QVector<RegisterDefinition> &registers)
{
    beginResetModel();
    rows_.clear();
    rows_.reserve(registers.size());
    for (const RegisterDefinition &definition : registers) {
        ParameterRow row;
        row.definition = definition;
        row.currentValue = definition.parameter;
        rows_.append(row);
    }
    endResetModel();
}

/**
 * @brief 设置指定行参数是否被勾选，用于 QML 表格复选框交互。
 * @author mozhengjie
 * @param row 参数行号。
 * @param checked 是否勾选。
 * @return bool 设置成功返回 true。
 */
bool ParameterTableModel::setRowChecked(int row, bool checked)
{
    if (row < 0 || row >= rows_.size()) {
        return false;
    }

    const QModelIndex selectIndex = index(row, SelectColumn);
    return setData(selectIndex, checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

/**
 * @brief 本地修改参数值但不下发，离开编辑框时保持浅黄色未发送状态。
 * @author mozhengjie
 * @param row 参数行号。
 * @param value 新参数值。
 * @return bool 修改成功返回 true。
 */
bool ParameterTableModel::editLocalValue(int row, const QString &value)
{
    if (row < 0 || row >= rows_.size()) {
        return false;
    }

    const QModelIndex valueIndex = index(row, ValueColumn);
    return setData(valueIndex, value, LocalEditRole);
}

/**
 * @brief 确认参数值并触发下发请求，通常由回车键调用。
 * @author mozhengjie
 * @param row 参数行号。
 * @param value 新参数值。
 * @return bool 提交成功返回 true。
 */
bool ParameterTableModel::submitValue(int row, const QString &value)
{
    if (row < 0 || row >= rows_.size()) {
        return false;
    }

    const QModelIndex valueIndex = index(row, ValueColumn);
    return setData(valueIndex, value, SubmitEditRole);
}

/**
 * @brief 循环查找功能说明中包含关键字的下一行。
 * @author mozhengjie
 * @param keyword 搜索关键字。
 * @param startRow 起始行，查找会从下一行开始。
 * @return int 匹配行号，未找到返回 -1。
 */
int ParameterTableModel::findNextFunctionRow(const QString &keyword, int startRow) const
{
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty() || rows_.isEmpty()) {
        return -1;
    }

    const int normalizedStart = startRow >= 0 && startRow < rows_.size() ? startRow + 1 : 0;
    for (int offset = 0; offset < rows_.size(); ++offset) {
        const int row = (normalizedStart + offset) % rows_.size();
        if (displayName(rows_[row].definition).contains(trimmedKeyword, Qt::CaseInsensitive)) {
            return row;
        }
    }
    return -1;
}

/**
 * @brief 获取已勾选的寄存器定义。
 * @author mozhengjie
 * @return QVector<RegisterDefinition> 已勾选寄存器列表，parameter 字段为当前值。
 */
QVector<RegisterDefinition> ParameterTableModel::checkedRegisters() const
{
    QVector<RegisterDefinition> result;
    for (const ParameterRow &row : rows_) {
        if (row.checked) {
            RegisterDefinition definition = row.definition;
            definition.parameter = row.currentValue;
            result.append(definition);
        }
    }
    return result;
}

/**
 * @brief 获取全部寄存器定义，parameter 字段为当前表格值。
 * @author mozhengjie
 * @return QVector<RegisterDefinition> 全部寄存器列表。
 */
QVector<RegisterDefinition> ParameterTableModel::allRegisters() const
{
    QVector<RegisterDefinition> result;
    result.reserve(rows_.size());
    for (const ParameterRow &row : rows_) {
        RegisterDefinition definition = row.definition;
        definition.parameter = row.currentValue;
        result.append(definition);
    }
    return result;
}

/**
 * @brief 获取指定行的 XML 参数定义。
 * @author mozhengjie
 * @param row 参数行号。
 * @return const RegisterDefinition* 有效行返回参数定义指针，否则返回空指针。
 */
const RegisterDefinition *ParameterTableModel::registerAt(int row) const
{
    if (row < 0 || row >= rows_.size()) {
        return nullptr;
    }
    return &rows_[row].definition;
}

/**
 * @brief 获取指定行的当前参数值。
 * @author mozhengjie
 * @param row 参数行号。
 * @return QString 当前参数值。
 */
QString ParameterTableModel::currentValueAt(int row) const
{
    if (row < 0 || row >= rows_.size()) {
        return {};
    }
    return rows_[row].currentValue;
}

/**
 * @brief 判断指定行参数值是否允许编辑。
 * @author mozhengjie
 * @param row 参数行号。
 * @return bool 可编辑返回 true。
 */
bool ParameterTableModel::isValueEditable(int row) const
{
    return row >= 0 && row < rows_.size() && isEditableDefinition(rows_[row].definition);
}

/**
 * @brief 根据寄存器地址更新参数发送状态。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param sent 是否已成功发送。
 * @return bool 找到对应参数并更新状态返回 true。
 */
bool ParameterTableModel::markParameterSendState(int startAddress, bool sent)
{
    for (int rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
        bool ok = false;
        const int rowAddress = rows_[rowIndex].definition.modbusAddr.toInt(&ok);
        if (!ok || rowAddress != startAddress) {
            continue;
        }

        rows_[rowIndex].pendingSend = !sent;
        const QModelIndex valueIndex = index(rowIndex, ValueColumn);
        emit dataChanged(valueIndex,
                         valueIndex,
                         {Qt::BackgroundRole, Qt::ToolTipRole, PendingSendRole, BackgroundColorRole});
        return true;
    }
    return false;
}

/**
 * @brief 按寄存器地址更新参数当前值。
 * @author mozhengjie
 * @param startAddress 起始寄存器地址。
 * @param value 新参数值。
 * @param sent 是否已同步到伺服。
 * @return bool 找到对应参数并更新成功返回 true。
 */
bool ParameterTableModel::updateRegisterValue(int startAddress, const QString &value, bool sent)
{
    for (int rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
        bool ok = false;
        const int rowAddress = rows_[rowIndex].definition.modbusAddr.toInt(&ok);
        if (!ok || rowAddress != startAddress) {
            continue;
        }

        rows_[rowIndex].currentValue = value;
        rows_[rowIndex].definition.parameter = value;
        rows_[rowIndex].pendingSend = !sent;
        const QModelIndex valueIndex = index(rowIndex, ValueColumn);
        emit dataChanged(valueIndex, valueIndex,
                         {Qt::DisplayRole,
                          Qt::EditRole,
                          Qt::BackgroundRole,
                          Qt::ToolTipRole,
                          ValueTextRole,
                          RawValueRole,
                          PendingSendRole,
                          BackgroundColorRole});
        return true;
    }
    return false;
}

/**
 * @brief 返回模型行数。
 * @author mozhengjie
 * @param parent 父索引。
 * @return int 行数。
 */
int ParameterTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

/**
 * @brief 返回模型列数。
 * @author mozhengjie
 * @param parent 父索引。
 * @return int 列数。
 */
int ParameterTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

/**
 * @brief 返回指定单元格数据。
 * @author mozhengjie
 * @param index 单元格索引。
 * @param role 数据角色。
 * @return QVariant 单元格数据。
 */
QVariant ParameterTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }

    const ParameterRow &row = rows_[index.row()];
    const RegisterDefinition &definition = row.definition;
    if (role == RowNumberRole) {
        return index.row() + 1;
    }

    if (role == SelectedRole) {
        return row.checked;
    }

    if (role == AddressRole) {
        return definition.modbusAddr;
    }

    if (role == FunctionRole) {
        return displayName(definition);
    }

    if (role == ValueTextRole) {
        return displayCurrentValue(definition, row.currentValue);
    }

    if (role == RawValueRole) {
        return row.currentValue;
    }

    if (role == DefaultValueRole) {
        return definition.defaultValue;
    }

    if (role == UnitRole) {
        return definition.unit;
    }

    if (role == MinimumRole) {
        return definition.minimum;
    }

    if (role == MaximumRole) {
        return definition.maximum;
    }

    if (role == AttributionRole) {
        return definition.rwAttribution;
    }

    if (role == EditableRole) {
        return isEditableDefinition(definition);
    }

    if (role == PendingSendRole) {
        return row.pendingSend;
    }

    if (role == BackgroundColorRole) {
        if (index.column() == SelectColumn) {
            return QStringLiteral("#EFEFEF");
        }
        if (index.column() == ValueColumn && row.pendingSend) {
            return QStringLiteral("#FFF4B8");
        }
        if (!isEditableDefinition(definition)) {
            return QStringLiteral("#E6E6E6");
        }
        return QStringLiteral("#FFFFFF");
    }

    if (role == ComboBoxRole) {
        return definition.isComboBox() && !definition.menuOptions.isEmpty();
    }

    if (role == ComboOptionsRole) {
        QStringList options;
        for (const MenuOption &option : definition.menuOptions) {
            options.append(optionDisplayText(option));
        }
        return options;
    }

    if (role == Qt::CheckStateRole && index.column() == SelectColumn) {
        return row.checked ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::TextAlignmentRole) {
        const Qt::Alignment alignment = index.column() == FunctionColumn
                                            ? Qt::AlignLeft | Qt::AlignVCenter
                                            : Qt::AlignCenter;
        return static_cast<int>(alignment);
    }

    if (role == Qt::ForegroundRole) {
        return QBrush(QColor(QStringLiteral("#000000")));
    }

    if (role == Qt::BackgroundRole && index.column() == SelectColumn) {
        return QBrush(QColor(QStringLiteral("#EFEFEF")));
    }

    if (role == Qt::BackgroundRole && index.column() == ValueColumn && row.pendingSend) {
        return QBrush(QColor(QStringLiteral("#FFF4B8")));
    }

    if (role == Qt::BackgroundRole && !isEditableDefinition(definition)) {
        return QBrush(QColor(QStringLiteral("#E6E6E6")));
    }

    if (role == Qt::ToolTipRole) {
        QStringList toolTips;
        if (!isEditableDefinition(definition) && index.column() == ValueColumn) {
            toolTips.append(QStringLiteral("只读、保留或范围地址参数不可编辑"));
        }
        if (row.pendingSend && index.column() == ValueColumn) {
            toolTips.append(QStringLiteral("参数已修改但尚未成功发送到伺服"));
        }
        if (definition.remark.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0) {
            toolTips.append(QStringLiteral("32 位参数，占用两个连续 Modbus 寄存器"));
        }
        if (definition.isComboBox() && !definition.menuOptions.isEmpty()) {
            toolTips.append(comboToolTip(definition));
        }
        return toolTips.join(QStringLiteral("\n"));
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }

    switch (index.column()) {
    case SelectColumn:
        return {};
    case AddressColumn:
        return definition.modbusAddr;
    case FunctionColumn:
        return displayName(definition);
    case ValueColumn:
        return role == Qt::EditRole ? row.currentValue : displayCurrentValue(definition, row.currentValue);
    case DefaultColumn:
        return definition.defaultValue;
    case UnitColumn:
        return definition.unit;
    case MinimumColumn:
        return definition.minimum;
    case MaximumColumn:
        return definition.maximum;
    case AttributionColumn:
        return definition.rwAttribution;
    default:
        return {};
    }
}

/**
 * @brief 返回 QML 可访问的数据角色名称。
 * @author mozhengjie
 * @return QHash<int, QByteArray> 角色编号与名称映射。
 */
QHash<int, QByteArray> ParameterTableModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractTableModel::roleNames();
    roles[Qt::DisplayRole] = "display";
    roles[Qt::EditRole] = "edit";
    roles[RowNumberRole] = "rowNumber";
    roles[SelectedRole] = "rowChecked";
    roles[AddressRole] = "address";
    roles[FunctionRole] = "functionName";
    roles[ValueTextRole] = "valueText";
    roles[RawValueRole] = "rawValue";
    roles[DefaultValueRole] = "defaultValue";
    roles[UnitRole] = "unit";
    roles[MinimumRole] = "minimumValue";
    roles[MaximumRole] = "maximumValue";
    roles[AttributionRole] = "attribution";
    roles[EditableRole] = "editable";
    roles[PendingSendRole] = "pendingSend";
    roles[BackgroundColorRole] = "backgroundColor";
    roles[ComboBoxRole] = "comboBox";
    roles[ComboOptionsRole] = "comboOptions";
    return roles;
}

/**
 * @brief 设置指定单元格数据。
 * @author mozhengjie
 * @param index 单元格索引。
 * @param value 新数据。
 * @param role 数据角色。
 * @return bool 设置成功返回 true。
 */
bool ParameterTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return false;
    }

    ParameterRow &row = rows_[index.row()];
    if (index.column() == SelectColumn && role == Qt::CheckStateRole) {
        row.checked = value.toInt() == Qt::Checked;
        emit dataChanged(index, index, {Qt::CheckStateRole, SelectedRole});
        return true;
    }

    const bool isValueEditRole = role == Qt::EditRole || role == LocalEditRole || role == SubmitEditRole;
    if (index.column() != ValueColumn || !isValueEditRole || !isEditableDefinition(row.definition)) {
        return false;
    }

    QString normalizedValue;
    if (!normalizeInputValue(row.definition, value.toString(), &normalizedValue)) {
        return false;
    }

    const bool requestSend = role == SubmitEditRole;
    if (row.currentValue == normalizedValue) {
        if (requestSend) {
            row.pendingSend = true;
            row.definition.parameter = normalizedValue;
            emit dataChanged(index,
                             index,
                             {Qt::BackgroundRole, Qt::ToolTipRole, PendingSendRole, BackgroundColorRole});
            emit parameterValueChanged(row.definition, normalizedValue);
        }
        return true;
    }

    row.currentValue = normalizedValue;
    row.definition.parameter = normalizedValue;
    row.pendingSend = true;
    emit dataChanged(index,
                     index,
                     {Qt::DisplayRole,
                      Qt::EditRole,
                      Qt::BackgroundRole,
                      Qt::ToolTipRole,
                      ValueTextRole,
                      RawValueRole,
                      PendingSendRole,
                      BackgroundColorRole});
    if (requestSend) {
        emit parameterValueChanged(row.definition, normalizedValue);
    }
    return true;
}

/**
 * @brief 返回指定单元格交互标志。
 * @author mozhengjie
 * @param index 单元格索引。
 * @return Qt::ItemFlags 交互标志。
 */
Qt::ItemFlags ParameterTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == SelectColumn) {
        itemFlags |= Qt::ItemIsUserCheckable;
    }
    if (index.column() == ValueColumn && isValueEditable(index.row())) {
        itemFlags |= Qt::ItemIsEditable;
    }
    return itemFlags;
}

/**
 * @brief 返回表头数据。
 * @author mozhengjie
 * @param section 表头区段。
 * @param orientation 表头方向。
 * @param role 数据角色。
 * @return QVariant 表头数据。
 */
QVariant ParameterTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    static const QStringList headers = {
        QStringLiteral("选择"), QStringLiteral("寄存器地址"), QStringLiteral("功能说明"),
        QStringLiteral("参数值"), QStringLiteral("默认值"), QStringLiteral("单位"),
        QStringLiteral("最小值"), QStringLiteral("最大值"), QStringLiteral("属性")};
    return headers.value(section);
}

/**
 * @brief 判断参数定义是否满足可写入条件。
 * @author mozhengjie
 * @param definition XML 参数定义。
 * @return bool 满足单地址 RW 条件返回 true。
 */
bool ParameterTableModel::isEditableDefinition(const RegisterDefinition &definition)
{
    return definition.rwAttribution.trimmed().compare(QStringLiteral("RW"), Qt::CaseInsensitive) == 0
           && !definition.modbusAddr.contains(QLatin1Char('-'))
           && !isReservedDefinition(definition);
}

/**
 * @brief 校验并归一化用户输入的参数值。
 * @author mozhengjie
 * @param definition XML 参数定义。
 * @param inputValue 用户输入值。
 * @param normalizedValue 校验后的标准值。
 * @return bool 校验成功返回 true。
 */
bool ParameterTableModel::normalizeInputValue(const RegisterDefinition &definition,
                                             const QString &inputValue,
                                             QString *normalizedValue)
{
    if (!normalizedValue) {
        return false;
    }

    const QString trimmedValue = inputValue.trimmed();
    if (trimmedValue.isEmpty()) {
        return false;
    }

    if (definition.isComboBox() && !definition.menuOptions.isEmpty()) {
        return normalizeComboValue(definition, trimmedValue, normalizedValue);
    }

    if (!isTextValueInRange(definition, trimmedValue)) {
        return false;
    }

    *normalizedValue = trimmedValue;
    return true;
}
