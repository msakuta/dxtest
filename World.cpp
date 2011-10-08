#include "World.h"
#include "perlinNoise.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>

/// Initialize the volume.
void dxtest::CellVolume::initialize(){
	float field[CELLSIZE][CELLSIZE];
	PerlinNoise::perlin_noise<CELLSIZE>(PerlinNoise::PerlinNoiseParams(12321, 0.5), PerlinNoise::FieldAssign<CELLSIZE>(field));
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		v[ix][iy][iz] = CellInt(field[ix][iz] * CELLSIZE / 2 < iy ? Cell::Air : Cell::Grass);
	}
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		updateAdj(ix, iy, iz);
	}
}

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

