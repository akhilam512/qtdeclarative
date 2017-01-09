/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKPATHITEMNVPRRENDERER_P_H
#define QQUICKPATHITEMNVPRRENDERER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qquickpathitem_p_p.h"
#include <qsgrendernode.h>
#include <private/qquicknvprfunctions_p.h>
#include <QColor>
#include <QVector4D>
#include <QDebug>

#ifndef QT_NO_OPENGL

QT_BEGIN_NAMESPACE

class QQuickPathItemNvprRenderNode;
class QOpenGLFramebufferObject;
class QOpenGLBuffer;
class QOpenGLExtraFunctions;

class QQuickPathItemNvprRenderer : public QQuickAbstractPathRenderer
{
public:
    enum Dirty {
        DirtyPath = 0x01,
        DirtyStyle = 0x02,
        DirtyFillRule = 0x04,
        DirtyDash = 0x08,
        DirtyFillGradient = 0x10,
        DirtyList = 0x20
    };

    void beginSync(int totalCount) override;
    void setPath(int index, const QQuickPath *path) override;
    void setStrokeColor(int index, const QColor &color) override;
    void setStrokeWidth(int index, qreal w) override;
    void setFillColor(int index, const QColor &color) override;
    void setFillRule(int index, QQuickVisualPath::FillRule fillRule) override;
    void setJoinStyle(int index, QQuickVisualPath::JoinStyle joinStyle, int miterLimit) override;
    void setCapStyle(int index, QQuickVisualPath::CapStyle capStyle) override;
    void setStrokeStyle(int index, QQuickVisualPath::StrokeStyle strokeStyle,
                        qreal dashOffset, const QVector<qreal> &dashPattern) override;
    void setFillGradient(int index, QQuickPathGradient *gradient) override;
    void endSync(bool async) override;

    void updateNode() override;

    void setNode(QQuickPathItemNvprRenderNode *node);

    struct NvprPath {
        QVector<GLubyte> cmd;
        QVector<GLfloat> coord;
        QByteArray str;
    };

private:
    struct VisualPathGuiData {
        int dirty = 0;
        NvprPath path;
        qreal strokeWidth;
        QColor strokeColor;
        QColor fillColor;
        QQuickVisualPath::JoinStyle joinStyle;
        int miterLimit;
        QQuickVisualPath::CapStyle capStyle;
        QQuickVisualPath::FillRule fillRule;
        bool dashActive;
        qreal dashOffset;
        QVector<qreal> dashPattern;
        bool fillGradientActive;
        QQuickPathItemGradientCache::GradientDesc fillGradient;
    };

    void convertPath(const QQuickPath *path, VisualPathGuiData *d);

    QQuickPathItemNvprRenderNode *m_node = nullptr;
    int m_accDirty = 0;

    QVector<VisualPathGuiData> m_vp;
};

QDebug operator<<(QDebug debug, const QQuickPathItemNvprRenderer::NvprPath &path);

class QQuickNvprMaterialManager
{
public:
    enum Material {
        MatSolid,
        MatLinearGradient,

        NMaterials
    };

    struct MaterialDesc {
        GLuint ppl = 0;
        GLuint prg = 0;
        int uniLoc[4];
    };

    void create(QQuickNvprFunctions *nvpr);
    MaterialDesc *activateMaterial(Material m);
    void releaseResources();

private:
    QQuickNvprFunctions *m_nvpr;
    MaterialDesc m_materials[NMaterials];
};

class QQuickNvprBlitter
{
public:
    bool create();
    void destroy();
    bool isCreated() const { return m_program != nullptr; }
    void texturedQuad(GLuint textureId, const QSize &size,
                      const QMatrix4x4 &proj, const QMatrix4x4 &modelview,
                      float opacity);

private:
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer *m_buffer = nullptr;
    int m_matrixLoc;
    int m_opacityLoc;
    QSize m_prevSize;
};

class QQuickPathItemNvprRenderNode : public QSGRenderNode
{
public:
    QQuickPathItemNvprRenderNode(QQuickPathItem *item);
    ~QQuickPathItemNvprRenderNode();

    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;

    static bool isSupported();

private:
    struct VisualPathRenderData {
        GLuint path = 0;
        int dirty = 0;
        QQuickPathItemNvprRenderer::NvprPath source;
        GLfloat strokeWidth;
        QVector4D strokeColor;
        QVector4D fillColor;
        GLenum joinStyle;
        GLint miterLimit;
        GLenum capStyle;
        GLenum fillRule;
        GLfloat dashOffset;
        QVector<GLfloat> dashPattern;
        bool fillGradientActive;
        QQuickPathItemGradientCache::GradientDesc fillGradient;
        QOpenGLFramebufferObject *fallbackFbo = nullptr;
    };

    void updatePath(VisualPathRenderData *d);
    void renderStroke(VisualPathRenderData *d, int strokeStencilValue, int writeMask);
    void renderFill(VisualPathRenderData *d);
    void renderOffscreenFill(VisualPathRenderData *d);
    void setupStencilForCover(bool stencilClip, int sv);

    static bool nvprInited;
    static QQuickNvprFunctions nvpr;
    static QQuickNvprMaterialManager mtlmgr;

    QQuickPathItem *m_item;
    QQuickNvprBlitter m_fallbackBlitter;
    QOpenGLExtraFunctions *f = nullptr;

    QVector<VisualPathRenderData> m_vp;

    friend class QQuickPathItemNvprRenderer;
};

QT_END_NAMESPACE

#endif // QT_NO_OPENGL

#endif // QQUICKPATHITEMNVPRRENDERER_P_H
