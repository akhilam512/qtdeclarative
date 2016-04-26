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

#include <private/qquickgenericshadereffect_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickitem_p.h>
#include <QSignalMapper>

QT_BEGIN_NAMESPACE

// The generic shader effect is used when the scenegraph backend indicates
// SupportsShaderEffectNode. This, unlike the monolithic and interconnected (e.g.
// with particles) OpenGL variant, passes most of the work to a scenegraph node
// created via the adaptation layer, thus allowing different implementation in
// the backends.

QQuickGenericShaderEffect::QQuickGenericShaderEffect(QQuickShaderEffect *item, QObject *parent)
    : QObject(parent)
    , m_item(item)
    , m_meshResolution(1, 1)
    , m_mesh(nullptr)
    , m_cullMode(QQuickShaderEffect::NoCulling)
    , m_blending(true)
    , m_supportsAtlasTextures(false)
    , m_mgr(nullptr)
    , m_dirty(0)
{
}

QQuickGenericShaderEffect::~QQuickGenericShaderEffect()
{
    for (int i = 0; i < NShader; ++i) {
        disconnectSignals(Shader(i));
        for (const auto &sm : qAsConst(m_signalMappers[i]))
            delete sm.mapper;
    }

    delete m_mgr;
}

void QQuickGenericShaderEffect::setFragmentShader(const QByteArray &src)
{
    if (m_fragShader.constData() == src.constData())
        return;

    m_fragShader = src;
    m_dirty |= QSGShaderEffectNode::DirtyShaders;

    if (m_item->isComponentComplete())
        updateShader(Fragment, src);

    m_item->update();
    emit m_item->fragmentShaderChanged();
}

void QQuickGenericShaderEffect::setVertexShader(const QByteArray &src)
{
    if (m_vertShader.constData() == src.constData())
        return;

    m_vertShader = src;
    m_dirty |= QSGShaderEffectNode::DirtyShaders;

    if (m_item->isComponentComplete())
        updateShader(Vertex, src);

    m_item->update();
    emit m_item->vertexShaderChanged();
}

void QQuickGenericShaderEffect::setBlending(bool enable)
{
    if (m_blending == enable)
        return;

    m_blending = enable;
    m_item->update();
    emit m_item->blendingChanged();
}

QVariant QQuickGenericShaderEffect::mesh() const
{
    return m_mesh ? qVariantFromValue(static_cast<QObject *>(m_mesh))
                  : qVariantFromValue(m_meshResolution);
}

void QQuickGenericShaderEffect::setMesh(const QVariant &mesh)
{
    QQuickShaderEffectMesh *newMesh = qobject_cast<QQuickShaderEffectMesh *>(qvariant_cast<QObject *>(mesh));
    if (newMesh && newMesh == m_mesh)
        return;

    if (m_mesh)
        disconnect(m_mesh, SIGNAL(geometryChanged()), this, 0);

    m_mesh = newMesh;

    if (m_mesh) {
        connect(m_mesh, SIGNAL(geometryChanged()), this, SLOT(markGeometryDirtyAndUpdate()));
    } else {
        if (mesh.canConvert<QSize>()) {
            m_meshResolution = mesh.toSize();
        } else {
            QList<QByteArray> res = mesh.toByteArray().split('x');
            bool ok = res.size() == 2;
            if (ok) {
                int w = res.at(0).toInt(&ok);
                if (ok) {
                    int h = res.at(1).toInt(&ok);
                    if (ok)
                        m_meshResolution = QSize(w, h);
                }
            }
            if (!ok)
                qWarning("ShaderEffect: mesh property must be a size or an object deriving from QQuickShaderEffectMesh");
        }
        m_defaultMesh.setResolution(m_meshResolution);
    }

    m_dirty |= QSGShaderEffectNode::DirtyShaderMesh;
    m_item->update();

    emit m_item->meshChanged();
}

void QQuickGenericShaderEffect::setCullMode(QQuickShaderEffect::CullMode face)
{
    if (m_cullMode == face)
        return;

    m_cullMode = face;
    m_item->update();
    emit m_item->cullModeChanged();
}

void QQuickGenericShaderEffect::setSupportsAtlasTextures(bool supports)
{
    if (m_supportsAtlasTextures == supports)
        return;

    m_supportsAtlasTextures = supports;
    markGeometryDirtyAndUpdate();
    emit m_item->supportsAtlasTexturesChanged();
}

QString QQuickGenericShaderEffect::log() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QString();

    return mgr->log();
}

QQuickShaderEffect::Status QQuickGenericShaderEffect::status() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QQuickShaderEffect::Error;

    return QQuickShaderEffect::Status(mgr->status());
}

