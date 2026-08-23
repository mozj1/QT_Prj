#include "xmlconfigloader.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

namespace {
/**
 * @brief 获取 XML 属性值。
 * @author mozhengjie
 * @param xml 当前 XML 流。
 * @param name 属性名称。
 * @return QString 属性文本。
 */
QString attributeValue(const QXmlStreamReader &xml, const QString &name)
{
    return xml.attributes().value(name).toString().trimmed();
}
} // namespace

/**
 * @brief 扫描指定目录下的 XML 型号文件。
 * @author mozhengjie
 * @param directoryPath XML 文件夹路径。
 * @return QVector<DeviceConfig> 已成功解析的型号配置列表。
 */
QVector<DeviceConfig> XmlConfigLoader::scanDirectory(const QString &directoryPath)
{
    QVector<DeviceConfig> configs;
    const QDir directory(directoryPath);
    if (!directory.exists()) {
        return configs;
    }

    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.xml"), QStringLiteral("*.XML")},
                                                        QDir::Files | QDir::Readable,
                                                        QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        DeviceConfig config;
        QString errorMessage;
        if (loadFromFile(fileInfo.absoluteFilePath(), &config, &errorMessage)) {
            configs.append(config);
        }
    }

    std::sort(configs.begin(), configs.end(), [](const DeviceConfig &left, const DeviceConfig &right) {
        return left.productName.localeAwareCompare(right.productName) < 0;
    });
    return configs;
}

/**
 * @brief 从 XML 文件加载单个型号配置。
 * @author mozhengjie
 * @param filePath XML 文件路径。
 * @param config 输出的设备配置。
 * @param errorMessage 解析失败时输出错误信息。
 * @return bool 加载成功返回 true。
 */
bool XmlConfigLoader::loadFromFile(const QString &filePath, DeviceConfig *config, QString *errorMessage)
{
    if (!config) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("配置输出指针为空");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开 XML 文件：%1").arg(file.errorString());
        }
        return false;
    }

    DeviceConfig result;
    result.filePath = QFileInfo(filePath).absoluteFilePath();

    QXmlStreamReader xml(&file);
    bool insideMonitorParameter = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString elementName = xml.name().toString();
            if (elementName == QStringLiteral("Product")) {
                result.productName = attributeValue(xml, QStringLiteral("ProductName"));
                result.productSeries = attributeValue(xml, QStringLiteral("ProductSeries"));
                result.versionModbusAddr = attributeValue(xml, QStringLiteral("VerModbusAddr"));
            } else if (elementName == QStringLiteral("Register")) {
                result.registers.append(readRegister(xml));
            } else if (elementName == QStringLiteral("MonitorParameter")) {
                insideMonitorParameter = true;
            } else if (insideMonitorParameter && elementName == QStringLiteral("dataGridView")) {
                result.monitors.append(readMonitorItem(xml));
            }
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("MonitorParameter")) {
            insideMonitorParameter = false;
        }
    }

    if (xml.hasError()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("XML 解析失败：第 %1 行，第 %2 列，%3")
                                .arg(xml.lineNumber())
                                .arg(xml.columnNumber())
                                .arg(xml.errorString());
        }
        return false;
    }

    normalizeDuplicateRegisters(&result.registers);
    if (!result.isValid()) {
        result.productName = QFileInfo(filePath).completeBaseName();
    }

    *config = result;
    return true;
}

/**
 * @brief 读取 Register 节点。
 * @author mozhengjie
 * @param xml 当前定位在 Register 起始节点的 XML 流。
 * @return RegisterDefinition 参数寄存器定义。
 */
RegisterDefinition XmlConfigLoader::readRegister(QXmlStreamReader &xml)
{
    RegisterDefinition definition;
    definition.modbusAddr = attributeValue(xml, QStringLiteral("ModbusAddr"));
    definition.functionCn = attributeValue(xml, QStringLiteral("FunctionCN"));
    definition.functionEn = attributeValue(xml, QStringLiteral("FunctionEN"));
    definition.columnType = attributeValue(xml, QStringLiteral("ColumnType"));
    definition.parameter = attributeValue(xml, QStringLiteral("Parameter"));
    definition.defaultValue = attributeValue(xml, QStringLiteral("Default"));
    definition.minimum = attributeValue(xml, QStringLiteral("Minimum"));
    definition.maximum = attributeValue(xml, QStringLiteral("Maximum"));
    definition.unit = attributeValue(xml, QStringLiteral("Unit"));
    definition.rwAttribution = attributeValue(xml, QStringLiteral("RWAttribution"));
    definition.remark = attributeValue(xml, QStringLiteral("Remark"));

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("Menu")) {
            MenuOption option;
            option.parameter = attributeValue(xml, QStringLiteral("Parameter"));
            option.nameCn = attributeValue(xml, QStringLiteral("NameCN"));
            option.nameEn = attributeValue(xml, QStringLiteral("NameEN"));
            definition.menuOptions.append(option);
        }
        xml.skipCurrentElement();
    }

    return definition;
}

/**
 * @brief 读取 MonitorParameter/dataGridView 节点。
 * @author mozhengjie
 * @param xml 当前定位在 dataGridView 起始节点的 XML 流。
 * @return MonitorDefinition 监控项定义。
 */
MonitorDefinition XmlConfigLoader::readMonitorItem(QXmlStreamReader &xml)
{
    MonitorDefinition definition;
    definition.dataGridViewId = attributeValue(xml, QStringLiteral("dataGridViewID"));
    definition.modbusAddr = attributeValue(xml, QStringLiteral("ModbusAddr"));
    definition.bitOffset = attributeValue(xml, QStringLiteral("BitOffset"));
    definition.nameCn = attributeValue(xml, QStringLiteral("NameCN"));
    definition.nameEn = attributeValue(xml, QStringLiteral("NameEN"));
    definition.parameter = attributeValue(xml, QStringLiteral("Parameter"));
    definition.unit = attributeValue(xml, QStringLiteral("Unit"));
    definition.remark = attributeValue(xml, QStringLiteral("Remark"));
    definition.readRegCount = attributeValue(xml, QStringLiteral("ReadRegCount"));
    xml.skipCurrentElement();
    return definition;
}

/**
 * @brief 对寄存器列表进行地址重复项规整。
 * @author mozhengjie
 * @param registers 原始寄存器列表。
 */
void XmlConfigLoader::normalizeDuplicateRegisters(QVector<RegisterDefinition> *registers)
{
    if (!registers) {
        return;
    }

    QVector<RegisterDefinition> normalized;
    normalized.reserve(registers->size());
    for (const RegisterDefinition &definition : *registers) {
        const auto existing = std::find_if(normalized.begin(), normalized.end(), [&definition](const RegisterDefinition &item) {
            return item.modbusAddr == definition.modbusAddr;
        });

        if (existing == normalized.end()) {
            normalized.append(definition);
            continue;
        }

        // 同一地址既存在 Textbox 又存在 Combox 时，优先保留下拉框定义，便于界面显示枚举值。
        if (!existing->isComboBox() && definition.isComboBox()) {
            *existing = definition;
        }
    }

    *registers = normalized;
}
