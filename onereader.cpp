// onereader.cpp
#include "onereader.h"
#include <algorithm>
#include <vector>
#include <QDebug>
#include <climits>  // For INT_MAX, INT_MIN

OneReader::OneReader(QObject *parent) : QObject(parent) {}

OneReader::~OneReader() {
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
        delete m_watcher;
    }
}

void OneReader::load(const QString& filename) {
    if (m_watcher && !m_watcher->isFinished()) {
        qDebug() << "Loading already in progress.";
        return;
    }

    delete m_watcher;
    m_watcher = new QFutureWatcher<bool>(this);

    connect(m_watcher, &QFutureWatcher<bool>::finished, this, [this]() {
        bool success = m_watcher->result();
        emit loadingFinished(success);
        m_watcher->deleteLater();
        m_watcher = nullptr;
    });

    QFuture<bool> future = QtConcurrent::run(this, &OneReader::doLoad, filename);
    m_watcher->setFuture(future);

    emit loadingStarted();
}

bool OneReader::doLoad(const QString& filename) {
    // Önceki verileri tamamen temizle
    scene = Scene();  // Sıfırla
    volumes.clear();
    textures.clear();

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << filename;
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::BigEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    qint64 fileSize = file.size();
    file.seek(fileSize - 8);
    qint64 headerLength;
    in >> headerLength;

    qint64 headerPos = fileSize - headerLength - 8;
    if (headerPos < 0) {
        qDebug() << "Invalid header position (negative): " << headerPos;
        return false;
    }
    file.seek(headerPos);

    qint32 fileID;
    in >> fileID;

    if (fileID != 102380) {
        qDebug() << "Invalid ONE file ID: " << fileID;
        return false;
    }

    qint32 version;
    in >> version;

    in >> scene.id;
    scene.name = readString(in);
    scene.params = parseParams(readString(in));

    qint32 numVolumes;
    in >> numVolumes;
    volumes.resize(numVolumes);
    for (int i = 0; i < numVolumes; ++i) {
        Volume& vol = volumes[i];
        in >> vol.id;
        vol.name = readString(in);
        vol.params = parseParams(readString(in));
    }

    qint32 numTextures;
    in >> numTextures;
    textures.resize(numTextures);
    for (int i = 0; i < numTextures; ++i) {
        Texture& tex = textures[i];
        in >> tex.id;
        tex.name = readString(in);
        tex.params = parseParams(readString(in));
    }

    // Read data from beginning
    file.seek(0);
    for (int i = 0; i < numTextures; ++i) {
        Texture& tex = textures[i];

        qint64 readId;
        in >> readId;
        if (readId != tex.id) {
            qDebug() << "Texture ID mismatch: read " << readId << " expected " << tex.id;
            return false;
        }

        qint32 numVoxels;
        in >> numVoxels;

        QString type = tex.params.value("TYPE", "");
        tex.isFloat = (type == "RGBA_FLOAT");

        int minX = INT_MAX, minY = INT_MAX, minZ = INT_MAX;
        int maxX = INT_MIN, maxY = INT_MIN, maxZ = INT_MIN;

        struct VoxelData {
            int x, y, z;
            float r, g, b, a;
        };
        std::vector<VoxelData> voxList;
        voxList.reserve(numVoxels);

        for (int v = 0; v < numVoxels; ++v) {
            qint32 x, y, z;
            in >> x >> y >> z;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            minZ = std::min(minZ, z);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            maxZ = std::max(maxZ, z);

            float r, g, b, a;
            if (tex.isFloat) {
                in >> r >> g >> b >> a;
            } else {
                quint8 rb, gb, bb, ab;
                in >> rb >> gb >> bb >> ab;
                r = rb / 255.0f;
                g = gb / 255.0f;
                b = bb / 255.0f;
                a = ab / 255.0f;
            }
            voxList.push_back({x, y, z, r, g, b, a});
        }

        tex.sizeX = maxX - minX + 1;
        tex.sizeY = maxY - minY + 1;
        tex.sizeZ = maxZ - minZ + 1;

        if (tex.isFloat) {
            tex.data.resize(static_cast<size_t>(tex.sizeX) * tex.sizeY * tex.sizeZ * 4, 0.0f);
        } else {
            tex.byteData.resize(static_cast<size_t>(tex.sizeX) * tex.sizeY * tex.sizeZ * 4, 0);
        }

        for (const auto& v : voxList) {
            int shifted_x = v.x - minX;
            int shifted_y = v.y - minY;
            int shifted_z = v.z - minZ;

            size_t index = (static_cast<size_t>(shifted_z) * tex.sizeY + shifted_y) * tex.sizeX + shifted_x;
            index *= 4;

            if (tex.isFloat) {
                tex.data[index] = v.g;
                tex.data[index + 1] = v.b;
                tex.data[index + 2] = v.r;
                tex.data[index + 3] = v.a;
            } else {
                tex.byteData[index] = static_cast<unsigned char>(v.g * 255.0f);
                tex.byteData[index + 1] = static_cast<unsigned char>(v.b * 255.0f);
                tex.byteData[index + 2] = static_cast<unsigned char>(v.r * 255.0f);
                tex.byteData[index + 3] = static_cast<unsigned char>(v.a * 255.0f);
            }
        }

    }

    return true;
}

OneReader::Texture* OneReader::getTextureForVolume(const Volume& vol) {
    QString texIdStr = vol.params.value("TEXTURE_ID_0", "");
    bool ok;
    qint64 texId = texIdStr.toLongLong(&ok);
    if (!ok) return nullptr;
    for (auto& tex : textures) {
        if (tex.id == texId) return &tex;
    }
    return nullptr;
}

QMap<QString, QString> OneReader::parseParams(const QString& paramStr) {
    QMap<QString, QString> map;
    QStringList parts = paramStr.split("!@", Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        QStringList kv = p.split(':', Qt::KeepEmptyParts);
        if (kv.size() == 2) {
            map[kv[0].trimmed()] = kv[1].trimmed();
        }
    }
    return map;
}

QString OneReader::readString(QDataStream& ds) {
    quint16 len;
    ds >> len;
    QByteArray ba(len, '\0');
    ds.readRawData(ba.data(), len);
    return QString::fromUtf8(ba);
}
