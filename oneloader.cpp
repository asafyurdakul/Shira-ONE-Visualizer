// oneloader.cpp (Yeni dosya: Implementation for OneLoader class)
#include "oneloader.h"
#include <QDebug>

OneLoader::OneLoader(QObject *parent) : QObject(parent), m_reader(new OneReader(this)), m_currentFilename("") {
    // Forward signals from reader
    connect(m_reader, &OneReader::loadingStarted, this, &OneLoader::loadingStarted);
    connect(m_reader, &OneReader::loadingFinished, this, &OneLoader::loadingFinished);
}

OneLoader::~OneLoader() {
    // No need to delete m_reader as it has this as parent
}

void OneLoader::load(const QString& filename) {
    if (filename == m_currentFilename) {
        qDebug() << "Same file already loaded, skipping.";
        return;  // Pass if same file
    }
    m_currentFilename = filename;
    m_reader->load(filename);
}

OneReader* OneLoader::getReader() const {
    return m_reader;
}
