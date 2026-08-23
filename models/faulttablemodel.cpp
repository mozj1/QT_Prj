#include "faulttablemodel.h"

#include <QBrush>
#include <QColor>

FaultTableModel::FaultTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void FaultTableModel::setFaults(const QVector<MonitorDefinition> &faults)
{
    beginResetModel();
    rows_.clear();
    rows_.reserve(faults.size());
    for (int index = 0; index < faults.size(); ++index) {
        FaultRow row;
        row.definition = faults[index];
        row.currentValue = faults[index].parameter.isEmpty() ? QStringLiteral("0") : faults[index].parameter;
        row.active = row.currentValue == QStringLiteral("1");
        row.originalIndex = index;
        rows_.append(row);
    }
    endResetModel();
}

bool FaultTableModel::updateFaultRegisterValue(int modbusAddress, quint16 registerValue)
{
    bool updated = false;
    for (int rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
        bool addressOk = false;
        const int rowAddress = rows_[rowIndex].definition.modbusAddr.trimmed().toInt(&addressOk);
        if (!addressOk || rowAddress != modbusAddress) {
            continue;
        }

        const int bitOffset = parseBitOffset(rows_[rowIndex].definition.bitOffset);
        if (bitOffset < 0 || bitOffset >= 16) {
            continue;
        }

        const bool bitActive = ((registerValue >> bitOffset) & 0x1U) != 0;
        const QString newValue = bitActive ? QStringLiteral("1") : QStringLiteral("0");
        if (rows_[rowIndex].active == bitActive && rows_[rowIndex].currentValue == newValue) {
            continue;
        }

        rows_[rowIndex].active = bitActive;
        rows_[rowIndex].currentValue = newValue;
        const QModelIndex valueIndex = index(rowIndex, ValueColumn);
        emit dataChanged(valueIndex, valueIndex, {Qt::DisplayRole, Qt::BackgroundRole});
        updated = true;
    }
    return updated;
}

QStringList FaultTableModel::activeFaultNames() const
{
    QStringList names;
    for (const FaultRow &row : rows_) {
        if (!row.active) {
            continue;
        }

        const QString name = displayName(row.definition);
        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

int FaultTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

int FaultTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FaultTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }

    const FaultRow &row = rows_[index.row()];
    const MonitorDefinition &definition = row.definition;

    if (role == Qt::TextAlignmentRole) {
        const Qt::Alignment alignment = index.column() == NameColumn
                                            ? Qt::AlignLeft | Qt::AlignVCenter
                                            : Qt::AlignCenter;
        return static_cast<int>(alignment);
    }

    if (role == Qt::ForegroundRole) {
        return QBrush(QColor(QStringLiteral("#000000")));
    }

    if (role == Qt::BackgroundRole && index.column() == ValueColumn && row.active) {
        return QBrush(QColor(QStringLiteral("#FF4D4F")));
    }

    if (role == Qt::ToolTipRole) {
        return QStringLiteral("寄存器 %1 bit%2").arg(definition.modbusAddr, definition.bitOffset);
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case AddressColumn:
        return definition.modbusAddr;
    case BitColumn:
        return definition.bitOffset.isEmpty() ? QStringLiteral("-") : definition.bitOffset;
    case NameColumn:
        return displayName(definition);
    case ValueColumn:
        return row.currentValue;
    case RemarkColumn:
        return definition.remark.isEmpty()
                   ? QStringLiteral("寄存器 %1 bit%2").arg(definition.modbusAddr, definition.bitOffset)
                   : definition.remark;
    default:
        return {};
    }
}

Qt::ItemFlags FaultTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant FaultTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    static const QStringList headers = {
        QStringLiteral("寄存器地址"), QStringLiteral("Bit位"), QStringLiteral("故障说明"),
        QStringLiteral("当前值"), QStringLiteral("备注")};
    return headers.value(section);
}

QString FaultTableModel::displayName(const MonitorDefinition &definition)
{
    return definition.nameCn.isEmpty() ? definition.nameEn : definition.nameCn;
}

int FaultTableModel::parseBitOffset(const QString &bitOffset)
{
    bool ok = false;
    const int bit = bitOffset.trimmed().toInt(&ok);
    return ok ? bit : -1;
}
