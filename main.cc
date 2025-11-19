#include <QApplication>
#include "Gui/MainWindow.hh"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("YpsHnS");
    app.setOrganizationName("Ypsilon");

    Yps::MainWindow w;
    w.show();
    return app.exec();
}