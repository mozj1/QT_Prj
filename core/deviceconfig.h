#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include <QString>
#include <QVector>

/**
 * @brief XML 枚举菜单项定义。
 * @author mozhengjie
 */
struct MenuOption
{
    QString parameter;
    QString nameCn;
    QString nameEn;
};

/**
 * @brief XML 参数寄存器定义。
 * @author mozhengjie
 */
struct RegisterDefinition
{
    QString modbusAddr;
    QString functionCn;
    QString functionEn;
    QString columnType;
    QString parameter;
    QString defaultValue;
    QString minimum;
    QString maximum;
    QString unit;
    QString rwAttribution;
    QString remark;
    QVector<MenuOption> menuOptions;

    /**
     * @brief 判断参数是否为下拉框枚举类型。
     * @author mozhengjie
     * @return bool 下拉框类型返回 true。
     */
    bool isComboBox() const
    {
        return columnType.compare(QStringLiteral("Combox"), Qt::CaseInsensitive) == 0;
    }

    /**
     * @brief 获取参数值显示文本。
     * @author mozhengjie
     * @return QString 参数值或“参数值-菜单名称”。
     */
    QString displayParameter() const
    {
        if (!isComboBox()) {
            return parameter;
        }

        for (const MenuOption &option : menuOptions) {
            if (option.parameter == parameter) {
                const QString optionName = option.nameCn.isEmpty() ? option.nameEn : option.nameCn;
                return optionName.isEmpty() ? parameter : QStringLiteral("%1-%2").arg(parameter, optionName);
            }
        }
        return parameter;
    }
};

/**
 * @brief XML 监控项定义。
 * @author mozhengjie
 */
struct MonitorDefinition
{
    QString dataGridViewId;
    QString modbusAddr;
    QString bitOffset;
    QString nameCn;
    QString nameEn;
    QString parameter;
    QString unit;
    QString remark;
    QString readRegCount;
};

/**
 * @brief 单个电机型号 XML 的完整配置定义。
 * @author mozhengjie
 */
struct DeviceConfig
{
    QString filePath;
    QString productName;
    QString productSeries;
    QString versionModbusAddr;
    QVector<RegisterDefinition> registers;
    QVector<MonitorDefinition> monitors;

    /**
     * @brief 判断配置是否具备有效型号信息。
     * @author mozhengjie
     * @return bool 有型号名称返回 true。
     */
    bool isValid() const
    {
        return !productName.isEmpty();
    }
};

#endif // DEVICECONFIG_H
