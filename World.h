#ifndef DXTEST_WORLD_H
#define DXTEST_WORLD_H
/** \file
 * \brief Header to define World class and its companion classes.
 */

#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <map>
#include <fstream>
#include "SignModulo.h"

namespace dxtest{

const int CELLSIZE = 16;

class Game;
class World;

class Cell{
public:
	enum Type{Air, Grass, Dirt, Gravel, NumTypes};

	Cell(Type t = Air) : type(t), adjacents(0){}
	Type getType()const{return type;}
	int getAdjacents()const{return adjacents;}
	bool isSolid()const{return type != Air;}
	void serialize(std::ostream &o);
	void unserialize(std::istream &i);
protected:
	enum Type type;
	int adjacents;

	friend class CellVolume;
};

class CellVolume{
public:
	static const Cell v0;

protected:
	World *world;
	Vec3i index;
	Cell v[CELLSIZE][CELLSIZE][CELLSIZE];

	/// <summary>
	/// Indices are in order of [X, Z, beginning and end]
	/// </summary>
	int _scanLines[CELLSIZE][CELLSIZE][2];

	int _solidcount;

	void updateAdj(int ix, int iy, int iz){
		v[ix][iy][iz].adjacents =
			(cell(ix - 1, iy, iz).type != Cell::Air ? 1 : 0) +
            (cell(ix + 1, iy, iz).type != Cell::Air ? 1 : 0) +
            (cell(ix, iy - 1, iz).type != Cell::Air ? 1 : 0) +
            (cell(ix, iy + 1, iz).type != Cell::Air ? 1 : 0) +
            (cell(ix, iy, iz - 1).type != Cell::Air ? 1 : 0) +
            (cell(ix, iy, iz + 1).type != Cell::Air ? 1 : 0);
/*		v[ix][iy][iz].adjacents =
			(0 < ix ? v[ix-1][iy][iz].getType() != Cell::Air : 0) +
			(ix < CELLSIZE-1 ? v[ix+1][iy][iz].getType() != Cell::Air : 0) +
			(0 < iy ? v[ix][iy-1][iz].getType() != Cell::Air : 0) +
			(iy < CELLSIZE-1 ? v[ix][iy+1][iz].getType() != Cell::Air : 0) +
			(0 < iz ? v[ix][iy][iz-1].getType() != Cell::Air : 0) +
			(iz < CELLSIZE-1 ? v[ix][iy][iz+1].getType() != Cell::Air : 0)*/;
	}
public:
	CellVolume(World *world = NULL, const Vec3i &ind = Vec3i(0,0,0)) : world(world), index(ind), _solidcount(0){
		for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < 2; iz++)
			_scanLines[ix][iy][iz] = 0;
	}
	const Vec3i &getIndex()const{return index;}
	const Cell &operator()(int ix, int iy, int iz)const;
	const Cell &cell(int ix, int iy, int iz)const{
		return operator()(ix, iy, iz);
	}
	bool isSolid(const Vec3i &ipos)const{
		return
			0 <= ipos[0] && ipos[0] < CELLSIZE &&
			0 <= ipos[1] && ipos[1] < CELLSIZE &&
			0 <= ipos[2] && ipos[2] < CELLSIZE &&
			v[ipos[0]][ipos[1]][ipos[2]].getType() != Cell::Air;
	}
	void setCell(const Cell &c, int ix, int iy, int iz){
		v[ix][iy][iz] = Cell(c.getType());
	}
	void initialize(const Vec3i &index);
	void updateCache();
	typedef int ScanLinesType[CELLSIZE][CELLSIZE][2];
	const ScanLinesType &getScanLines()const{
		return _scanLines;
	}
	int getSolidCount()const{return _solidcount;}

	void serialize(std::ostream &o);
	void unserialize(std::istream &i);

	static int cellInvokes;
	static int cellForeignInvokes;
	static int cellForeignExists;
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

	const Cell &cell(int ix, int iy, int iz){
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

	void serialize(std::ostream &o);
	void unserialize(std::istream &i);
};




// ----------------------------------------------------------------------------
//                            Implementation
// ----------------------------------------------------------------------------

inline void Cell::serialize(std::ostream &o){
	char b = (char)type;
	o.write(&b, 1);
}

inline void Cell::unserialize(std::istream &i){
	char b;
	i.read(&b, 1);
	type = (Type)b;
}


inline void CellVolume::serialize(std::ostream &o){
	o.write((char*)&index, sizeof index);
	o.write((char*)&_solidcount, sizeof _solidcount);
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++)
		v[ix][iy][iz].serialize(o);
}

inline void CellVolume::unserialize(std::istream &i){
	i.read((char*)&index, sizeof index);
	i.read((char*)&_solidcount, sizeof _solidcount);
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++)
		v[ix][iy][iz].unserialize(i);
}


class Player;

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
inline const Cell &CellVolume::operator()(int ix, int iy, int iz)const{
	cellInvokes++;
	if(ix < 0 || CELLSIZE <= ix){
		cellForeignInvokes++;
		Vec3i ci(index[0] + SignDiv(ix, CELLSIZE), index[1], index[2]);
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			cellForeignExists++;
			const CellVolume &cv = it->second;
			return cv(SignModulo(ix, CELLSIZE), iy, iz);
		}
		else
			return (*this)(ix < 0 ? 0 : CELLSIZE - 1, iy, iz);
	}
	if(iy < 0 || CELLSIZE <= iy){
		cellForeignInvokes++;
		Vec3i ci(index[0], index[1] + SignDiv(iy, CELLSIZE), index[2]);
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			cellForeignExists++;
			const CellVolume &cv = it->second;
			return cv(ix, SignModulo(iy, CELLSIZE), iz);
		}
		else
			return (*this)(ix, iy < 0 ? 0 : CELLSIZE - 1, iz);
	}
	if(iz < 0 || CELLSIZE <= iz){
		cellForeignInvokes++;
		Vec3i ci(index[0], index[1], index[2] + SignDiv(iz, CELLSIZE));
		World::VolumeMap::iterator it = world->volume.find(ci);
		if(it != world->volume.end()){
			cellForeignExists++;
			const CellVolume &cv = it->second;
			return cv(ix, iy, SignModulo(iz, CELLSIZE));
		}
		else
			return (*this)(ix, iy, iz < 0 ? 0 : CELLSIZE - 1);
	}
	return
		0 <= ix && ix < CELLSIZE &&
		0 <= iy && iy < CELLSIZE &&
		0 <= iz && iz < CELLSIZE 
		? v[ix][iy][iz] : v0;
}


}

#endif
