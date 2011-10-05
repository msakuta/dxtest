#include "World.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>

/// Convert from real world coords to massvolume index vector
Vec3i dxtest::World::real2ind(const Vec3d &pos){
	Vec3d tpos = pos + Vec3d(0,-1.7,0);
	Vec3i vi(floor(tpos[0]), floor(tpos[1]), floor(tpos[2]));
	return vi + Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
}

/// Convert from massvolume index to real world coords
Vec3d dxtest::World::ind2real(const Vec3i &ipos){
	Vec3i tpos = ipos - Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
	return tpos.cast<double>() - Vec3d(0,-1.7,0);
}

