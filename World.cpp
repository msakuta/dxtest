#include "World.h"
#include "Game.h"
#include "Player.h"
#include "perlinNoise.h"
#include "perlinNoise3d.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>
#include <vector>
/** \file
 * \brief Implements World class.
 */

namespace dxtest{

/// <summary>The constant object that will returned when index is out of range.</summary>
/// <remarks>What a mess.</remarks>
const Cell CellVolume::v0(Cell::Air);


int CellVolume::cellInvokes = 0;
int CellVolume::cellForeignInvokes = 0;
int CellVolume::cellForeignExists = 0;


/// <summary>
/// Initialize this CellVolume with Perlin Noise with given position index.
/// </summary>
/// <param name="ci">The position of new CellVolume</param>
void CellVolume::initialize(const Vec3i &ci){
	float field[CELLSIZE][CELLSIZE];

	PerlinNoise::PerlinNoiseParams3D pnp(12321, 0.5);
	pnp.octaves = 8;
	pnp.xofs = ci[0] * CELLSIZE;
	pnp.yofs = ci[2] * CELLSIZE;
	pnp.zofs = ci[1] * CELLSIZE;
	PerlinNoise::perlin_noise<CELLSIZE>(pnp, PerlinNoise::FieldAssign<CELLSIZE>(field));

	pnp.octaves = 7;
	pnp.yofs = ci[1] * CELLSIZE;
	pnp.zofs = ci[2] * CELLSIZE;
	float cellFactorTable[4][CELLSIZE][CELLSIZE][CELLSIZE];
	const unsigned long seeds[4] = {54123, 112398, 93532, 3417453};
	for(int i = 0; i < 4; i++){
		pnp.seed = seeds[i];
		PerlinNoise::perlin_noise_3D<CELLSIZE>(pnp, PerlinNoise::FieldAssign3D<CELLSIZE>(cellFactorTable[i]));
	}

#if 0
	int values[CELLSIZE][CELLSIZE][CELLSIZE] = {0};
	for(int iz = 0; iz < CELLSIZE; iz++){
		int fz = iz + ci[2] * CELLSIZE + 115;
		if(fz < 0)
			continue;
		if(230 <= fz)
			break;
		char cbuf[256];
		sprintf(cbuf, "s2\\Co-2-%d.txt", iz);
		FILE *fp = fopen(cbuf, "r");
		for(int fy = 0; fy < 2048; fy++){
			int iy = fy - ci[1] * CELLSIZE - 385;
			for(int fx = 0; fx < 224; fx++){
				int ix = fx - ci[0] * CELLSIZE - 112;
				int val;
				fscanf(fp, "%d\t", &val);
				if(0 <= iy && iy < CELLSIZE && 0 <= ix && ix < CELLSIZE)
					values[iz][iy][ix] = val;
			}
		}
		fclose(fp);
	}
#endif

	_solidcount = 0;
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iz = 0; iz < CELLSIZE; iz++){
		// Compute the height first. The height is distance from the surface just below the cell of interest,
		// can be negative when it's below surface.
		int baseHeight = ci[1] * CELLSIZE - ((int)floor(field[ix][iz] * CELLSIZE * 4) - 16);

		for(int iy = 0; iy < CELLSIZE; iy++){
			int height = iy + baseHeight;

			if(0 < height)
				v[ix][iy][iz] = Cell(ci[1] * CELLSIZE + iy < 0 ? Cell::Water : Cell::Air);
			else{
				float grassness = 0 < height || height < -10 ? 0 : cellFactorTable[0][ix][iy][iz] / (1 << -height);
				float dirtness = cellFactorTable[1][ix][iy][iz];
				float gravelness = cellFactorTable[2][ix][iy][iz];
				float rockness = cellFactorTable[3][ix][iy][iz] * (height < 0 ? 1.25 - 0.5 / (1. - height / 16.) : 0.75);

				Cell::Type ct;
				if (dirtness < grassness && gravelness < grassness && rockness < grassness)
					ct = Cell::Grass;
				else if (gravelness < dirtness && rockness < dirtness)
					ct = Cell::Dirt;
				else if(rockness < gravelness)
					ct = Cell::Gravel;
				else
					ct = Cell::Rock;
				world->bricks[ct]++;
				v[ix][iy][iz] = Cell(ct);
				_solidcount++;
			}
		}
	}
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		updateAdj(ix, iy, iz);
	}

}

World::World(Game &agame) : game(agame), volume(operator<){
	game.world = this;
	for(int i = 0; i < Cell::NumTypes; i++)
		bricks[i] = 0;
}

void World::initialize(){
}

/// <summary>
/// Convert from real world coords to massvolume index vector
/// </summary>
/// <param name="pos">world vector</param>
/// <returns>indices</returns>
Vec3i World::real2ind(const Vec3d &pos){
	Vec3d tpos = pos;
	Vec3i vi((int)floor(tpos[0]), (int)floor(tpos[1]), (int)floor(tpos[2]));
	return vi + Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
}

/// <summary>
/// Convert from massvolume index to real world coords
/// </summary>
/// <param name="ipos">indices</param>
/// <returns>world vector</returns>
Vec3d World::ind2real(const Vec3i &ipos){
	Vec3i tpos = ipos - Vec3i(CELLSIZE, CELLSIZE, CELLSIZE) / 2;
	return tpos.cast<double>();
}

