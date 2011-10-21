#ifndef DXTEST_WORLD_H
#define DXTEST_WORLD_H
/** \file
 * \brief Header to define World class and its companion classes.
 */

#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <map>
#include "SignModulo.h"

namespace dxtest{

const int CELLSIZE = 16;

class World;

class Cell{
public:
	enum Type{Air, Grass, Banana};

	Cell(Type t = Air) : type(t){}
	Type getType()const{return type;}
protected:
	enum Type type;
};

class CellVolume{
public:
	class CellInt : public Cell{
	public:
		CellInt(Type t = Air) : Cell(t), adjcents(0){}
		int adjcents;
	};

	static const CellInt v0;

protected:
	World *world;
	Vec3i index;
	CellInt v[CELLSIZE][CELLSIZE][CELLSIZE];

	void updateAdj(int ix, int iy, int iz){
		v[ix][iy][iz].adjcents =
			(0 < ix ? v[ix-1][iy][iz].getType() != Cell::Air : 0) +
			(ix < CELLSIZE-1 ? v[ix+1][iy][iz].getType() != Cell::Air : 0) +
			(0 < iy ? v[ix][iy-1][iz].getType() != Cell::Air : 0) +
			(iy < CELLSIZE-1 ? v[ix][iy+1][iz].getType() != Cell::Air : 0) +
			(0 < iz ? v[ix][iy][iz-1].getType() != Cell::Air : 0) +
			(iz < CELLSIZE-1 ? v[ix][iy][iz+1].getType() != Cell::Air : 0);
	}
public:
	CellVolume(World *world = NULL, const Vec3i &ind = Vec3i(0,0,0)) : world(world), index(ind){}
	const CellInt &operator()(int ix, int iy, int iz)const;
	bool isSolid(const Vec3i &ipos)const{
		return
			0 <= ipos[0] && ipos[0] < CELLSIZE &&
			0 <= ipos[1] && ipos[1] < CELLSIZE &&
			0 <= ipos[2] && ipos[2] < CELLSIZE &&
			v[ipos[0]][ipos[1]][ipos[2]].getType() != Cell::Air;
	}
	void setCell(const Cell &c, int ix, int iy, int iz){
		v[ix][iy][iz] = CellInt(c.getType());
	}
	void initialize(const Vec3i &index);
};

inline bool operator<(const Vec3i &a, const Vec3i &b){
	if(a[0] < b[0])
		return true;
	else if(b[0] < a[0])
		return false;
	else if(a[1] < b[1])
		return true;
	else if(b[1] < a[1])
		return false;
	else if(a[2] < b[2])
		return true;
	else
		return false;
}

class Game;

class World{
public:
	typedef std::map<Vec3i, CellVolume, bool(*)(const Vec3i &, const Vec3i &)> VolumeMap;
	VolumeMap volume;

	Game &game;

	void initialize();
	static Vec3i real2ind(const Vec3d &pos);
	static Vec3d ind2real(const Vec3i &ipos);

	World(Game &agame);

	const CellVolume::CellInt &cell(int ix, int iy, int iz){
		if(volume.find(Vec3i(ix, iy, iz)) != volume.end()){
			CellVolume &cv = volume[Vec3i(ix, iy, iz)];
			return cv(ix - ix / CELLSIZE * CELLSIZE, iy - iy / CELLSIZE * CELLSIZE, iz - iz / CELLSIZE * CELLSIZE);
		}
		else
			return CellVolume::v0;
	}

	bool isSolid(int ix, int iy, int iz){
		return isSolid(Vec3i(ix, iy, iz));
	}

	bool isSolid(const Vec3i &v){
		Vec3i ci(SignDiv(v[0], CELLSIZE), SignDiv(v[1], CELLSIZE), SignDiv(v[2], CELLSIZE));
		if(volume.find(ci) != volume.end()){
			CellVolume &cv = volume[ci];
			return cv.isSolid(Vec3i(SignModulo(v[0], CELLSIZE), SignModulo(v[1], CELLSIZE), SignModulo(v[2], CELLSIZE)));
		}
		else
			return false;
	}

	void think(double dt);
};


class Player;

class Game{
public:
	Player *player;
	World *world;
};

/// <summary>Returns Cell object indexed by coordinates in this CellVolume.</summary>
/// <remarks>
/// If the indices reach border of the CellVolume, it will recursively retrieve foreign Cells.
///
/// Note that even if two or more indices are out of range, this function will find the correct Cell
/// by recursively calling itself in turn with each axes.
/// But what's the difference between this and World::cell(), you may ask. That's the key for the next
/// step of optimization.
/// </remarks>
/// <param name="ix">Index along X axis in Cells. If in range [0, CELLSIZE), this object's member is returned.</param>
/// <param name="iy">Index along Y axis in Cells. If in range [0, CELLSIZE), this object's member is returned.</param>
/// <param name="iz">Index along Z axis in Cells. If in range [0, CELLSIZE), this object's member is returned.</param>
/// <returns>A CellInt object at ix, iy, iz</returns>
inline const CellVolume::CellInt &CellVolume::operator()(int ix, int iy, int iz)const{
	if(ix < 0 || CELLSIZE <= ix){
		Vec3i ci(index[0] + SignDiv(ix, CELLSIZE), index[1], index[2]);
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			const CellVolume &cv = it->second;
			return cv(SignModulo(ix, CELLSIZE), iy, iz);
		}
	}
	if(iy < 0 || CELLSIZE <= iy){
		Vec3i ci(index[0], index[1] + SignDiv(iy, CELLSIZE), index[2]);
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			const CellVolume &cv = it->second;
			return cv(ix, SignModulo(iy, CELLSIZE), iz);
		}
	}
	if(iz < 0 || CELLSIZE <= iz){
		Vec3i ci(index[0], index[1], index[2] + SignDiv(iz, CELLSIZE));
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			const CellVolume &cv = it->second;
			return cv(ix, iy, SignModulo(iz, CELLSIZE));
		}
	}
	return
		0 <= ix && ix < CELLSIZE &&
		0 <= iy && iy < CELLSIZE &&
		0 <= iz && iz < CELLSIZE 
		? v[ix][iy][iz] : v0;
}

inline World::World(Game &agame) : game(agame), volume(operator<){game.world = this;}


}

#endif