QQuickShaderEffect::ShaderType QQuickGenericShaderEffect::shaderType() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QQuickShaderEffect::HLSL;

    return QQuickShaderEffect::ShaderType(mgr->shaderType());
}

QQuickShaderEffect::ShaderCompilationType QQuickGenericShaderEffect::shaderCompilationType() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QQuickShaderEffect::OfflineCompilation;

    return QQuickShaderEffect::ShaderCompilationType(mgr->shaderCompilationType());
}

QQuickShaderEffect::ShaderSourceType QQuickGenericShaderEffect::shaderSourceType() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QQuickShaderEffect::ShaderByteCode;

    return QQuickShaderEffect::ShaderSourceType(mgr->shaderSourceType());
}

void QQuickGenericShaderEffect::handleEvent(QEvent *event)
{
    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *e = static_cast<QDynamicPropertyChangeEvent *>(event);
        for (int shaderType = 0; shaderType < NShader; ++shaderType) {
            const auto &vars(m_shaders[shaderType].shaderInfo.variables);
            for (int idx = 0; idx < vars.count(); ++idx) {
                if (vars[idx].name == e->propertyName()) {
                    propertyChanged((shaderType << 16) | idx);
                    break;
                }
            }
        }
    }
}

void QQuickGenericShaderEffect::handleGeometryChanged(const QRectF &, const QRectF &)
{
    m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
}

QSGNode *QQuickGenericShaderEffect::handleUpdatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QSGShaderEffectNode *node = static_cast<QSGShaderEffectNode *>(oldNode);

    if (m_item->width() <= 0 || m_item->height() <= 0) {
        delete node;
        return nullptr;
    }

    // The manager should be already created on the gui thread. Just take that instance.
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr) {
        delete node;
        return nullptr;
    }

    if (!node) {
        QSGRenderContext *rc = QQuickWindowPrivate::get(m_item->window())->context;
        node = rc->sceneGraphContext()->createShaderEffectNode(rc, mgr);
        m_dirty = QSGShaderEffectNode::DirtyShaderAll;
    }

    // Dirty mesh and geometry are handled here, the rest is passed on to the node.
    if (m_dirty & QSGShaderEffectNode::DirtyShaderMesh) {
        node->setGeometry(nullptr);
        m_dirty &= ~QSGShaderEffectNode::DirtyShaderMesh;
        m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
    }

    if (m_dirty & QSGShaderEffectNode::DirtyShaderGeometry) {
        const QRectF rect(0, 0, m_item->width(), m_item->height());
        QQuickShaderEffectMesh *mesh = m_mesh ? m_mesh : &m_defaultMesh;
        QSGGeometry *geometry = node->geometry();

        geometry = mesh->updateGeometry(geometry, 2, 0, node->normalizedTextureSubRect(), rect);

        node->setFlag(QSGNode::OwnsGeometry, false);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry, true);

        m_dirty &= ~QSGShaderEffectNode::DirtyShaderGeometry;
    }

    QSGShaderEffectNode::SyncData sd;
    sd.dirty = m_dirty;
    sd.cullMode = QSGShaderEffectNode::CullMode(m_cullMode);
    sd.blending = m_blending;
    sd.supportsAtlasTextures = m_supportsAtlasTextures;
    sd.vertex.shader = &m_shaders[Vertex];
    sd.vertex.dirtyConstants = &m_dirtyConstants[Vertex];
    sd.vertex.dirtyTextures = &m_dirtyTextures[Vertex];
    sd.fragment.shader = &m_shaders[Fragment];
    sd.fragment.dirtyConstants = &m_dirtyConstants[Fragment];
    sd.fragment.dirtyTextures = &m_dirtyTextures[Fragment];

    node->sync(&sd);

    m_dirty = 0;
    for (int i = 0; i < NShader; ++i) {
        m_dirtyConstants[i].clear();
        m_dirtyTextures[i].clear();
    }

    return node;
}

void QQuickGenericShaderEffect::handleComponentComplete()
{
    updateShader(Vertex, m_vertShader);
    updateShader(Fragment, m_fragShader);
}

void QQuickGenericShaderEffect::handleItemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    // Move the window ref.
    if (change == QQuickItem::ItemSceneChange) {
        for (int shaderType = 0; shaderType < NShader; ++shaderType) {
            for (const auto &vd : qAsConst(m_shaders[shaderType].varData)) {
                if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
                    QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
                    if (source) {
                        if (value.window)
                            QQuickItemPrivate::get(source)->refWindow(value.window);
                        else
                            QQuickItemPrivate::get(source)->derefWindow();
                    }
                }
            }
        }
    }
}

