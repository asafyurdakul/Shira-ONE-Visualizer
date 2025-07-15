#define _USE_MATH_DEFINES // For M_PI in MSVC
#include "onerenderer.h"
#include <QDebug>
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cstring>

OneRenderer::OneRenderer(QWidget *parent) : QOpenGLWidget(parent) {
    memset(m_textures, 0, sizeof(m_textures));
}

OneRenderer::~OneRenderer() {
    makeCurrent();
    for (int i = 0; i < 10; ++i) {
        if (m_textures[i]) glDeleteTextures(1, &m_textures[i]);
    }
    m_vbo.destroy();
    m_boundVBO.destroy();
    glDeleteVertexArrays(1, &m_vao);
    glDeleteVertexArrays(1, &m_boundVAO);
}

void OneRenderer::setOneReader(OneReader *reader) {
    m_reader = reader;
    makeCurrent();
    createTextures();
    update();
}

void OneRenderer::toggleBounds(bool enable) {
    m_drawBounds = enable;
    update();
}

void OneRenderer::setNestedMode(bool enable) {
    m_nestedMode = enable;
    if (m_reader) {
        createTextures();
    }
    update();
}

void OneRenderer::setBackgroundColor(const QVector3D &color)
{
    m_backgroundColor = color;
    update();
}

QString loadShaderSource(const QString& resourcePath) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open shader file:" << resourcePath;
        return "";
    }
    QTextStream in(&file);
    return in.readAll();
}

void OneRenderer::initializeGL() {
    initializeOpenGLFunctions();

    // Load common vertex shader
    QString vertSource = loadShaderSource(":/shaders/single.vert");

    // Load single fragment shader
    QString singleFragSource = loadShaderSource(":/shaders/single.frag");
    if (!m_singleProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertSource)) {
        qDebug() << "Single vertex compile error:" << m_singleProgram.log();
    }
    if (!m_singleProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, singleFragSource)) {
        qDebug() << "Single fragment shader compile error:" << m_singleProgram.log();
    }
    if (!m_singleProgram.link()) {
        qDebug() << "Single shader link error:" << m_singleProgram.log();
    }

    // Load nested fragment shader
    QString nestedFragSource = loadShaderSource(":/shaders/nested.frag");
    if (!m_nestedProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertSource)) {
        qDebug() << "Nested vertex compile error:" << m_nestedProgram.log();
    }
    if (!m_nestedProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, nestedFragSource)) {
        qDebug() << "Nested fragment shader compile error:" << m_nestedProgram.log();
    }
    if (!m_nestedProgram.link()) {
        qDebug() << "Nested shader link error:" << m_nestedProgram.log();
    }

    // Line shader (hardcoded as it's simple)
    QString lineVertSource = R"(
#version 330
layout (location=0) in vec3 position;
uniform mat4 viewProjectionMatrix;
uniform mat4 modelMatrix;
void main() {
    gl_Position = viewProjectionMatrix * modelMatrix * vec4(position,1);
}
)";
    QString lineFragSource = R"(
#version 330
out vec4 fragColor;
void main() {
    fragColor = vec4(1,1,1,1);
}
)";
    if (!m_lineProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, lineVertSource)) {
        qDebug() << "Line vertex shader compile error:" << m_lineProgram.log();
    }
    if (!m_lineProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, lineFragSource)) {
        qDebug() << "Line fragment shader compile error:" << m_lineProgram.log();
    }
    if (!m_lineProgram.link()) {
        qDebug() << "Line shader link error:" << m_lineProgram.log();
    }

    setupCubeGeometry();
    setupBoundLines();
}

void OneRenderer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    m_projMatrix.setToIdentity();
    m_projMatrix.perspective(45.0f, static_cast<float>(w) / h, 0.1f, 100.0f);
}

