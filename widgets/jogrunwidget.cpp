#include "widgets/jogrunwidget.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief 构造点动运行占位面板，后续可在此接入点动控制功能。
 * @author mozhengjie
 * @param parent 父控件指针。
 */
JogRunWidget::JogRunWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(7, 7, 7, 7);

    auto *label = new QLabel(QStringLiteral("点动运行窗体"), this);
    label->setAlignment(Qt::AlignCenter);
    QFont font = label->font();
    font.setFamily(QStringLiteral("宋体"));
    font.setPointSize(11);
    font.setWeight(QFont::Medium);
    label->setFont(font);
    layout->addWidget(label);

    setMinimumSize(80, 60);
    setStyleSheet(QStringLiteral(
        "JogRunWidget { background: #F4F4F4; color: #000000; border: 1px solid #000000; font-family: '宋体'; font-size: 11px; }"
        "QLabel { background: transparent; color: #000000; font-family: '宋体'; font-size: 11px; }"
    ));
}
