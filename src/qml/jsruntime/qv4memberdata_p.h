/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4MEMBERDATA_H
#define QV4MEMBERDATA_H

#include "qv4global_p.h"
#include "qv4managed_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct MemberData : Base {
    MemberData(InternalClass *c)
        : Base(c), size(0) {}
    union {
        uint size;
        double _dummy;
    };
    Value data[1];
};

}

struct MemberData : Managed
{
    V4_MANAGED(MemberData, Managed)

    Value &operator[] (uint idx) { return d()->data[idx]; }
    const Value *data() const { return d()->data; }
    Value *data() { return d()->data; }
    inline uint size() const { return d()->size; }

    static Heap::MemberData *reallocate(QV4::ExecutionEngine *e, Heap::MemberData *old, uint idx);

    static void markObjects(Heap::Base *that, ExecutionEngine *e);
};

}

QT_END_NAMESPACE

#endif