/// <summary>Solidity check for given index coordinates</summary>
bool World::isSolid(const Vec3i &v){
	Vec3i ci(SignDiv(v[0], CELLSIZE), SignDiv(v[1], CELLSIZE), SignDiv(v[2], CELLSIZE));
	if(volume.find(ci) != volume.end()){
		CellVolume &cv = volume[ci];
		return cv.isSolid(Vec3i(SignModulo(v[0], CELLSIZE), SignModulo(v[1], CELLSIZE), SignModulo(v[2], CELLSIZE)));
	}
	else
		return false;
}

/// <summary>Solidity check for given world coordinates</summary>
bool World::isSolid(const Vec3d &rv){
	Vec3i v = real2ind(rv);
	Vec3i ci(SignDiv(v[0], CELLSIZE), SignDiv(v[1], CELLSIZE), SignDiv(v[2], CELLSIZE));
	if(volume.find(ci) != volume.end()){
		CellVolume &cv = volume[ci];
		const Cell &c = cv(SignModulo(v[0], CELLSIZE), SignModulo(v[1], CELLSIZE), SignModulo(v[2], CELLSIZE));
		return c.getType() & Cell::HalfBit ? rv[1] - floor(rv[1]) < .5 : c.isSolid();
	}
	else
		return false;
}

/// <summary>Returns distance to nearest possible boundary upwards.</summary>
/// <remarks>The return value means how deep this point is from the surface. Only meaningful for solid cells.</remarks>
double World::boundaryHeight(const Vec3d &rv){
	Vec3i v = real2ind(rv);
	Vec3i ci(SignDiv(v[0], CELLSIZE), SignDiv(v[1], CELLSIZE), SignDiv(v[2], CELLSIZE));
	if(volume.find(ci) != volume.end()){
		CellVolume &cv = volume[ci];
		const Cell &c = cv(SignModulo(v[0], CELLSIZE), SignModulo(v[1], CELLSIZE), SignModulo(v[2], CELLSIZE));
		return c.getType() & Cell::HalfBit ? ceil(rv[1] - .5) - (rv[1] - 0.5) : ceil(rv[1]) - rv[1];
	}
	else
		return 0.;
}

void World::think(double dt){
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

void CellVolume::updateAdj(int ix, int iy, int iz){
	v[ix][iy][iz].adjacents =
		(!cell(ix - 1, iy, iz).isTranslucent() ? 1 : 0) +
        (!cell(ix + 1, iy, iz).isTranslucent() ? 1 : 0) +
        (!cell(ix, iy - 1, iz).isTranslucent() ? 1 : 0) +
        (!cell(ix, iy + 1, iz).isTranslucent() ? 1 : 0) +
        (!cell(ix, iy, iz - 1).isTranslucent() ? 1 : 0) +
        (!cell(ix, iy, iz + 1).isTranslucent() ? 1 : 0);
	v[ix][iy][iz].adjacentWater =
		(cell(ix - 1, iy, iz).getType() == Cell::Water ? 1 : 0) +
        (cell(ix + 1, iy, iz).getType() == Cell::Water ? 1 : 0) +
        (cell(ix, iy - 1, iz).getType() == Cell::Water ? 1 : 0) +
        (cell(ix, iy + 1, iz).getType() == Cell::Water ? 1 : 0) +
        (cell(ix, iy, iz - 1).getType() == Cell::Water ? 1 : 0) +
        (cell(ix, iy, iz + 1).getType() == Cell::Water ? 1 : 0);
}

void CellVolume::updateCache()
{
	for (int ix = 0; ix < CELLSIZE; ix++) for (int iy = 0; iy < CELLSIZE; iy++) for (int iz = 0; iz < CELLSIZE; iz++)
		updateAdj(ix, iy, iz);
    
	// Build up scanline map
	for (int ix = 0; ix < CELLSIZE; ix++) for (int iz = 0; iz < CELLSIZE; iz++)
	{
		// Find start and end points for this scan line
		bool begun = false;
		bool transBegun = false;
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

			if(c.type == Cell::Water && c.adjacents != 6 && c.adjacentWater != 6 && c.adjacents + c.adjacentWater != 6){
				if(!transBegun){
					transBegun = true;
					tranScanLines[ix][iz][0] = iy;
				}
				tranScanLines[ix][iz][1] = iy + 1;
			}
		}
	}
}

void World::serialize(std::ostream &o){
	int count = volume.size();
	o.write((char*)&count, sizeof count);
	for(VolumeMap::iterator it = volume.begin(); it != volume.end(); it++)
		it->second.serialize(o);
}

void World::unserialize(std::istream &is){
	try
	{
		volume.clear();
		int count;
		is.read((char*)&count, sizeof count);
		for (int i = 0; i < count; i++)
		{
			CellVolume cv(this);
			cv.unserialize(is);
			volume.insert(std::pair<Vec3i, CellVolume>(cv.getIndex(), cv));
		}
		for(VolumeMap::iterator it = volume.begin(); it != volume.end(); it++)
			it->second.updateCache();
	}
	catch(std::exception &e)
	{
		*game.logwriter << e.what() << std::endl;
		return;
	}
}

}
