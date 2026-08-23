#ifndef FAULTTABLEMODEL_H
#define FAULTTABLEMODEL_H

#include "../core/deviceconfig.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QVector>

/**
 * @brief 故障总表数据模型，负责显示寄存器 bit 故障项和报警高亮状态。
 * @author mozhengjie
 */
class FaultTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        AddressColumn = 0,
        BitColumn,
        NameColumn,
        ValueColumn,
        RemarkColumn,
        ColumnCount
    };

    /**
     * @brief 构造故障总表模型。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit FaultTableModel(QObject *parent = nullptr);

    /**
     * @brief 设置 XML 中解析出的故障 bit 定义。
     * @author mozhengjie
     * @param faults 故障 bit 定义列表。
     */
    void setFaults(const QVector<MonitorDefinition> &faults);

    /**
     * @brief 根据寄存器原始值刷新同地址下全部故障 bit 当前值。
     * @author mozhengjie
     * @param modbusAddress 寄存器地址。
     * @param registerValue 16 位寄存器原始值。
     * @return bool 找到并刷新至少一个故障 bit 时返回 true。
     */
    bool updateFaultRegisterValue(int modbusAddress, quint16 registerValue);

    /**
     * @brief 获取当前处于报警状态的故障名称。
     * @author mozhengjie
     * @return QStringList 当前值为 1 的故障名称列表。
     */
    QStringList activeFaultNames() const;

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

private:
    struct FaultRow
    {
        MonitorDefinition definition;
        QString currentValue = QStringLiteral("0");
        bool active = false;
        int originalIndex = 0;
    };

    /**
     * @brief 获取故障项优先显示名称。
     * @author mozhengjie
     * @param definition 故障 bit 定义。
     * @return QString 中文优先、英文兜底的名称。
     */
    static QString displayName(const MonitorDefinition &definition);

    /**
     * @brief 将 bit 偏移文本解析为数值。
     * @author mozhengjie
     * @param bitOffset bit 偏移文本。
     * @return int bit 偏移，解析失败返回 -1。
     */
    static int parseBitOffset(const QString &bitOffset);

    QVector<FaultRow> rows_;
};

#endif // FAULTTABLEMODEL_H
