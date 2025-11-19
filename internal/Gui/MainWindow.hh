#ifndef YPSHNS_MAINWINDOW_HH
#define YPSHNS_MAINWINDOW_HH

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPixmap>
#include <memory>
#include "PhotoHnS.hh"

namespace Yps
{
    class MainWindow : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override = default;

    private slots:
        void openContainer();
        void embedFile();
        void extractFile();
        void showKeyDialog();

    private:
        void setupUi();
        void updateMetaInfo(const std::optional<Yps::MetaData>& meta);
        void loadPreview(const QString& path);
        void clearAll();

        // UI-элементы
        QLabel       *previewLabel = nullptr;
        QLabel       *metaLabel    = nullptr;
        QLineEdit    *containerPathEdit = nullptr;
        QLineEdit    *payloadPathEdit   = nullptr;

        std::unique_ptr<Yps::PhotoHnS> hns;   // наш «рабочий» объект
        std::optional<Yps::MetaData> currentMeta;  // если уже есть внедрённые данные
        QString      currentContainerPath;
    };
} // Yps

#endif //YPSHNS_MAINWINDOW_HH