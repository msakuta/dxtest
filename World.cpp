#include "World.h"
#include "Game.h"
#include "Player.h"
#include "perlinNoise.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>
#include <vector>
/** \file
 * \brief Implements World class.
 */

/// <summary>The constant object that will returned when index is out of range.</summary>
/// <remarks>What a mess.</remarks>
const dxtest::Cell dxtest::CellVolume::v0(dxtest::Cell::Air);


int dxtest::CellVolume::cellInvokes = 0;
int dxtest::CellVolume::cellForeignInvokes = 0;
int dxtest::CellVolume::cellForeignExists = 0;


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

	_solidcount = 0;
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		v[ix][iy][iz] = Cell(field[ix][iz] * CELLSIZE * 2 < iy + ci[1] * CELLSIZE ? Cell::Air : Cell::Grass);
		if(v[ix][iy][iz].type != Cell::Air)
			_solidcount++;
	}
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		updateAdj(ix, iy, iz);
	}
}

dxtest::World::World(dxtest::Game &agame) : game(agame), volume(operator<){
	game.world = this;
}

void dxtest::World::initialize(){
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
	std::vector<CellVolume*> changed;
	int radius = Game::maxViewDistance / CELLSIZE;
	for (int ix = -radius; ix <= radius; ix++) for (int iy = 0; iy < 2; iy++) for (int iz = -radius; iz <= radius; iz++){
		Vec3i ci(
			SignDiv((i[0] + ix * CELLSIZE), CELLSIZE),
			SignDiv((i[1] + (2 * iy - 1) * CELLSIZE / 2), CELLSIZE),
			SignDiv((i[2] + iz * CELLSIZE), CELLSIZE));
		if(volume.find(ci) == volume.end()){
			volume[ci] = CellVolume(this, ci);
			CellVolume &cv = volume[ci];
			cv.initialize(ci);
			changed.push_back(&volume[ci]);
		}
	}

	for(std::vector<CellVolume*>::iterator it = changed.begin(); it != changed.end(); it++)
		(*it)->updateCache();
}

void dxtest::CellVolume::updateCache()
{
	for (int ix = 0; ix < CELLSIZE; ix++) for (int iy = 0; iy < CELLSIZE; iy++) for (int iz = 0; iz < CELLSIZE; iz++)
		updateAdj(ix, iy, iz);
    
	// Build up scanline map
	for (int ix = 0; ix < CELLSIZE; ix++) for (int iz = 0; iz < CELLSIZE; iz++)
	{
		// Find start and end points for this scan line
		bool begun = false;
		for (int iy = 0; iy < CELLSIZE; iy++)
		{
			Cell &c = v[ix][iy][iz];
			if (c.type != Cell::Air && (c.adjacents != 0 && c.adjacents != 6))
			{
				if (!begun)
				{
					begun = true;
					_scanLines[ix][iz][0] = iy;
				}
				_scanLines[ix][iz][1] = iy + 1;
			}
		}
	}
}
