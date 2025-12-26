// internal/GUI/GUI.cc
#include "GUI.hh"

#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>  // Added for QApplication and qApp
#include <QtGui/QClipboard>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtGui/QPalette>
#include <QtWidgets/QStyleFactory>
#include <QtCore/QThread>
#include <QtConcurrent/QtConcurrent>

#include <fstream>  // For saving extracted data if needed

// Implementation of MainWindow
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), stego(new Yps::PhotoHnS()), worker(nullptr) {
    setupTheme();
    setupUI();

    // Initialize worker in a separate thread
    QThread *workerThread = new QThread(this);
    worker = new StegoWorker(stego);
    worker->moveToThread(workerThread);

    // Connect signals
    connect(this, &MainWindow::destroyed, workerThread, &QThread::quit);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);

    // Connect worker signals to slots
    connect(worker, &StegoWorker::embedFinished, this, [this](bool success, const QString &outputPath) {
        progressBar->setValue(100);
        if (success) {
            logMessage("Embedding successful. Output: " + outputPath);
            updatePreview(outputPath);
        } else {
            logMessage("Embedding failed.", true);
        }
    });
    connect(worker, &StegoWorker::extractFinished, this, [this](bool success, const std::vector<byte> &data) {
        progressBar->setValue(100);
        if (success) {
            // Save extracted data to file
            QString savePath = QFileDialog::getSaveFileName(this, "Save Extracted Data", "", "All Files (*)");
            if (!savePath.isEmpty()) {
                std::ofstream file(savePath.toStdString(), std::ios::binary);
                if (file) {
                    file.write(reinterpret_cast<const char*>(data.data()), data.size());
                    logMessage("Extraction successful. Saved to: " + savePath);
                } else {
                    logMessage("Failed to save extracted data.", true);
                }
            }
        } else {
            logMessage("Extraction failed.", true);
        }
    });
    connect(worker, &StegoWorker::infoFinished, this, [this](const QString &info) {
        infoTextEdit->append(info);
    });
    connect(worker, &StegoWorker::progressUpdated, progressBar, &QProgressBar::setValue);

    workerThread->start();
}

MainWindow::~MainWindow() {
    delete stego;
}

