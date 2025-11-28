#include "MainWindow.hh"
#include "KeyDialog.hh"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPushButton>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStatusBar>
#include <array>
#include <iomanip>
#include <sstream>
#include "MainWindow.hh"

namespace Yps
{
    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), hns(std::make_unique<Yps::PhotoHnS>())
    {
        setWindowTitle("YpsHnS – стеганография");
        resize(1000, 700);
        setupUi();
        statusBar()->showMessage("Готов");

        // Тёмная тема как в VS Code (работает на Win/Mac/Linux)
        qApp->setStyle("Fusion");
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window,          QColor(30, 30, 30));
        darkPalette.setColor(QPalette::WindowText,      Qt::white);
        darkPalette.setColor(QPalette::Base,            QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase,   QColor(35, 35, 35));
        darkPalette.setColor(QPalette::Text,            Qt::white);
        darkPalette.setColor(QPalette::Button,          QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ButtonText,      Qt::white);
        darkPalette.setColor(QPalette::BrightText,      Qt::red);
        darkPalette.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        qApp->setPalette(darkPalette);

        // Немного QSS для красоты
        setStyleSheet(R"(
            QLabel { color: #d4d4d4; }
            QPushButton { background-color: #007acc; border: none; padding: 8px; border-radius: 4px; }
            QPushButton:hover { background-color: #148cd2; }
            QLineEdit { background-color: #3c3c3c; border: 1px solid #555; padding: 6px; }
        )");
    }

    void MainWindow::setupUi()
    {
        auto *central = new QWidget(this);
        setCentralWidget(central);
        auto *mainLayout = new QVBoxLayout(central);

        // ── Верхняя панель ─────────────────────────────────────
        auto *topBox = new QGroupBox("Контейнер (фото)");
        auto *topLayout = new QHBoxLayout(topBox);
        containerPathEdit = new QLineEdit;
        containerPathEdit->setReadOnly(true);
        auto *openBtn = new QPushButton("Открыть контейнер...");
        connect(openBtn, &QPushButton::clicked, this, &MainWindow::openContainer);
        topLayout->addWidget(containerPathEdit);
        topLayout->addWidget(openBtn);
        mainLayout->addWidget(topBox);

        // ── Предпросмотр изображения ─────────────────────────────
        previewLabel = new QLabel("Перетащите или откройте изображение");
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setMinimumHeight(400);
        previewLabel->setStyleSheet("background-color: #1e1e1e; border: 1px solid #444;");
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(previewLabel);
        mainLayout->addWidget(scroll, 1);

        // ── Метаданные ─────────────────────────────────────────
        metaLabel = new QLabel("Метаданные: нет");
        metaLabel->setWordWrap(true);
        metaLabel->setStyleSheet("font-family: Consolas; background: #252526; padding: 12px;");
        mainLayout->addWidget(new QGroupBox("Информация о внедрённых данных", metaLabel));

        // ── Действия ───────────────────────────────────────────
        auto *actionsBox = new QGroupBox("Действия");
        auto *actionsLayout = new QGridLayout(actionsBox);

        payloadPathEdit = new QLineEdit;
        payloadPathEdit->setReadOnly(true);
        auto *choosePayloadBtn = new QPushButton("Выбрать файл для скрытия...");
        connect(choosePayloadBtn, &QPushButton::clicked, this, [this]{
            QString path = QFileDialog::getOpenFileName(this, "Выбрать файл");
            if (!path.isEmpty()) {
                payloadPathEdit->setText(path);
            }
        });

        auto *embedBtn = new QPushButton("Внедрить → новый файл");
        connect(embedBtn, &QPushButton::clicked, this, &MainWindow::embedFile);

        auto *extractBtn = new QPushButton("Извлечь скрытый файл");
        extractBtn->setEnabled(false);            // включается только если есть meta
        connect(extractBtn, &QPushButton::clicked, this, &MainWindow::extractFile);

        auto *keyBtn = new QPushButton("Ключ шифрования...");

        connect(keyBtn, &QPushButton::clicked, this, &MainWindow::showKeyDialog);

        actionsLayout->addWidget(new QLabel("Файл для скрытия:"), 0, 0);
        actionsLayout->addWidget(payloadPathEdit, 0, 1);
        actionsLayout->addWidget(choosePayloadBtn, 0, 2);
        actionsLayout->addWidget(embedBtn, 1, 0, 1, 3);
        actionsLayout->addWidget(extractBtn, 1, 0, 1, 3);
        actionsLayout->addWidget(keyBtn, 2, 0, 1, 3);

        mainLayout->addWidget(actionsBox);
    }

    void MainWindow::openContainer()
    {
        QString path = QFileDialog::getOpenFileName(this, "Открыть контейнер", "", "Images (*.png *.jpg *.jpeg)");
        if (path.isEmpty()) return;

        hns->embed_data.reset();  // чистим старое состояние

        currentContainerPath = path;
        containerPathEdit->setText(path);
        loadPreview(path);

        auto meta = hns->tryReadMetaOnly(path.toStdString());
        updateMetaInfo(meta);  // ← теперь безопасно
        currentMeta = meta;
    }

    void MainWindow::loadPreview(const QString& path)
    {
        QPixmap pix(path);
        if (pix.isNull()) {
            previewLabel->setText("Не удалось загрузить изображение");
            return;
        }
        previewLabel->setPixmap(pix.scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void MainWindow::updateMetaInfo(const std::optional<Yps::MetaData>& meta_opt)
    {
        if (!meta_opt.has_value()) {
            metaLabel->setText("Метаданные: нет");
            currentMeta.reset();
            return;
        }

        const auto& meta = meta_opt.value();  // ← теперь безопасно

        std::ostringstream oss;
        oss << "Размер внедрённых данных: " << (meta.write_size - sizeof(Yps::MetaData)) << " байт\n"
            << "Имя файла: " << (meta.filename[0] ? meta.filename : "<без имени>") << "\n"
            << "Режим LSB: " << (meta.lsb_mode == Yps::LsbMode::OneBit ? "1 бит" : "2 бита") << "\n"
            << "Контейнер: " << (meta.ext == Yps::Extension::PNG ? "PNG" : "JPEG");

        metaLabel->setText(QString::fromStdString(oss.str()));
        currentMeta = meta_opt;  // сохраняем целиком
    }

    void MainWindow::embedFile()
    {
        if (currentContainerPath.isEmpty() || payloadPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Error", "Select container and file to hide");
            return;
        }

        // Read payload
        QFile file(payloadPathEdit->text());
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot read file");
            return;
        }
        QByteArray payloadData = file.readAll();
        std::vector<byte> data(
            reinterpret_cast<const byte*>(payloadData.constData()),
            reinterpret_cast<const byte*>(payloadData.constData() + payloadData.size())
        );

        QString savePath = QFileDialog::getSaveFileName(this, "Save container", "", "Images (*.png *.jpg *.jpeg)");
        if (savePath.isEmpty()) return;

        // Pre-create embed_data here so embed() reuses it
        hns->embed_data = std::make_unique<EmbedData>();

        // Set payload filename in meta
        std::string payload_name = QFileInfo(payloadPathEdit->text()).fileName().toStdString();
        std::strncpy(hns->embed_data->meta.filename, payload_name.c_str(),
                     sizeof(hns->embed_data->meta.filename) - 1);
        hns->embed_data->meta.filename[sizeof(hns->embed_data->meta.filename) - 1] = '\0';

        // Call embed (it will reuse embed_data and set container_full_path)
        auto result = hns->embed(data, currentContainerPath.toStdString(), savePath.toStdString());

        if (result) {
            QMessageBox::information(this, "Success", "Data successfully embedded!");
            hns->embed_data.reset();
            currentContainerPath = savePath;
            containerPathEdit->setText(savePath);
            loadPreview(savePath);
            updateMetaInfo(hns->tryReadMetaOnly(savePath.toStdString()));
            openContainer();
        } else {
            QMessageBox::critical(this, "Error", "Not enough capacity or file error");
        }
    }

    void MainWindow::extractFile()
    {
        if (!currentMeta || currentContainerPath.isEmpty()) return;

        QString defaultName = QString::fromStdString(currentMeta->filename);
        QString savePath = QFileDialog::getSaveFileName(this,
            "Извлечь файл", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
            defaultName);

        if (savePath.isEmpty()) return;

        auto optData = hns->extract(currentContainerPath.toStdString());
        if (!optData) {
            QMessageBox::critical(this, "Ошибка", "Не удалось извлечь данные");
            return;
        }

        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось записать файл");
            return;
        }
        file.write(reinterpret_cast<const char*>(optData->data()), optData->size());
        QMessageBox::information(this, "Готово", "Файл успешно извлечён!");
    }

    void MainWindow::clearAll()
    {
        currentMeta.reset();
        currentContainerPath.clear();
        payloadPathEdit->clear();
        previewLabel->clear();
        metaLabel->setText("Метаданные: нет");
    }

    void MainWindow::showKeyDialog()
    {
        KeyDialog dialog(this);
        dialog.exec();
    }
} // Yps