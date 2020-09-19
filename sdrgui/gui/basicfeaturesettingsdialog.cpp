#include <QColorDialog>

#include "basicfeaturesettingsdialog.h"
#include "ui_basicfeaturesettingsdialog.h"

BasicFeatureSettingsDialog::BasicFeatureSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BasicFeatureSettingsDialog),
    m_hasChanged(false)
{
    ui->setupUi(this);
    ui->title->setText(m_title);
    m_color =m_color;
    paintColor();
}

BasicFeatureSettingsDialog::~BasicFeatureSettingsDialog()
{
    delete ui;
}

void BasicFeatureSettingsDialog::setTitle(const QString& title)
{
    ui->title->blockSignals(true);
    ui->title->setText(title);
    m_title = title;
    ui->title->blockSignals(false);
}

void BasicFeatureSettingsDialog::setColor(const QColor& color)
{
    m_color = color;
    paintColor();
}

void BasicFeatureSettingsDialog::paintColor()
{
    QPixmap pm(24, 24);
    pm.fill(m_color);
    ui->colorBtn->setIcon(pm);
    ui->colorText->setText(tr("#%1%2%3")
        .arg(m_color.red(), 2, 16, QChar('0'))
        .arg(m_color.green(), 2, 16, QChar('0'))
        .arg(m_color.blue(), 2, 16, QChar('0')));
}

void BasicFeatureSettingsDialog::on_colorBtn_clicked()
{
    QColor c = m_color;
    c = QColorDialog::getColor(c, this, tr("Select Color for Channel"), QColorDialog::DontUseNativeDialog);

    if (c.isValid())
    {
        m_color = c;
        paintColor();
    }
}

void BasicFeatureSettingsDialog::on_title_editingFinished()
{
    m_title = ui->title->text();
}

void BasicFeatureSettingsDialog::accept()
{
    m_hasChanged = true;
    QDialog::accept();
}
