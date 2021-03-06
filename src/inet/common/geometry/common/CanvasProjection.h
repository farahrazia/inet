//
// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_CANVASPROJECTION_H
#define __INET_CANVASPROJECTION_H

#include "inet/common/geometry/common/Rotation.h"

namespace inet {

class INET_API CanvasProjection
{
  protected:
    Rotation rotation;
    cFigure::Point scale;
    cFigure::Point translation;

    static CanvasProjection defaultCanvasProjection;
    static std::map<const cCanvas *, const CanvasProjection *> canvasProjections;

  public:
    CanvasProjection() : scale(cFigure::Point(1, 1)) {}
    CanvasProjection(Rotation rotation, cFigure::Point translation);

    const Rotation& getRotation() const { return rotation; }
    void setRotation(const Rotation& rotation) { this->rotation = rotation; }

    const cFigure::Point& getScale() const { return scale; }
    void setScale(const cFigure::Point& scale) { this->scale = scale; }

    const cFigure::Point& getTranslation() const { return translation; }
    void setTranslation(const cFigure::Point& translation) { this->translation = translation; }

    cFigure::Point computeCanvasPoint(const Coord& point) const;
    cFigure::Point computeCanvasPoint(const Coord& point, double& depth) const;
    Coord computeCanvasPointInverse(const cFigure::Point& point, double depth) const;

    static const CanvasProjection *getCanvasProjection(const cCanvas *canvas);
    static void setCanvasProjection(const cCanvas *canvas, const CanvasProjection *canvasProjection);
};

} // namespace inet

#endif // ifndef __INET_CANVASPROJECTION_H

