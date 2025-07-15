// one_reader.cpp
#include "onereader.h"
#include <algorithm>
#include <vector>
#include <QDebug>

bool OneReader::load(const QString& filename) {
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

        //qDebug() << "Texture ID:" << tex.id << "Name:" << tex.name << "Params:" << tex.params << "Num Voxels:" << numVoxels << "Is Float:" << tex.isFloat;

        int maxX = 0, maxY = 0, maxZ = 0;

        struct VoxelData {
            int x, y, z;
            float r, g, b, a;
        };
        std::vector<VoxelData> voxList;
        voxList.reserve(numVoxels);

        for (int v = 0; v < numVoxels; ++v) {
            qint32 x, y, z;
            in >> x >> y >> z;
            maxX = std::max(maxX, x + 1);
            maxY = std::max(maxY, y + 1);
            maxZ = std::max(maxZ, z + 1);

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

            /*if(i == 0)
                if (v < 5) { // Log first 5 voxels
                    qDebug() << "Voxel " << v << ": x=" << x << " y=" << y << " z=" << z << " r=" << r << " g=" << g << " b=" << b << " a=" << a;
                }*/
        }
        //qDebug() << "Texture Dimensions: sizeX=" << maxX << " sizeY=" << maxY << " sizeZ=" << maxZ;

        tex.sizeX = maxX;
        tex.sizeY = maxY;
        tex.sizeZ = maxZ;

        tex.data.resize(static_cast<size_t>(tex.sizeX) * tex.sizeY * tex.sizeZ * 4, 0.0f);

        for (const auto& v : voxList) {
            size_t index = (static_cast<size_t>(v.z) * tex.sizeY + v.y) * tex.sizeX + v.x;
            index *= 4;

            tex.data[index] = v.g;
            tex.data[index + 1] = v.b;
            tex.data[index + 2] = v.r;
            tex.data[index + 3] = v.a;
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
