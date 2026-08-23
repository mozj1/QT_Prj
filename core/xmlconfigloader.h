#ifndef XMLCONFIGLOADER_H
#define XMLCONFIGLOADER_H

#include "deviceconfig.h"

#include <QString>
#include <QVector>

class QXmlStreamReader;

/**
 * @brief 伺服型号 XML 配置加载器。
 * @author mozhengjie
 */
class XmlConfigLoader final
{
public:
    /**
     * @brief 扫描指定目录下的 XML 型号文件。
     * @author mozhengjie
     * @param directoryPath XML 文件夹路径。
     * @return QVector<DeviceConfig> 已成功解析的型号配置列表。
     */
    static QVector<DeviceConfig> scanDirectory(const QString &directoryPath);

    /**
     * @brief 从 XML 文件加载单个型号配置。
     * @author mozhengjie
     * @param filePath XML 文件路径。
     * @param config 输出的设备配置。
     * @param errorMessage 解析失败时输出错误信息。
     * @return bool 加载成功返回 true。
     */
    static bool loadFromFile(const QString &filePath, DeviceConfig *config, QString *errorMessage);

private:
    /**
     * @brief 读取 Register 节点。
     * @author mozhengjie
     * @param xml 当前定位在 Register 起始节点的 XML 流。
     * @return RegisterDefinition 参数寄存器定义。
     */
    static RegisterDefinition readRegister(QXmlStreamReader &xml);

    /**
     * @brief 读取 MonitorParameter/dataGridView 节点。
     * @author mozhengjie
     * @param xml 当前定位在 dataGridView 起始节点的 XML 流。
     * @return MonitorDefinition 监控项定义。
     */
    static MonitorDefinition readMonitorItem(QXmlStreamReader &xml);

    /**
     * @brief 对寄存器列表进行地址重复项规整。
     * @author mozhengjie
     * @param registers 原始寄存器列表。
     */
    static void normalizeDuplicateRegisters(QVector<RegisterDefinition> *registers);
};

#endif // XMLCONFIGLOADER_H
