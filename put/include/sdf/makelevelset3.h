#ifndef MAKELEVELSET3_H
#define MAKELEVELSET3_H

#include "types.h"
#include "array3.h"
#include "maths/vec.h"


// tri is a list of triangles in the mesh, and x is the positions of the vertices
// absolute distances will be nearly correct for triangle soup, but a closed mesh is
// needed for accurate signs. Distances for all grid cells within exact_band cells of
// a triangle should be exact; further away a distance is calculated but it might not
// be to the closest triangle - just one nearby.
void make_level_set3(const std::vector<Vec3ui> &tri, const std::vector<Vec3f> &x,
                     const Vec3f &origin, float dx, int nx, int ny, int nz,
                     Array3f &phi, const int exact_band=1);

#endif
