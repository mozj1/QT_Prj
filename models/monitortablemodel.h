#ifndef MONITORTABLEMODEL_H
#define MONITORTABLEMODEL_H

#include "../core/deviceconfig.h"

#include <QAbstractTableModel>
#include <QVector>

/**
 * @brief 监控总表数据模型，负责监控项显示、勾选和置顶排序。
 * @author mozhengjie
 */
class MonitorTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        SelectColumn = 0,
        AddressColumn,
        NameColumn,
        ValueColumn,
        UnitColumn,
        RemarkColumn,
        ColumnCount
    };

    /**
     * @brief 构造监控总表模型。
     * @author mozhengjie
     * @param parent 父对象指针。
     */
    explicit MonitorTableModel(QObject *parent = nullptr);

    /**
     * @brief 设置 XML 监控项定义列表。
     * @author mozhengjie
     * @param monitors XML 解析后的监控项定义。
     */
    void setMonitors(const QVector<MonitorDefinition> &monitors);

    /**
     * @brief 获取已勾选的监控项定义。
     * @author mozhengjie
     * @return QVector<MonitorDefinition> 已勾选监控项列表。
     */
    QVector<MonitorDefinition> checkedMonitors() const;

    /**
     * @brief 按监控项定义更新当前显示值。
     * @author mozhengjie
     * @param monitor XML 监控项定义。
     * @param value 当前监控值。
     * @return bool 找到对应监控项并更新成功返回 true。
     */
    bool updateMonitorValue(const MonitorDefinition &monitor, const QString &value);

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

private:
    struct MonitorRow
    {
        MonitorDefinition definition;
        QString currentValue;
        bool checked = false;
        int originalIndex = 0;
    };

    /**
     * @brief 将已勾选监控项按地址升序移动到顶部。
     * @author mozhengjie
     */
    void sortRowsByCheckedState();

    /**
     * @brief 解析监控项地址用于排序。
     * @author mozhengjie
     * @param addressText 地址文本。
     * @return int 地址数值，无法解析时返回较大值。
     */
    static int sortableAddress(const QString &addressText);

    QVector<MonitorRow> rows_;
};

#endif // MONITORTABLEMODEL_H