void OneRenderer::paintGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!m_reader || m_reader->volumes.isEmpty()) return;

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    QOpenGLShaderProgram& prog = m_nestedMode ? m_nestedProgram : m_singleProgram;
    prog.bind();

    QVector3D cameraPos = m_rotation.rotatedVector(QVector3D(0, 0, -m_distance));
    QVector3D up = m_rotation.rotatedVector(QVector3D(0, 1, 0));
    m_viewMatrix.setToIdentity();
    m_viewMatrix.lookAt(cameraPos, QVector3D(0, 0, 0), up);

    QMatrix4x4 vp = m_projMatrix * m_viewMatrix;
    prog.setUniformValue("viewProjectionMatrix", vp);
    QMatrix4x4 invView = m_viewMatrix.inverted();
    prog.setUniformValue("inverseViewMatrix", invView);

    float globalJScale = m_reader->scene.params.value("EMISSION", "1.0").toFloat();
    float globalKScale = m_reader->scene.params.value("OPACITY", "600.0").toFloat();
    prog.setUniformValue("jScale", globalJScale);
    prog.setUniformValue("kScale", globalKScale);

    QMatrix4x4 trans[10];
    QMatrix4x4 itrans[10];
    float tj[10];
    float tk[10];
    float tb[10];
    int tr[10];

    for (int i = 0; i < 10; ++i) {
        trans[i].setToIdentity();
        itrans[i].setToIdentity();
        tj[i] = 1.0f;
        tk[i] = 1.0f;
        tb[i] = 0.0f;
        tr[i] = 0;
    }

    QMatrix4x4 outerModel;
    outerModel.setToIdentity();

    if (m_nestedMode) {
        prog.setUniformValue("numValidTextures", m_numTextures);

        for (int i = 0; i < 10; ++i) {
            int unit = i;
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_3D, (i < m_numTextures) ? m_textures[i] : 0);
            prog.setUniformValue(QString("textures[%1]").arg(i).toUtf8().constData(), unit);
        }

        for (int j = 0; j < m_numTextures; ++j) {

            int idx = m_sortedVolumeIndices[j];
            const auto& vol = m_reader->volumes[idx];
            const auto& params = vol.params;

            float sx = params.value("SCALE_X", "1.0").toFloat();
            float sy = params.value("SCALE_Y", "1.0").toFloat();
            float sz = params.value("SCALE_Z", "1.0").toFloat();
            float ox = params.value("OFFSET_X", "0.0").toFloat();
            float oy = params.value("OFFSET_Y", "0.0").toFloat();
            float oz = params.value("OFFSET_Z", "0.0").toFloat();
            float rx = params.value("ROT_X", "0.0").toFloat();
            float ry = params.value("ROT_Y", "0.0").toFloat();
            float rz = params.value("ROT_Z", "0.0").toFloat();

            QMatrix4x4 model;
            model.setToIdentity();
            model.scale(1.0 / sx, 1.0 / sy, 1.0 / sz);
            model.rotate(rx, 1.0f, 0.0f, 0.0f);
            model.rotate(ry, 0.0f, 1.0f, 0.0f);
            model.rotate(rz, 0.0f, 0.0f, 1.0f);
            model.translate(-0.5 * ox, -0.5 * oy, -0.5 * oz);

            qDebug()<<m_reader->scene.name<<m_reader->scene.params;
            qDebug()<<"vol"<<vol.id<<vol.params;
            //auto* tex = m_reader->getTextureForVolume(m_reader->volumes[idx]);
            //qDebug()<<"tex"<<tex->id<<tex->params;
            //qDebug()<<"tex"<<tex->id<<tex->sizeX<<tex->sizeY<<tex->sizeZ;
            //qDebug() << "Volume" << idx << "Transform:" << model;
            //qDebug() << "Inverse Transform:" << model.inverted();


            trans[j] = model;
            itrans[j] = model.inverted();

            tj[j] =  QString::number(globalJScale).toFloat();
            tk[j] = QString::number(globalKScale).toFloat();
            tb[j] = params.value("BLEND", "0.0").toFloat();
            tr[j] = (params.value("REPLACE", "false").toLower() == "true") ? 1 : 0;


            qDebug()<<"jScale"<<tj[j];
            qDebug()<<"kScale"<<tk[j];
        }

        int loc_trans = prog.uniformLocation("texture_transform");
        prog.setUniformValueArray(loc_trans, trans, 10);

        int loc_itrans = prog.uniformLocation("texture_iTransform");
        prog.setUniformValueArray(loc_itrans, itrans, 10);

        int loc_tj = prog.uniformLocation("texture_jscale");
        prog.setUniformValueArray(loc_tj, tj, 10, 1);

        int loc_tk = prog.uniformLocation("texture_kscale");
        prog.setUniformValueArray(loc_tk, tk, 10, 1);

        int loc_tb = prog.uniformLocation("texture_blend");
        prog.setUniformValueArray(loc_tb, tb, 10, 1);

        int loc_tr = prog.uniformLocation("texture_replace");
        prog.setUniformValueArray(loc_tr, tr, 10);

        // Set normalization uniforms based on first texture
        int normalize = 0;
        float norm_grey = 1.0f;
        float norm_alpha = 1.0f;
        float norm_exp = 1.0f;
        /*if (m_numTextures > 0) {
            for (int j = 4; j < m_numTextures; ++j) { // Her texture için, ama uniform global, max al veya first
                int idx = m_sortedVolumeIndices[j];
                auto* tex = m_reader->getTextureForVolume(m_reader->volumes[idx]);
                if (tex && tex->isFloat) {
                    normalize = 1;
                    norm_grey = std::max(norm_grey, tex->params.value("MAX_GREY", "1.0").toFloat());
                    norm_alpha = std::max(norm_alpha, tex->params.value("MAX_A", "1.0").toFloat());
                    QString jscale = tex->params.value("JSCALE", "LINEAR");
                    if (jscale == "POWER") {
                        norm_exp = tex->params.value("JSCALE_POWER", "1.0").toFloat();
                    } else if (jscale == "LOG") {
                        norm_exp = 1.0f; // Log için shader'da log(1 + x) ekle eğer lazım
                    }
                }
            }
        }*/

        prog.setUniformValue("texture_normalize", normalize);
        prog.setUniformValue("texture_norm_grey", norm_grey);
        prog.setUniformValue("texture_norm_alpha", norm_alpha);
        prog.setUniformValue("texture_norm_exp", norm_exp);

        prog.setUniformValue("numEmitters", 0);
        prog.setUniformValue("star_brightness", 0.0f);
        prog.setUniformValue("backgroundColor", m_backgroundColor);

    }
    else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, m_textures[0]);
        prog.setUniformValue("tex", 0);

        if (!m_reader->volumes.isEmpty()) {
            const auto& vol = m_reader->volumes[0];
            const auto& params = vol.params;

            float jScaleVol = params.value("EMISSION", QString::number(globalJScale)).toFloat();
            float kScaleVol = params.value("OPACITY", QString::number(globalKScale)).toFloat();
            prog.setUniformValue("jScale", jScaleVol);
            prog.setUniformValue("kScale", kScaleVol);

            float sx = 1.0 / params.value("SCALE_X", "1.0").toFloat();
            float sy = 1.0 / params.value("SCALE_Y", "1.0").toFloat();
            float sz = 1.0 / params.value("SCALE_Z", "1.0").toFloat();
            float ox = -0.5 * params.value("OFFSET_X", "0.0").toFloat();
            float oy = -0.5 * params.value("OFFSET_Y", "0.0").toFloat();
            float oz = -0.5 * params.value("OFFSET_Z", "0.0").toFloat();
            float rx = params.value("ROT_X", "0.0").toFloat();
            float ry = params.value("ROT_Y", "0.0").toFloat();
            float rz = params.value("ROT_Z", "0.0").toFloat();

            outerModel.scale(sx, sy, sz);
            outerModel.rotate(rx, 1.0f, 0.0f, 0.0f);
            outerModel.rotate(ry, 0.0f, 1.0f, 0.0f);
            outerModel.rotate(rz, 0.0f, 0.0f, 1.0f);
            outerModel.translate(ox, oy, oz);

        }
    }

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    prog.release();

    if (m_drawBounds) {
        m_lineProgram.bind();
        m_lineProgram.setUniformValue("viewProjectionMatrix", vp);
        for (int j = 0; j < m_numTextures; ++j) {
            QMatrix4x4 model;
            if (m_nestedMode) {
                model = itrans[j];
            } else {
                model = outerModel;
            }
            m_lineProgram.setUniformValue("modelMatrix", model);
            glBindVertexArray(m_boundVAO);
            glDrawArrays(GL_LINES, 0, 24);
        }
        glBindVertexArray(0);
        m_lineProgram.release();
    }

}

