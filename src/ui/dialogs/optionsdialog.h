#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>
#include "settingsmanager.h"

namespace Ui {
class OptionsDialog;
}

class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    // ▼▼▼ コンストラクタを修正 ▼▼▼
    explicit OptionsDialog(const AppSettings &currentSettings, QWidget *parent = nullptr);
    ~OptionsDialog();

    // ▼▼▼ 新しい設定値を取得するための関数を追加 ▼▼▼
    AppSettings getSettings() const;

private:
    Ui::OptionsDialog *ui;
    AppSettings m_settings;
};

#endif // OPTIONSDIALOG_H
