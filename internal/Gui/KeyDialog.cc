#include "KeyDialog.hh"
#include <QFileDialog>
#include <QMessageBox>
#include <QTextEdit>
#include <iomanip>
#include <sstream>

#include "KeyDialog.hh"

namespace Yps
{
    KeyDialog::KeyDialog(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Ключ шифрования");
        resize(520, 300);
        setupUi();
    }

    void KeyDialog::setupUi()
    {
        auto *layout = new QVBoxLayout(this);

        QTextEdit *keyEdit = new QTextEdit(this);
        keyEdit->setReadOnly(true);
        keyEdit->setFont(QFont("Consolas", 11));

        // Получаем ключ и выводим в hex (как в VS Code)
        const auto& key = Yps::AuthorKey::getInstance().get_key();
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned char b : key)
            oss << std::setw(2) << static_cast<int>(b) << ' ';
        keyEdit->setText(QString::fromStdString(oss.str()));

        auto *saveBtn = new QPushButton("Сохранить ключ в файл...", this);
        connect(saveBtn, &QPushButton::clicked, this, &KeyDialog::saveKeyToFile);

        layout->addWidget(new QLabel("Текущий ключ шифрования (SHA-256):"));
        layout->addWidget(keyEdit);
        layout->addWidget(saveBtn);
    }

    void KeyDialog::saveKeyToFile()
    {
        QString path = QFileDialog::getSaveFileName(this, "Сохранить ключ", "", "Text files (*.txt)");
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось записать файл");
            return;
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        const auto& key = Yps::AuthorKey::getInstance().get_key();
        for (unsigned char b : key)
            oss << std::setw(2) << static_cast<int>(b);
        file.write(oss.str().c_str());
        QMessageBox::information(this, "Готово", "Ключ успешно сохранён");
    }
} // Yps