/*
 *  Copyright (c) 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "KoShapeMeshGradientHandles.h"

#include <QVector>

#include <KoShapeFillWrapper.h>
#include <kis_algebra_2d.h>

KoShapeMeshGradientHandles::KoShapeMeshGradientHandles(KoFlake::FillVariant fillVariant,
                                                       KoShape *shape)
    : m_fillVariant(fillVariant)
    , m_shape(shape)
{
}

QVector<KoShapeMeshGradientHandles::Handle> KoShapeMeshGradientHandles::handles() const
{
    QVector<Handle> result;

    const SvgMeshGradient *g = gradient();
    if (!g) return result;

    SvgMeshArray const *mesharray = g->getMeshArray().data();

    for (int irow = 0; irow < mesharray->numRows(); ++irow) {
        for (int icol = 0; icol < mesharray->numColumns(); ++icol) {
            // add corners as well
            result << getHandles(mesharray, SvgMeshPatch::Top, irow, icol);

            result << getBezierHandles(mesharray, SvgMeshPatch::Left, irow, icol);

            if (irow == mesharray->numRows() - 1) {
                result << getHandles(mesharray, SvgMeshPatch::Left, irow, icol);

                if (icol == mesharray->numColumns() - 1) {
                    result << getHandles(mesharray, SvgMeshPatch::Bottom, irow, icol);
                } else {
                    result << getBezierHandles(mesharray, SvgMeshPatch::Bottom, irow, icol);
                }
            }

            if (icol == mesharray->numColumns() - 1) {
                result << getHandles(mesharray, SvgMeshPatch::Right, irow, icol);
            }
        }
    }

    // we get pointer events in points (pts, not logical), so we transform these now
    // and then invert them while drawing handles (see SelectionDecorator).
    QTransform t = abosoluteTransformation(g->gradientUnits());
    for (auto &handle: result) {
        handle.pos = t.map(handle.pos);
    }

    return result;
}

KUndo2Command* KoShapeMeshGradientHandles::moveGradientHandle(const Handle &handle,
                                                              const QPointF &newPos)
{
    KoShapeFillWrapper wrapper(m_shape, m_fillVariant);
    SvgMeshGradient *newGradient = new SvgMeshGradient(*wrapper.meshgradient());
    SvgMeshArray *mesharray = newGradient->getMeshArray().data();
    SvgMeshPatch *patch = newGradient->getMeshArray()->getPatch(handle.row, handle.col);
    std::array<QPointF, 4> path = patch->getSegment(handle.segmentType);

    QTransform t = abosoluteTransformation(newGradient->gradientUnits()).inverted();

    if (handle.type == Handle::BezierHandle) {
        int index = getHandleIndex(path, handle.pos);
        path[index] = t.map(newPos);
        mesharray->modifyHandle(SvgMeshPosition {handle.row, handle.col, handle.segmentType}, path);

    } else if (handle.type == Handle::Corner) {
        mesharray->modifyCorner(SvgMeshPosition {handle.row, handle.col, handle.segmentType}, t.map(newPos));
    }

    return wrapper.setMeshGradient(newGradient, QTransform());
}

QPainterPath KoShapeMeshGradientHandles::path() const
{
    QPainterPath painterPath;

    if (!gradient())
        return painterPath;

    SvgMeshGradient *g = new SvgMeshGradient(*gradient());
    if (g->gradientUnits() == KoFlake::ObjectBoundingBox) {
        const QTransform gradientToUser = KisAlgebra2D::mapToRect(m_shape->outlineRect());
        g->setTransform(gradientToUser);
    }

    SvgMeshArray *mesharray = g->getMeshArray().data();

    for (int i = 0; i < mesharray->numRows(); ++i) {
        for (int j = 0; j < mesharray->numColumns(); ++j) {
            painterPath.addPath(mesharray->getPatch(i, j)->getPath());
        }
    }

    return painterPath;
}

int KoShapeMeshGradientHandles::getHandleIndex(const std::array<QPointF, 4> &path, QPointF point)
{
    QTransform t = abosoluteTransformation(gradient()->gradientUnits());
    int index = 0;
    for (; index < 4; ++index) {
        if (t.map(path[index]) == point) {
            return index;
        }
    }
    KIS_ASSERT(false); // a bug
    return -1;
}

const SvgMeshGradient* KoShapeMeshGradientHandles::gradient() const
{
    KoShapeFillWrapper wrapper(m_shape, m_fillVariant);
    return wrapper.meshgradient();
}

QVector<KoShapeMeshGradientHandles::Handle> KoShapeMeshGradientHandles::getHandles(const SvgMeshArray *mesharray,
                                                                                   SvgMeshPatch::Type type,
                                                                                   int row,
                                                                                   int col) const
{
    QVector<Handle> buffer;
    std::array<QPointF, 4> path = mesharray->getPath(type, row, col);
    buffer << Handle(Handle::Corner, path[0], row, col, type);
    buffer << Handle(Handle::BezierHandle, path[1], row, col, type);
    buffer << Handle(Handle::BezierHandle, path[2], row, col, type);

    return buffer;
}

QVector<KoShapeMeshGradientHandles::Handle> KoShapeMeshGradientHandles::getBezierHandles(const SvgMeshArray *mesharray,
                                                                                         SvgMeshPatch::Type type,
                                                                                         int row,
                                                                                         int col) const
{
    QVector<Handle> buffer;
    std::array<QPointF, 4> path = mesharray->getPath(type, row, col);
    buffer << Handle(Handle::BezierHandle, path[1], row, col, type);
    buffer << Handle(Handle::BezierHandle, path[2], row, col, type);

    return buffer;
}

QTransform KoShapeMeshGradientHandles::abosoluteTransformation(KoFlake::CoordinateSystem system) const
{
    QTransform t;
    if (system == KoFlake::UserSpaceOnUse) {
        t = m_shape->absoluteTransformation();
    } else {
        const QTransform gradientToUser = KisAlgebra2D::mapToRect(m_shape->outlineRect());
        t = gradientToUser * m_shape->absoluteTransformation();
    }
    return t;
}