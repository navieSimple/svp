#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "SettingsItemDelegate.h"
#include "SvpSettings.h"
#include <QDebug>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    QListWidgetItem *item;
    ui->setupUi(this);
    ui->listWidget->setItemDelegate(new SettingsItemDelegate());

    item = new QListWidgetItem(QIcon(":/icons/fill-color.png"), "图像", ui->listWidget);
    item = new QListWidgetItem(QIcon(":/icons/script-error.png"), "调试", ui->listWidget);

    connect(ui->buttonBox, SIGNAL(accepted()), SLOT(updateAndAccept()));
    connect(ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));
    connect(ui->listWidget, SIGNAL(currentRowChanged(int)), ui->stackedWidget, SLOT(setCurrentIndex(int)));

    ui->listWidget->setCurrentRow(0);

    switch (SvpSettings::instance()->preferCodec()) {
    case SX_RAW:
        ui->rawRadioButton->setChecked(true);
        break;
    case SX_LZ4:
        ui->lz4RadioButton->setChecked(true);
        break;
    case SX_AUTO:
        ui->smartRadioButton->setChecked(true);
        break;
    }
    ui->lz4hcCheckBox->setChecked(SvpSettings::instance()->isLZ4HC());
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

SettingsDialog *SettingsDialog::instance()
{
    static SettingsDialog *dialog = 0;
    if (!dialog)
        dialog = new SettingsDialog;
    return dialog;
}

void SettingsDialog::updateAndAccept()
{
    int codec;
    if (ui->rawRadioButton->isChecked())
        codec = SX_RAW;
    else if (ui->lz4RadioButton->isChecked())
        codec = SX_LZ4;
    else if (ui->smartRadioButton->isChecked())
        codec = SX_AUTO;
    SvpSettings::instance()->setPreferCodec(codec);
    SvpSettings::instance()->setLZ4HC(ui->lz4hcCheckBox->isChecked());
    SvpSettings::instance()->save();
    accept();
}