void MainWindow::setupUI() {
    // Menu bar
    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *fileMenu = menuBar->addMenu("File");
    fileMenu->addAction("Load Container", this, &MainWindow::loadContainer);
    fileMenu->addAction("Load Data to Embed", this, &MainWindow::loadDataToEmbed);
    fileMenu->addAction("Embed", this, &MainWindow::embedData);
    fileMenu->addAction("Extract", this, &MainWindow::extractData);
    fileMenu->addSeparator();
    fileMenu->addAction("Show Encryption Key", this, &MainWindow::showKey);
    fileMenu->addAction("Copy Key to Clipboard", this, &MainWindow::copyKeyToClipboard);
    fileMenu->addAction("Save Key to File", this, &MainWindow::saveKeyToFile);
    setMenuBar(menuBar);

    // Toolbar
    QToolBar *toolBar = new QToolBar(this);
    toolBar->addAction("Load Container", this, &MainWindow::loadContainer);
    toolBar->addAction("Embed", this, &MainWindow::embedData);
    toolBar->addAction("Extract", this, &MainWindow::extractData);
    addToolBar(toolBar);

    // Status bar
    statusBar();

    // Central widget with splitter (like VS Code layout)
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    centralLayout->addWidget(mainSplitter);

    // Sidebar (left panel)
    sidebarWidget = new QWidget();
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->addWidget(new QLabel("Controls", sidebarWidget));

    QGroupBox *inputGroup = new QGroupBox("Input Paths", sidebarWidget);
    QFormLayout *formLayout = new QFormLayout(inputGroup);
    containerPathEdit = new QLineEdit(inputGroup);
    dataPathEdit = new QLineEdit(inputGroup);
    outputPathEdit = new QLineEdit(inputGroup);
    formLayout->addRow("Container:", containerPathEdit);
    formLayout->addRow("Data to Embed:", dataPathEdit);
    formLayout->addRow("Output Path:", outputPathEdit);
    sidebarLayout->addWidget(inputGroup);

    QPushButton *loadContainerBtn = new QPushButton("Load Container", sidebarWidget);
    connect(loadContainerBtn, &QPushButton::clicked, this, &MainWindow::loadContainer);
    sidebarLayout->addWidget(loadContainerBtn);

    QPushButton *loadDataBtn = new QPushButton("Load Data", sidebarWidget);
    connect(loadDataBtn, &QPushButton::clicked, this, &MainWindow::loadDataToEmbed);
    sidebarLayout->addWidget(loadDataBtn);

    embedButton = new QPushButton("Embed Data", sidebarWidget);
    connect(embedButton, &QPushButton::clicked, this, &MainWindow::embedData);
    sidebarLayout->addWidget(embedButton);

    extractButton = new QPushButton("Extract Data", sidebarWidget);
    connect(extractButton, &QPushButton::clicked, this, &MainWindow::extractData);
    sidebarLayout->addWidget(extractButton);

    QPushButton *infoButton = new QPushButton("Show Embedded Info", sidebarWidget);
    connect(infoButton, &QPushButton::clicked, this, &MainWindow::showEmbeddedInfo);
    sidebarLayout->addWidget(infoButton);

    progressBar = new QProgressBar(sidebarWidget);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    sidebarLayout->addWidget(progressBar);

    sidebarLayout->addStretch();
    mainSplitter->addWidget(sidebarWidget);

    // Main area (right panel)
    mainAreaWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(mainAreaWidget);
    mainLayout->addWidget(new QLabel("Preview & Info", mainAreaWidget));

    // Preview area
    previewScrollArea = new QScrollArea(mainAreaWidget);
    previewLabel = new QLabel(previewScrollArea);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewScrollArea->setWidget(previewLabel);
    previewScrollArea->setWidgetResizable(true);
    mainLayout->addWidget(previewScrollArea, 2);  // Larger stretch for preview

    // Info text edit
    infoTextEdit = new QTextEdit(mainAreaWidget);
    infoTextEdit->setReadOnly(true);
    mainLayout->addWidget(infoTextEdit, 1);

    mainSplitter->addWidget(mainAreaWidget);

    // Set splitter sizes (sidebar smaller)
    mainSplitter->setSizes({200, 800});
}

void MainWindow::setupTheme() {
    // Dark theme similar to VS Code
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    QApplication::setPalette(darkPalette);
}

void MainWindow::loadContainer() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Container File", "", "Images (*.png *.jpg *.jpeg)");
    if (!filePath.isEmpty()) {
        currentContainerPath = filePath;
        containerPathEdit->setText(filePath);
        updatePreview(filePath);
        showEmbeddedInfo();
        logMessage("Container loaded: " + filePath);
    }
}

void MainWindow::loadDataToEmbed() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Data to Embed", "", "All Files (*)");
    if (!filePath.isEmpty()) {
        currentDataPath = filePath;
        dataPathEdit->setText(filePath);
        logMessage("Data loaded: " + filePath);
    }
}

void MainWindow::embedData() {
    if (currentContainerPath.isEmpty() || currentDataPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please load container and data files.");
        return;
    }

    currentOutputPath = QFileDialog::getSaveFileName(this, "Save Output File", "", "Images (*.png *.jpg *.jpeg)");
    if (currentOutputPath.isEmpty()) return;
    outputPathEdit->setText(currentOutputPath);

    // Load data to embed
    QFile dataFile(currentDataPath);
    if (!dataFile.open(QIODevice::ReadOnly)) {
        logMessage("Failed to open data file.", true);
        return;
    }
    QByteArray dataBytes = dataFile.readAll();
    std::vector<byte> data(dataBytes.begin(), dataBytes.end());

    // Run in worker
    progressBar->setValue(0);
    QMetaObject::invokeMethod(worker, "performEmbed", Qt::QueuedConnection,
                              Q_ARG(std::vector<byte>, data),
                              Q_ARG(QString, currentContainerPath),
                              Q_ARG(QString, currentOutputPath));
}

void MainWindow::extractData() {
    if (currentContainerPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please load container file.");
        return;
    }

    // Run in worker
    progressBar->setValue(0);
    QMetaObject::invokeMethod(worker, "performExtract", Qt::QueuedConnection,
                              Q_ARG(QString, currentContainerPath));
}

