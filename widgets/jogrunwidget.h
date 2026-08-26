#ifndef JOGRUNWIDGET_H
#define JOGRUNWIDGET_H

#include <QWidget>

/**
 * @brief 点动运行 QWidget 面板，供 QDockWidget 浮动/嵌入时直接承载。
 * @author mozhengjie
 */
class JogRunWidget final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造点动运行面板。
     * @author mozhengjie
     * @param parent 父控件指针。
     */
    explicit JogRunWidget(QWidget *parent = nullptr);
};

#endif // JOGRUNWIDGET_H
