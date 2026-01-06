#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class QLabel;
class QTabWidget;
QString getLinkColorStyle(const QWidget* widget);

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

private:
    void setupUi();
    QWidget* createGeneralTab();
    QWidget* createLicenseTab();

    QTabWidget *m_tabWidget;
};

#endif // ABOUTDIALOG_H