void MainWindow::showEmbeddedInfo() {
    if (currentContainerPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please load container file.");
        return;
    }

    // Run in worker
    QMetaObject::invokeMethod(worker, "getEmbeddedInfo", Qt::QueuedConnection,
                              Q_ARG(QString, currentContainerPath));
}

void MainWindow::showKey() {
    QString keyStr = getEncryptionKey();
    QMessageBox::information(this, "Encryption Key", keyStr);
}

void MainWindow::copyKeyToClipboard() {
    QString keyStr = getEncryptionKey();
    QApplication::clipboard()->setText(keyStr);
    logMessage("Key copied to clipboard.");
}

void MainWindow::saveKeyToFile() {
    QString savePath = QFileDialog::getSaveFileName(this, "Save Key", "", "Text Files (*.txt)");
    if (!savePath.isEmpty()) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << getEncryptionKey();
            file.close();
            logMessage("Key saved to: " + savePath);
        } else {
            logMessage("Failed to save key.", true);
        }
    }
}

void MainWindow::updatePreview(const QString &filePath) {
    QImage image(filePath);
    if (!image.isNull()) {
        previewLabel->setPixmap(QPixmap::fromImage(image.scaled(previewScrollArea->size(), Qt::KeepAspectRatio)));
    } else {
        previewLabel->setText("Preview not available or invalid image.");
    }
}

void MainWindow::logMessage(const QString &msg, bool error) {
    QString color = error ? "<font color='red'>" : "<font color='green'>";
    infoTextEdit->append(color + msg + "</font>");
}

QString MainWindow::getEncryptionKey() const {
    auto keyArray = Yps::AuthorKey::getInstance().get_key();
    QString keyStr;
    for (auto b : keyArray) {
        keyStr += QString("%1").arg(static_cast<unsigned char>(b), 2, 16, QChar('0'));
    }
    return keyStr;
}

// Worker implementations
void StegoWorker::performEmbed(const std::vector<byte> &data, const QString &containerPath, const QString &outputPath) {
    // Simulate progress (can be enhanced with actual progress from stego operations if modified)
    emit progressUpdated(20);
    auto result = stego->embed(data, containerPath.toStdString(), outputPath.toStdString());
    emit progressUpdated(80);
    bool success = result.has_value();
    emit embedFinished(success, QString::fromStdString(result.value_or("")));
}

void StegoWorker::performExtract(const QString &containerPath) {
    emit progressUpdated(20);
    auto result = stego->extract(containerPath.toStdString());
    emit progressUpdated(80);
    if (result.has_value()) {
        emit extractFinished(true, result.value());
    } else {
        emit extractFinished(false, {});
    }
}

void StegoWorker::getEmbeddedInfo(const QString &containerPath) {
    // Perform extraction to access embedded data (non-destructive, as it doesn't modify the file)
    auto fullDataOpt = stego->extract(containerPath.toStdString());
    QString info;
    if (fullDataOpt.has_value()) {
        auto embedDataOpt = stego->getEmbedData();
        if (embedDataOpt.has_value()) {
            const auto& embedData = embedDataOpt.value();
            info = "Embedded data found.\nSize: " + QString::number(fullDataOpt->size()) + " bytes.\n"
                   "Type: " + (embedData.meta.container == Yps::ContainerType::PHOTO ? "PHOTO" : "UNKNOWN") + "\n"
                   "Extension: " + (embedData.meta.ext == Yps::Extension::PNG ? "PNG" : "JPEG") + "\n"
                   "LSB Mode: " + (embedData.meta.lsb_mode == Yps::LsbMode::OneBit ? "One Bit" :
                                   embedData.meta.lsb_mode == Yps::LsbMode::TwoBits ? "Two Bits" : "None") + "\n"
                   "Filename: " + QString::fromStdString(embedData.meta.filename);
        } else {
            info = "Embedded data extracted, but metadata context unavailable.";
        }
    } else {
        info = "No embedded data found or invalid container. Possible reasons:\n"
               "- The file does not contain embedded data.\n"
               "- Insufficient capacity or corruption during embedding.\n"
               "- For JPEG: Re-compression or editing may have altered DCT coefficients.\n"
               "- Try using PNG for more reliable embedding/extraction, as JPEG is lossy.";
    }
    emit infoFinished(info);
}