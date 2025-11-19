#ifndef YPSHNS_KEYDIALOG_HH
#define YPSHNS_KEYDIALOG_HH

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <array>
#include "AuthorKey.hh"

namespace Yps
{
    class KeyDialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit KeyDialog(QWidget *parent = nullptr);

    private slots:
        void saveKeyToFile();

    private:
        void setupUi();
    };
} // Yps

#endif //YPSHNS_KEYDIALOG_HH