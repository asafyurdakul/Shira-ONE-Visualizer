// onereader.h
#ifndef ONEREADER_H
#define ONEREADER_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QFile>
#include <QDataStream>
#include <QObject>
#include <QtConcurrent>
#include <QFutureWatcher>

class OneReader : public QObject {
    Q_OBJECT

public:
    explicit OneReader(QObject *parent = nullptr);
    ~OneReader();

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

    void load(const QString& filename); // Asynchronous load

    Texture* getTextureForVolume(const Volume& vol);

signals:
    void loadingStarted();
    void loadingFinished(bool success);

private:
    bool doLoad(const QString& filename); // Synchronous loading logic

    QMap<QString, QString> parseParams(const QString& paramStr);
    QString readString(QDataStream& ds);

    QFutureWatcher<bool> *m_watcher = nullptr;
};

#endif // ONEREADER_H
