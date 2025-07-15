#ifndef ONERENDERER_H
#define ONERENDERER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include "onereader.h"

class OneRenderer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    OneRenderer(QWidget *parent = nullptr);
    ~OneRenderer();

    void setOneReader(OneReader *reader);
    void toggleBounds(bool enable);
    void setNestedMode(bool enable);
    void setBackgroundColor(const QVector3D &color);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    OneReader *m_reader = nullptr;
    QOpenGLShaderProgram m_singleProgram;
    QOpenGLShaderProgram m_nestedProgram;
    QOpenGLShaderProgram m_lineProgram;
    GLuint m_vao = 0;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_boundVBO;
    GLuint m_boundVAO = 0;
    GLuint m_textures[10] = {0};
    int m_numTextures = 0;
    QMatrix4x4 m_projMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_distance = 1.5f;
    QQuaternion m_rotation;
    QPoint m_lastMousePos;
    bool m_drawBounds = false;
    bool m_nestedMode = false;
    QVector<int> m_sortedVolumeIndices;
    QVector3D m_backgroundColor = QVector3D(0.5,0.5,0.5);

    void createTextures();
    void setupCubeGeometry();
    void setupBoundLines();
};

#endif // ONERENDERER_H