void OneRenderer::mousePressEvent(QMouseEvent *event) {
    m_lastMousePos = event->pos();
}

void OneRenderer::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        auto projectToSphere = [this](const QPoint& p) -> QVector3D {
            float x = (2.0f * p.x() / width()) - 1.0f;
            float y = 1.0f - (2.0f * p.y() / height());
            float d = std::sqrt(x * x + y * y);
            float z = 0.0f;
            if (d <= 1.0f) {
                z = std::sqrt(1.0f - d * d);
            } else {
                float norm = 1.0f / d;
                x *= norm;
                y *= norm;
            }
            return QVector3D(x, y, z).normalized();
        };

        QVector3D prevVec = projectToSphere(m_lastMousePos);
        QVector3D currVec = projectToSphere(event->pos());

        float dot = QVector3D::dotProduct(prevVec, currVec);
        dot = std::clamp(dot, -1.0f, 1.0f);
        float angle = std::acos(dot) * 180.0f / M_PI;
        QVector3D axis = QVector3D::crossProduct(prevVec, currVec);

        if (axis.lengthSquared() > 0.0001f) {
            QQuaternion rot = QQuaternion::fromAxisAndAngle(axis.normalized(), angle);
            m_rotation = rot * m_rotation;
            m_rotation.normalize();
        }

        m_lastMousePos = event->pos();
        update();
    }
}