QSGGuiThreadShaderEffectManager *QQuickGenericShaderEffect::shaderEffectManager() const
{
    if (!m_mgr) {
        // return null if this is not the gui thread and not already created
        if (QThread::currentThread() != m_item->thread())
            return m_mgr;
        // need a window and a rendercontext (i.e. the scenegraph backend is ready)
        QQuickWindow *w = m_item->window();
        if (w && w->isSceneGraphInitialized()) {
            m_mgr = QQuickWindowPrivate::get(w)->context->sceneGraphContext()->createGuiThreadShaderEffectManager();
            if (m_mgr) {
                QObject::connect(m_mgr, SIGNAL(logAndStatusChanged()), m_item, SIGNAL(logChanged()));
                QObject::connect(m_mgr, SIGNAL(logAndStatusChanged()), m_item, SIGNAL(statusChanged()));
                QObject::connect(m_mgr, SIGNAL(textureChanged()), this, SLOT(markGeometryDirtyAndUpdateIfSupportsAtlas()));
            }
        } else if (!w) {
            qWarning("ShaderEffect: Backend specifics cannot be queried until the item has a window");
        } else {
            qWarning("ShaderEffect: Backend specifics cannot be queried until the scenegraph has initialized");
        }
    }

    return m_mgr;
}

void QQuickGenericShaderEffect::disconnectSignals(Shader shaderType)
{
    for (auto &sm : m_signalMappers[shaderType]) {
        if (sm.active) {
            sm.active = false;
            QObject::disconnect(m_item, nullptr, sm.mapper, SLOT(map()));
            QObject::disconnect(sm.mapper, SIGNAL(mapped(int)), this, SLOT(propertyChanged(int)));
        }
    }
    for (const auto &vd : qAsConst(m_shaders[shaderType].varData)) {
        if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
            if (source) {
                if (m_item->window())
                    QQuickItemPrivate::get(source)->derefWindow();
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

void QQuickGenericShaderEffect::updateShader(Shader shaderType, const QByteArray &src)
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return;

    disconnectSignals(shaderType);

    m_shaders[shaderType].varData.clear();

    if (src.isEmpty()) {
        m_shaders[shaderType].valid = false;
        return;
    }

    // Figure out what input parameters and variables are used in the shader.
    // For file-based shader source/bytecode this is where the data is pulled
    // in from the file.
    QSGGuiThreadShaderEffectManager::ShaderInfo shaderInfo;
    // ### this will need some sort of caching mechanism
    if (!mgr->reflect(src, &shaderInfo)) {
        qWarning("ShaderEffect: shader reflection failed for %s", src.constData());
        m_shaders[shaderType].valid = false;
        return;
    }

    m_shaders[shaderType].shaderInfo = shaderInfo;
    m_shaders[shaderType].valid = true;

    const int varCount = shaderInfo.variables.count();
    m_shaders[shaderType].varData.resize(varCount);

    // Reuse signal mappers as much as possible since the mapping is based on
    // the index and shader type which are both constant.
    if (m_signalMappers[shaderType].count() < varCount)
        m_signalMappers[shaderType].resize(varCount);

    const bool texturesSeparate = mgr->hasSeparateSamplerAndTextureObjects();

    // Hook up the signals to get notified about changes for properties that
    // correspond to variables in the shader. Store also the values.
    for (int i = 0; i < varCount; ++i) {
        const auto &v(shaderInfo.variables.at(i));
        QSGShaderEffectNode::VariableData &vd(m_shaders[shaderType].varData[i]);
        const bool isSpecial = v.name.startsWith("qt_"); // special names not mapped to properties
        if (isSpecial) {
            if (v.name == QByteArrayLiteral("qt_Opacity"))
                vd.specialType = QSGShaderEffectNode::VariableData::Opacity;
            else if (v.name == QByteArrayLiteral("qt_Matrix"))
                vd.specialType = QSGShaderEffectNode::VariableData::Matrix;
            else if (v.name.startsWith("qt_SubRect_"))
                vd.specialType = QSGShaderEffectNode::VariableData::SubRect;
            continue;
        }

        // The value of a property corresponding to a sampler is the source
        // item ref, unless there are separate texture objects in which case
        // the sampler is ignored (here).
        if (v.type == QSGGuiThreadShaderEffectManager::ShaderInfo::Sampler) {
            if (texturesSeparate) {
                vd.specialType = QSGShaderEffectNode::VariableData::Unused;
                continue;
            } else {
                vd.specialType = QSGShaderEffectNode::VariableData::Source;
            }
        } else if (v.type == QSGGuiThreadShaderEffectManager::ShaderInfo::Texture) {
            Q_ASSERT(texturesSeparate);
            vd.specialType = QSGShaderEffectNode::VariableData::Source;
        } else {
            vd.specialType = QSGShaderEffectNode::VariableData::None;
        }

        // Find the property on the ShaderEffect item.
        const int propIdx = m_item->metaObject()->indexOfProperty(v.name.constData());
        if (propIdx >= 0) {
            QMetaProperty mp = m_item->metaObject()->property(propIdx);
            if (!mp.hasNotifySignal())
                qWarning("ShaderEffect: property '%s' does not have notification method", v.name.constData());

            // Have a QSignalMapper that emits mapped() with an index+type on each property change notify signal.
            auto &sm(m_signalMappers[shaderType][i]);
            if (!sm.mapper) {
                sm.mapper = new QSignalMapper;
                sm.mapper->setMapping(m_item, i | (shaderType << 16));
            }
            sm.active = true;
            const QByteArray signalName = '2' + mp.notifySignal().methodSignature();
            QObject::connect(m_item, signalName, sm.mapper, SLOT(map()));
            QObject::connect(sm.mapper, SIGNAL(mapped(int)), this, SLOT(propertyChanged(int)));
        } else {
            // Do not warn for dynamic properties.
            if (!m_item->property(v.name.constData()).isValid())
                qWarning("ShaderEffect: '%s' does not have a matching property!", v.name.constData());
        }

        vd.value = m_item->property(v.name.constData());

        if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
            if (source) {
                if (m_item->window())
                    QQuickItemPrivate::get(source)->refWindow(m_item->window());
                QObject::connect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

bool QQuickGenericShaderEffect::sourceIsUnique(QQuickItem *source, Shader typeToSkip, int indexToSkip) const
{
    for (int shaderType = 0; shaderType < NShader; ++shaderType) {
        for (int idx = 0; idx < m_shaders[shaderType].varData.count(); ++idx) {
            if (shaderType != typeToSkip || idx != indexToSkip) {
                const auto &vd(m_shaders[shaderType].varData[idx]);
                if (vd.specialType == QSGShaderEffectNode::VariableData::Source && qvariant_cast<QObject *>(vd.value) == source)
                    return false;
            }
        }
    }
    return true;
}

void QQuickGenericShaderEffect::propertyChanged(int mappedId)
{
    const Shader type = Shader(mappedId >> 16);
    const int idx = mappedId & 0xFFFF;
    const auto &v(m_shaders[type].shaderInfo.variables[idx]);
    auto &vd(m_shaders[type].varData[idx]);

    if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
        QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
        if (source) {
            if (m_item->window())
                QQuickItemPrivate::get(source)->derefWindow();
            // QObject::disconnect() will disconnect all matching connections.
            // If the same source has been attached to two separate
            // textures/samplers, then changing one of them would trigger both
            // to be disconnected. So check first.
            if (sourceIsUnique(source, type, idx))
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
        }

        vd.value = m_item->property(v.name.constData());

        source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
        if (source) {
            // 'source' needs a window to get a scene graph node. It usually gets one through its
            // parent, but if the source item is "inline" rather than a reference -- i.e.
            // "property variant source: Image { }" instead of "property variant source: foo" -- it
            // will not get a parent. In those cases, 'source' should get the window from 'item'.
            if (m_item->window())
                QQuickItemPrivate::get(source)->refWindow(m_item->window());
            QObject::connect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
        }

        m_dirty |= QSGShaderEffectNode::DirtyShaderTexture;
        m_dirtyTextures[type].insert(idx);

     } else {
        vd.value = m_item->property(v.name.constData());
        m_dirty |= QSGShaderEffectNode::DirtyShaderConstant;
        m_dirtyConstants[type].insert(idx);
    }

    m_item->update();
}

void QQuickGenericShaderEffect::sourceDestroyed(QObject *object)
{
    for (int shaderType = 0; shaderType < NShader; ++shaderType) {
        for (auto &vd : m_shaders[shaderType].varData) {
            if (vd.specialType == QSGShaderEffectNode::VariableData::Source && vd.value.canConvert<QObject *>()) {
                if (qvariant_cast<QObject *>(vd.value) == object)
                    vd.value = QVariant();
            }
        }
    }
}

void QQuickGenericShaderEffect::markGeometryDirtyAndUpdate()
{
    m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
    m_item->update();
}

void QQuickGenericShaderEffect::markGeometryDirtyAndUpdateIfSupportsAtlas()
{
    if (m_supportsAtlasTextures)
        markGeometryDirtyAndUpdate();
}

QT_END_NAMESPACE
