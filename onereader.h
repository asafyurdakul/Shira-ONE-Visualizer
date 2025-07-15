// one_reader.h
#ifndef ONEREADER_H
#define ONEREADER_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QFile>
#include <QDataStream>

class OneReader {
public:
    struct Texture {
        qint64 id;
        QString name;
        QMap<QString, QString> params;
        int sizeX = 0;
        int sizeY = 0;
        int sizeZ = 0;
        bool isFloat = false;
        std::vector<float> data; // RGBA float, 4 channels
        std::vector<unsigned char> byteData; // Yeni: Byte tipi için (RGBA8)
    };

    struct Volume {
        qint64 id;
        QString name;
        QMap<QString, QString> params;
    };

    struct Scene {
        qint64 id;
        QString name;
        QMap<QString, QString> params;
    };

    Scene scene;
    QVector<Volume> volumes;
    QVector<Texture> textures;

    bool load(const QString& filename);

    Texture* getTextureForVolume(const Volume& vol);

private:
    QMap<QString, QString> parseParams(const QString& paramStr);
    QString readString(QDataStream& ds);
};

#endif // ONEREADER_H
