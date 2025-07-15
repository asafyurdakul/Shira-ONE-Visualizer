#include "mainwindow.h"
//#include "onefilereader.h"
#include "ui_mainwindow.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
/*
void MainWindow::loadOneFile(const QString &path)
{
    OneReader reader;
    if (reader.loadFromFile(path)) {
        const OneScene& scene = reader.scene();
        qDebug() << "Scene Name:" << scene.name;
        qDebug() << "Scene ID:" << scene.id;
        qDebug() << "Scene Parameters:";
        for (const auto& key : scene.params.keys()) {
            qDebug() << " -" << key << ":" << scene.params.value(key);
        }

        for (const auto& volume : scene.volumes) {
            qDebug() << "Volume:" << volume.name << "(ID:" << volume.id << ")";
            for (const auto& key : volume.params.keys()) {
                qDebug() << "  " << key << ":" << volume.params.value(key);
            }
        }

        const auto& textures = reader.textures();
        for (const auto& tex : textures) {
            qDebug() << "Texture:" << tex.name << "(ID:" << tex.id << ")";
            for (const auto& key : tex.params.keys()) {
                qDebug() << "  " << key << ":" << tex.params.value(key);
            }
        }
    } else {
        qWarning() << "ONE dosyası okunamadı!";
    }
}
*/
void MainWindow::on_btnLoadOne_clicked()
{
 /*  ONEFileReader reader("ONE Test.ONE");
   if (reader.readFile()) {
       const ONEFileReader::Scene& scene = reader.getScene();
       const QVector<ONEFileReader::Texture>& textures = reader.getTextures();
       // Process scene and textures as needed

       //logging
       // Log scene information
       qDebug() << "Scene Information:";
       qDebug() << "  ID:" << scene.id;
       qDebug() << "  Name:" << scene.name;
       qDebug() << "  Parameters:";
       for (auto it = scene.parameters.constBegin(); it != scene.parameters.constEnd(); ++it) {
           qDebug() << "    " << it.key() << ":" << it.value();
       }

       // Log volumes information
       qDebug() << "Volumes (" << scene.volumes.size() << "):";
       for (const ONEFileReader::Volume& volume : scene.volumes) {
           qDebug() << "  Volume ID:" << volume.id;
           qDebug() << "    Name:" << volume.name;
           qDebug() << "    Parameters:";
           for (auto it = volume.parameters.constBegin(); it != volume.parameters.constEnd(); ++it) {
               qDebug() << "      " << it.key() << ":" << it.value();
           }
       }

       // Log textures information
       qDebug() << "Textures (" << textures.size() << "):";
       for (const ONEFileReader::Texture& texture : textures) {
           qDebug() << "  Texture ID:" << texture.id;
           qDebug() << "    Name:" << texture.name;
           qDebug() << "    Number of Voxels:" << texture.voxels.size();
           qDebug() << "    Type:" << texture.parameters.value("TYPE");
           qDebug() << "    Parameters:";
           for (auto it = texture.parameters.constBegin(); it != texture.parameters.constEnd(); ++it) {
               qDebug() << "      " << it.key() << ":" << it.value();
           }
       }


   } else {
       qDebug() << "Error:" << reader.getError();
   }
*/
}

