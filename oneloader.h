// oneloader.h (Yeni dosya: Header for OneLoader class)
#ifndef ONELOADER_H
#define ONELOADER_H

#include <QObject>
#include <QString>
#include "onereader.h"  // Include OneReader header

class OneLoader : public QObject {
    Q_OBJECT

public:
    explicit OneLoader(QObject *parent = nullptr);
    ~OneLoader();

    void load(const QString& filename);
    OneReader* getReader() const;

signals:
    void loadingStarted();
    void loadingFinished(bool success);

private:
    OneReader *m_reader;
    QString m_currentFilename;
};

#endif // ONELOADER_H
