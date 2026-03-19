#ifndef INCLUDE_MESHTASTICKEYSDIALOG_H
#define INCLUDE_MESHTASTICKEYSDIALOG_H

#include <QDialog>

namespace Ui {
class MeshtasticKeysDialog;
}

class MeshtasticKeysDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeshtasticKeysDialog(QWidget* parent = nullptr);
    ~MeshtasticKeysDialog();

    void setKeySpecList(const QString& keySpecList);
    QString getKeySpecList() const;

private slots:
    void on_validate_clicked();
    void accept() override;

private:
    bool validateCurrentInput();

    Ui::MeshtasticKeysDialog* ui;
};

#endif // INCLUDE_MESHTASTICKEYSDIALOG_H
