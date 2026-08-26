#include "monitortablemodel.h"

#include <algorithm>
#include <limits>

#include <QBrush>
#include <QColor>

namespace {
/**
 * @brief 获取监控项优先显示名称。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @return QString 中文优先、英文兜底的名称。
 */
QString displayName(const MonitorDefinition &definition)
{
    return definition.nameCn.isEmpty() ? definition.nameEn : definition.nameCn;
}

/**
 * @brief 组合监控项备注文本。
 * @author mozhengjie
 * @param definition 监控项定义。
 * @return QString 备注文本。
 */
QString displayRemark(const MonitorDefinition &definition)
{
    QString remark = definition.remark;
    if (!definition.bitOffset.isEmpty()) {
        remark = remark.isEmpty()
                     ? QStringLiteral("bit%1").arg(definition.bitOffset)
                     : QStringLiteral("%1; bit%2").arg(remark, definition.bitOffset);
    }
    if (!definition.readRegCount.isEmpty()) {
        remark = remark.isEmpty()
                     ? QStringLiteral("读取 %1 个寄存器").arg(definition.readRegCount)
                     : QStringLiteral("%1; 读取 %2 个寄存器").arg(remark, definition.readRegCount);
    }
    return remark;
}
} // namespace

MonitorTableModel::MonitorTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void MonitorTableModel::setMonitors(const QVector<MonitorDefinition> &monitors)
{
    beginResetModel();
    rows_.clear();
    rows_.reserve(monitors.size());
    for (int index = 0; index < monitors.size(); ++index) {
        MonitorRow row;
        row.definition = monitors[index];
        row.currentValue = monitors[index].parameter;
        row.originalIndex = index;
        rows_.append(row);
    }
    endResetModel();
}

QVector<MonitorDefinition> MonitorTableModel::checkedMonitors() const
{
    QVector<MonitorDefinition> result;
    for (const MonitorRow &row : rows_) {
        if (row.checked) {
            result.append(row.definition);
        }
    }
    return result;
}

/**
 * @brief 按监控项定义更新当前显示值。
 * @author mozhengjie
 * @param monitor XML 监控项定义。
 * @param value 当前监控值。
 * @return bool 找到对应监控项并更新成功返回 true。
 */
bool MonitorTableModel::updateMonitorValue(const MonitorDefinition &monitor, const QString &value)
{
    for (int rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
        const MonitorDefinition &definition = rows_[rowIndex].definition;
        const bool sameAddress = definition.modbusAddr == monitor.modbusAddr
                                 && definition.bitOffset == monitor.bitOffset
                                 && definition.nameCn == monitor.nameCn
                                 && definition.nameEn == monitor.nameEn;
        if (!sameAddress) {
            continue;
        }

        rows_[rowIndex].currentValue = value;
        const QModelIndex valueIndex = index(rowIndex, ValueColumn);
        emit dataChanged(valueIndex, valueIndex, {Qt::DisplayRole, ValueTextRole});
        return true;
    }
    return false;
}

bool MonitorTableModel::setRowChecked(int row, bool checked)
{
    if (row < 0 || row >= rows_.size()) {
        return false;
    }

    if (rows_[row].checked == checked) {
        return true;
    }

    rows_[row].checked = checked;
    sortRowsByCheckedState();
    return true;
}

int MonitorTableModel::findNextNameRow(const QString &keyword, int startRow) const
{
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty() || rows_.isEmpty()) {
        return -1;
    }

    const int rowCount = rows_.size();
    for (int offset = 1; offset <= rowCount; ++offset) {
        const int rowIndex = (startRow + offset + rowCount) % rowCount;
        if (displayName(rows_[rowIndex].definition).contains(trimmedKeyword, Qt::CaseInsensitive)) {
            return rowIndex;
        }
    }

    return -1;
}

int MonitorTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

int MonitorTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MonitorTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }

    const MonitorRow &row = rows_[index.row()];
    const MonitorDefinition &definition = row.definition;

    if (role == SelectedRole) {
        return row.checked;
    }

    if (role == AddressRole) {
        return definition.modbusAddr;
    }

    if (role == NameRole) {
        return displayName(definition);
    }

    if (role == ValueTextRole) {
        return row.currentValue;
    }

    if (role == UnitRole) {
        return definition.unit.isEmpty() ? QStringLiteral("-") : definition.unit;
    }

    if (role == RemarkRole) {
        return displayRemark(definition);
    }

    if (role == BackgroundColorRole) {
        if (index.column() == SelectColumn) {
            return QStringLiteral("#EFEFEF");
        }
        if (row.checked) {
            return QStringLiteral("#E5F2D8");
        }
        return QStringLiteral("#FFFFFF");
    }

    if (role == Qt::CheckStateRole && index.column() == SelectColumn) {
        return row.checked ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::TextAlignmentRole) {
        const Qt::Alignment alignment = index.column() == NameColumn
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

    if (role == Qt::BackgroundRole && row.checked) {
        return QBrush(QColor(QStringLiteral("#E5F2D8")));
    }

    if (role == Qt::ToolTipRole && !definition.bitOffset.isEmpty()) {
        return QStringLiteral("位监控项：读取寄存器 %1 后解析 bit%2").arg(definition.modbusAddr, definition.bitOffset);
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case SelectColumn:
        return {};
    case AddressColumn:
        return definition.modbusAddr;
    case NameColumn:
        return displayName(definition);
    case ValueColumn:
        return row.currentValue;
    case UnitColumn:
        return definition.unit.isEmpty() ? QStringLiteral("-") : definition.unit;
    case RemarkColumn:
        return displayRemark(definition);
    default:
        return {};
    }
}

bool MonitorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != SelectColumn || role != Qt::CheckStateRole) {
        return false;
    }
    if (index.row() < 0 || index.row() >= rows_.size()) {
        return false;
    }

    return setRowChecked(index.row(), value.toInt() == Qt::Checked);
}

QHash<int, QByteArray> MonitorTableModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractTableModel::roleNames();
    roles[SelectedRole] = "rowChecked";
    roles[AddressRole] = "address";
    roles[NameRole] = "nameText";
    roles[ValueTextRole] = "valueText";
    roles[UnitRole] = "unitText";
    roles[RemarkRole] = "remarkText";
    roles[BackgroundColorRole] = "backgroundColor";
    return roles;
}

Qt::ItemFlags MonitorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == SelectColumn) {
        itemFlags |= Qt::ItemIsUserCheckable;
    }
    return itemFlags;
}

QVariant MonitorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    static const QStringList headers = {
        QStringLiteral("选择"), QStringLiteral("寄存器地址"), QStringLiteral("监控名称"),
        QStringLiteral("当前值"), QStringLiteral("单位"), QStringLiteral("备注")};
    return headers.value(section);
}

void MonitorTableModel::sortRowsByCheckedState()
{
    beginResetModel();
    std::sort(rows_.begin(), rows_.end(), [](const MonitorRow &left, const MonitorRow &right) {
        if (left.checked != right.checked) {
            return left.checked;
        }

        const int leftAddress = sortableAddress(left.definition.modbusAddr);
        const int rightAddress = sortableAddress(right.definition.modbusAddr);
        if (leftAddress != rightAddress) {
            return leftAddress < rightAddress;
        }
        return left.originalIndex < right.originalIndex;
    });
    endResetModel();
}

int MonitorTableModel::sortableAddress(const QString &addressText)
{
    bool ok = false;
    const int address = addressText.toInt(&ok);
    return ok ? address : std::numeric_limits<int>::max();
}
