#include "optionsdialog.h"
#include "ui_optionsdialog.h"

OptionsDialog::OptionsDialog(const AppSettings &currentSettings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    m_settings(currentSettings)
{
    ui->setupUi(this);

    // 1. コンボボックスに選択肢を追加
    ui->themeComboBox->addItem("ライト", "light");
    ui->themeComboBox->addItem("ダーク", "dark");

    // 2. 現在の設定をコンボボックスに反映
    int index = ui->themeComboBox->findData(m_settings.theme);
    if (index != -1) {
        ui->themeComboBox->setCurrentIndex(index);
    }

    // MainWindowから受け取った現在の設定値をスピンボックスに設定する
    ui->chunkSizeSpinBox->setValue(currentSettings.chunkSize);
    ui->preloadRangeSpinBox->setValue(currentSettings.preloadRange);
    ui->progressTimerSpinBox->setValue(currentSettings.progressTimerInterval);
    ui->uiTimerSpinBox->setValue(currentSettings.uiUpdateTimerInterval);
    ui->lockDocksCheckBox->setChecked(currentSettings.docksAreLocked);
    ui->syncUiStateCheckBox->setChecked(currentSettings.syncUiStateAcrossLists);
    ui->scanSubdirectoriesCheckBox->setChecked(currentSettings.scanSubdirectories);
    ui->fileScanLimitSpinBox->setValue(currentSettings.fileScanLimit);
    ui->autoUpdatePreviewsCheckBox->setChecked(currentSettings.autoUpdatePreviews);
    ui->contextMenuCheckBox->setChecked(currentSettings.contextMenuEnabled);
    ui->comboSwitchOpenFile->addItem("自動 (推奨)", QVariant::fromValue(VideoSwitchPolicy::Default));
    ui->comboSwitchOpenFile->addItem("常に切り替える", QVariant::fromValue(VideoSwitchPolicy::Always));
    ui->comboSwitchOpenFile->addItem("切り替えない", QVariant::fromValue(VideoSwitchPolicy::Never));

    // --- コンボボックスの設定ヘルパー (ラムダ) ---
    auto setupPolicyCombo = [](QComboBox* combo, VideoSwitchPolicy currentVal) {
        combo->clear();
        // ★ 修正点: QVariant::fromValue ではなく、static_cast<int> を使用する
        combo->addItem("自動 (推奨)", static_cast<int>(VideoSwitchPolicy::Default));
        combo->addItem("常に切り替える", static_cast<int>(VideoSwitchPolicy::Always));
        combo->addItem("切り替えない", static_cast<int>(VideoSwitchPolicy::Never));

        // 現在値を選択
        int index = combo->findData(static_cast<int>(currentVal));
        if (index != -1) combo->setCurrentIndex(index);
    };

    // 各コンボボックスの初期化
    setupPolicyCombo(ui->comboSwitchOpenFile, currentSettings.switchOnOpenFile);

    if (ui->comboSwitchItemClick) {
        setupPolicyCombo(ui->comboSwitchItemClick, currentSettings.switchOnItemClick);
    }
    if (ui->comboSwitchAutoPlay) {
        setupPolicyCombo(ui->comboSwitchAutoPlay, currentSettings.switchOnAutoPlay);
    }
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

AppSettings OptionsDialog::getSettings() const
{
    AppSettings newSettings;
    newSettings.chunkSize = ui->chunkSizeSpinBox->value();
    newSettings.preloadRange = ui->preloadRangeSpinBox->value();
    newSettings.progressTimerInterval = ui->progressTimerSpinBox->value();
    newSettings.uiUpdateTimerInterval = ui->uiTimerSpinBox->value();
    newSettings.docksAreLocked = ui->lockDocksCheckBox->isChecked();
    newSettings.syncUiStateAcrossLists = ui->syncUiStateCheckBox->isChecked();
    newSettings.scanSubdirectories = ui->scanSubdirectoriesCheckBox->isChecked();
    newSettings.fileScanLimit = ui->fileScanLimitSpinBox->value();
    newSettings.autoUpdatePreviews = ui->autoUpdatePreviewsCheckBox->isChecked();
    newSettings.theme = ui->themeComboBox->currentData().toString();
    newSettings.contextMenuEnabled = ui->contextMenuCheckBox->isChecked();
    newSettings.switchOnOpenFile = static_cast<VideoSwitchPolicy>(ui->comboSwitchOpenFile->currentData().toInt());

    if (ui->comboSwitchItemClick) {
        newSettings.switchOnItemClick = static_cast<VideoSwitchPolicy>(ui->comboSwitchItemClick->currentData().toInt());
    }
    if (ui->comboSwitchAutoPlay) {
        newSettings.switchOnAutoPlay = static_cast<VideoSwitchPolicy>(ui->comboSwitchAutoPlay->currentData().toInt());
    }

    return newSettings;
}
