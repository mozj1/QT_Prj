#ifndef COMMUNICATIONSETTINGSDIALOG_H
#define COMMUNICATIONSETTINGSDIALOG_H

#include "../core/communicationconfig.h"

#include <QDialog>

class QComboBox;
class QSpinBox;

/**
 * @brief Communication settings dialog for Modbus RTU port, address, baud rate and serial format.
 * @author mozhengjie
 */
class CommunicationSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Builds the communication settings dialog with the current configuration as defaults.
     * @author mozhengjie
     * @param config Current communication configuration.
     * @param parent Parent widget.
     */
    explicit CommunicationSettingsDialog(const CommunicationConfig &config, QWidget *parent = nullptr);

    /**
     * @brief Returns the communication configuration confirmed by the user.
     * @author mozhengjie
     * @return CommunicationConfig Confirmed communication configuration.
     */
    CommunicationConfig settings() const;

private:
    /**
     * @brief Populates available local serial ports.
     * @author mozhengjie
     * @param currentPort Current port name.
     */
    void populateSerialPorts(const QString &currentPort);

    /**
     * @brief Populates common baud-rate options.
     * @author mozhengjie
     * @param currentBaudRate Current baud rate.
     */
    void populateBaudRates(int currentBaudRate);

    /**
     * @brief Populates supported serial-format options.
     * @author mozhengjie
     * @param config Current communication configuration.
     */
    void populateFormats(const CommunicationConfig &config);

    QComboBox *portComboBox_ = nullptr;
    QSpinBox *slaveAddressSpinBox_ = nullptr;
    QComboBox *baudRateComboBox_ = nullptr;
    QComboBox *formatComboBox_ = nullptr;
    QSpinBox *timeoutSpinBox_ = nullptr;
    QSpinBox *retrySpinBox_ = nullptr;
};

#endif // COMMUNICATIONSETTINGSDIALOG_H
