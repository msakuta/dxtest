#include "World.h"
#include "Player.h"
#include "perlinNoise.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>
/** \file
 * \brief Implements World class.
 */

/// <summary>The constant object that will returned when index is out of range.</summary>
/// <remarks>What a mess.</remarks>
const dxtest::CellVolume::CellInt dxtest::CellVolume::v0(dxtest::Cell::Air);

/// <summary>
/// Initialize this CellVolume with Perlin Noise with given position index.
/// </summary>
/// <param name="ci">The position of new CellVolume</param>
void dxtest::CellVolume::initialize(const Vec3i &ci){
	float field[CELLSIZE][CELLSIZE];

	PerlinNoise::PerlinNoiseParams pnp(12321, 0.5);
	pnp.octaves = 7;
	pnp.xofs = ci[0] * CELLSIZE;
	pnp.yofs = ci[2] * CELLSIZE;
	PerlinNoise::perlin_noise<CELLSIZE>(pnp, PerlinNoise::FieldAssign<CELLSIZE>(field));

	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		v[ix][iy][iz] = CellInt(field[ix][iz] * CELLSIZE * 2 < iy + ci[1] * CELLSIZE ? Cell::Air : Cell::Grass);
	}
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		updateAdj(ix, iy, iz);
	}
}

void dxtest::World::initialize(){
	Vec3i zero(0, 0, 0);
	volume[zero] = CellVolume();
	volume[zero].initialize(zero);
}

/// <summary>
/// Convert from real world coords to massvolume index vector
/// </summary>
/// <param name="pos">world vector</param>
/// <returns>indices</returns>
Vec3i dxtest::World::real2ind(const Vec3d &pos){
	Vec3d tpos = pos;
	Vec3i vi(floor(tpos[0]), floor(tpos[1]), floor(tpos[2]));
	return vi + Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
}

/// <summary>
/// Convert from massvolume index to real world coords
/// </summary>
/// <param name="ipos">indices</param>
/// <returns>world vector</returns>
Vec3d dxtest::World::ind2real(const Vec3i &ipos){
	Vec3i tpos = ipos - Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
	return tpos.cast<double>();
}


void dxtest::World::think(double dt){
	Vec3i i = real2ind(game.player->getPos());
	for (int ix = -2; ix <= 2; ix++) for (int iy = 0; iy < 2; iy++) for (int iz = -2; iz <= 2; iz++){
		Vec3i ci(
			SignDiv((i[0] + ix * CELLSIZE), CELLSIZE),
			SignDiv((i[1] + (2 * iy - 1) * CELLSIZE / 2), CELLSIZE),
			SignDiv((i[2] + iz * CELLSIZE), CELLSIZE));
		if(volume.find(ci) == volume.end()){
			CellVolume &cv = volume[ci];
			cv.initialize(ci);
		}
	}
}
