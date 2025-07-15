// main.cpp
#include "onerenderer.h"

#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <qfiledialog.h>
#include <QCheckBox>
#include <QColorDialog>  // EKLE: QColorDialog için

class MyApplication : public QApplication {
public:
    MyApplication(int &argc, char **argv) : QApplication(argc, argv) {}

    bool notify(QObject *receiver, QEvent *event) override {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::bad_alloc &e) {
            qDebug() << "Caught bad_alloc in notify:" << e.what();
            QMessageBox::critical(nullptr, "Error", "Out of memory! The file may be too large to load.");
            return false;
        } catch (const std::exception &e) {
            qDebug() << "Caught exception in notify:" << e.what();
            QMessageBox::critical(nullptr, "Error", QString("An exception occurred: ") + e.what());
            return false;
        } catch (...) {
            qDebug() << "Caught unknown exception in notify";
            QMessageBox::critical(nullptr, "Error", "An unknown exception occurred.");
            return false;
        }
    }
};

int main(int argc, char *argv[])
{
    MyApplication a(argc, argv);

    QMainWindow window;
    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    OneRenderer *renderer = new OneRenderer(centralWidget);
    layout->addWidget(renderer);

    QPushButton *loadButton = new QPushButton("Load .ONE File", centralWidget);
    layout->addWidget(loadButton);

    QPushButton *toggleBoundsButton = new QPushButton("Toggle Bounds", centralWidget);
    layout->addWidget(toggleBoundsButton);

    QCheckBox *nestedCheckBox = new QCheckBox("Nested Mode", centralWidget);
    nestedCheckBox->setChecked(true);
    layout->addWidget(nestedCheckBox);

    // EKLE: Background color seçme butonu
    QPushButton *colorButton = new QPushButton("Select Background Color", centralWidget);
    layout->addWidget(colorButton);

    OneReader *reader = new OneReader();

    QObject::connect(loadButton, &QPushButton::clicked, [&]() {
        QString filename = QFileDialog::getOpenFileName(&window, "Open .ONE File", "", "ONE Files (*.one)");
        if (!filename.isEmpty()) {
            if (reader->load(filename)) {
                renderer->setNestedMode(true);
                renderer->setOneReader(reader);
            } else {
                // Handle error, e.g., show message
            }
        }
    });

    QObject::connect(toggleBoundsButton, &QPushButton::clicked, [&]() {
        static bool boundsEnabled = false;
        boundsEnabled = !boundsEnabled;
        renderer->toggleBounds(boundsEnabled);
    });

    QObject::connect(nestedCheckBox, &QCheckBox::toggled, renderer, &OneRenderer::setNestedMode);

    // EKLE: Color button connect
    QObject::connect(colorButton, &QPushButton::clicked, [&]() {
        QColor color = QColorDialog::getColor(Qt::gray, &window, "Select Background Color");
        if (color.isValid()) {
            renderer->setBackgroundColor(QVector3D(color.redF(), color.greenF(), color.blueF()));
            renderer->update();
        }
    });

    window.setCentralWidget(centralWidget);
    window.resize(800, 600);
    window.show();

    return a.exec();
}