void OneRenderer::wheelEvent(QWheelEvent *event) {
    m_distance *= std::pow(1.001f, -event->angleDelta().y());
    m_distance = std::clamp(m_distance, 0.1f, 10.0f);
    update();
}

void OneRenderer::createTextures() {
    for (int i = 0; i < 10; ++i) {
        if (m_textures[i]) {
            glDeleteTextures(1, &m_textures[i]);
            m_textures[i] = 0;
        }
    }

    if (!m_reader || m_reader->volumes.isEmpty()) return;

    m_sortedVolumeIndices.clear();

    if (m_nestedMode) {
        std::vector<std::pair<int, int>> order_indices;
        for (int i = 0; i < m_reader->volumes.size(); ++i) {
            int order = m_reader->volumes[i].params.value("ORDER", "0").toInt();
            order_indices.push_back({order, i});
        }
        // Sabit: Artan sıralama (düşük order dıştan içe)
        std::sort(order_indices.begin(), order_indices.end());

        int num = qMin(10, static_cast<int>(order_indices.size()));
        m_numTextures = 0;
        for (int j = 0; j < num; ++j) {
            int idx = order_indices[j].second;
            auto* tex = m_reader->getTextureForVolume(m_reader->volumes[idx]);
            if (!tex) continue;

            glGenTextures(1, &m_textures[m_numTextures]);
            glBindTexture(GL_TEXTURE_3D, m_textures[m_numTextures]);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            // PBO ile yükleme
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

            if (tex->isFloat) {
                size_t dataSize = tex->data.size() * sizeof(float);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (mapped) {
                    memcpy(mapped, tex->data.data(), dataSize);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
                glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, tex->sizeX, tex->sizeY, tex->sizeZ, 0, GL_RGBA, GL_FLOAT, nullptr);
            } else {
                size_t dataSize = tex->byteData.size() * sizeof(unsigned char);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (mapped) {
                    memcpy(mapped, tex->byteData.data(), dataSize);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
                glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, tex->sizeX, tex->sizeY, tex->sizeZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);

            m_sortedVolumeIndices << idx;
            m_numTextures++;
        }
    } else {
        m_numTextures = 1;
        auto* tex = m_reader->getTextureForVolume(m_reader->volumes[0]);
        if (tex) {
            glGenTextures(1, &m_textures[0]);
            glBindTexture(GL_TEXTURE_3D, m_textures[0]);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            // PBO ile yükleme
            GLuint pbo;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

            if (tex->isFloat) {
                size_t dataSize = tex->data.size() * sizeof(float);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (mapped) {
                    memcpy(mapped, tex->data.data(), dataSize);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
                glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, tex->sizeX, tex->sizeY, tex->sizeZ, 0, GL_RGBA, GL_FLOAT, nullptr);
            } else {
                size_t dataSize = tex->byteData.size() * sizeof(unsigned char);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (mapped) {
                    memcpy(mapped, tex->byteData.data(), dataSize);
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
                glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, tex->sizeX, tex->sizeY, tex->sizeZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);

            m_sortedVolumeIndices << 0;
        }
    }
}

void OneRenderer::setupCubeGeometry() {
    float bs = 1.0f;
    float vertices[] = {
        // front
        -bs, -bs, bs,  bs, -bs, bs,  bs, bs, bs,
        -bs, -bs, bs,  bs, bs, bs,  -bs, bs, bs,
        // back
        -bs, -bs, -bs,  -bs, bs, -bs,  bs, bs, -bs,
        -bs, -bs, -bs,  bs, bs, -bs,  bs, -bs, -bs,
        // left
        -bs, -bs, -bs,  -bs, -bs, bs,  -bs, bs, bs,
        -bs, -bs, -bs,  -bs, bs, bs,  -bs, bs, -bs,
        // right
        bs, -bs, -bs,  bs, bs, -bs,  bs, bs, bs,
        bs, -bs, -bs,  bs, bs, bs,  bs, -bs, bs,
        // bottom
        -bs, -bs, -bs,  bs, -bs, -bs,  bs, -bs, bs,
        -bs, -bs, -bs,  bs, -bs, bs,  -bs, -bs, bs,
        // top
        -bs, bs, -bs,  -bs, bs, bs,  bs, bs, bs,
        -bs, bs, -bs,  bs, bs, bs,  bs, bs, -bs
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
    m_vbo.release();
}

void OneRenderer::setupBoundLines() {
    float bs = 1.0f;
    float boundVertices[] = {
        // bottom square
        -bs, -bs, -bs,  bs, -bs, -bs,
        bs, -bs, -bs,  bs, -bs, bs,
        bs, -bs, bs,  -bs, -bs, bs,
        -bs, -bs, bs,  -bs, -bs, -bs,
        // top square
        -bs, bs, -bs,  bs, bs, -bs,
        bs, bs, -bs,  bs, bs, bs,
        bs, bs, bs,  -bs, bs, bs,
        -bs, bs, bs,  -bs, bs, -bs,
        // vertical edges
        -bs, -bs, -bs,  -bs, bs, -bs,
        bs, -bs, -bs,  bs, bs, -bs,
        bs, -bs, bs,  bs, bs, bs,
        -bs, -bs, bs,  -bs, bs, bs
    };

    glGenVertexArrays(1, &m_boundVAO);
    glBindVertexArray(m_boundVAO);

    m_boundVBO.create();
    m_boundVBO.bind();
    m_boundVBO.allocate(boundVertices, sizeof(boundVertices));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
}

