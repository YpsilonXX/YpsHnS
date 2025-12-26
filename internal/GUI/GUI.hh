// internal/GUI/GUI.hh
#ifndef GUI_HH
#define GUI_HH

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

#include "PhotoHnS.hh"  // Include existing steganography headers
#include "Encryption.hh"
#include "AuthorKey.hh"

// Forward declaration for worker class (for background operations)
class StegoWorker;

// MainWindow class definition
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void loadContainer();
    void loadDataToEmbed();
    void embedData();
    void extractData();
    void showKey();
    void copyKeyToClipboard();
    void saveKeyToFile();
    void updatePreview(const QString &filePath);
    void showEmbeddedInfo();

private:
    // UI components
    QWidget *centralWidget;
    QSplitter *mainSplitter;
    QWidget *sidebarWidget;  // Left sidebar like VS Code
    QWidget *mainAreaWidget; // Right main area
    QLabel *previewLabel;    // For image preview
    QScrollArea *previewScrollArea;
    QTextEdit *infoTextEdit; // For displaying info/logs
    QLineEdit *containerPathEdit;
    QLineEdit *dataPathEdit;
    QLineEdit *outputPathEdit;
    QPushButton *embedButton;
    QPushButton *extractButton;
    QProgressBar *progressBar;

    // Steganography instance
    Yps::PhotoHnS *stego;

    // Paths
    QString currentContainerPath;
    QString currentDataPath;
    QString currentOutputPath;

    // Worker for background tasks
    StegoWorker *worker;

    // Setup UI elements
    void setupUI();
    void setupTheme();  // Dark theme like VS Code
    void logMessage(const QString &msg, bool error = false);

    // Helper to get key as string
    QString getEncryptionKey() const;
};

// Worker class for background stego operations (to avoid blocking UI)
class StegoWorker : public QObject {
    Q_OBJECT

public:
    explicit StegoWorker(Yps::PhotoHnS *stegoInstance, QObject *parent = nullptr)
        : QObject(parent), stego(stegoInstance) {}

signals:
    void embedFinished(bool success, const QString &outputPath);
    void extractFinished(bool success, const std::vector<byte> &data);
    void infoFinished(const QString &info);
    void progressUpdated(int value);

public slots:
    void performEmbed(const std::vector<byte> &data, const QString &containerPath, const QString &outputPath);
    void performExtract(const QString &containerPath);
    void getEmbeddedInfo(const QString &containerPath);

private:
    Yps::PhotoHnS *stego;
};

#endif // GUI_HH