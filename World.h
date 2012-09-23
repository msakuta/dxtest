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

/// <summary>The atomic unit of the world.</summary>
class Cell{
public:
	enum Type{
		Air, ///< Null cell
		Grass, ///< Grass cell
		Dirt, ///< Dirt cell
		Gravel, ///< Gravel cell
		Rock, ///< Rock cell
		Water, ///< Water cell
		HalfBit = 0x8, ///< Bit indicating half height of base material.
		HalfAir = HalfBit | Air, ///< This does not really exist, just a theoretical entity.
		HalfGrass = HalfBit | Grass, ///< Half-height Grass.
		HalfDirt = HalfBit | Dirt,
		HalfGravel = HalfBit | Gravel,
		HalfRock = HalfBit | Rock,
		NumTypes
	};

	Cell(Type t = Air) : type(t), adjacents(0), adjacentWater(0){}
	Type getType()const{return type;}
	short getValue()const{return value;}
	void setValue(short avalue){value = avalue;}
	int getAdjacents()const{return adjacents;}
	int getAdjacentWaterCells()const{return adjacentWater;}
	bool isSolid()const{return type != Air && type != Water;}
	bool isTranslucent()const{return type == Air || type == Water || type & HalfBit;}
	void serialize(std::ostream &o);
	void unserialize(std::istream &i);
protected:
	enum Type type;
	short value;
	char adjacents;
	char adjacentWater;

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

	/// Scanlines for transparent cells
	int tranScanLines[CELLSIZE][CELLSIZE][2];

	int _solidcount;
	int bricks[Cell::NumTypes];

	void updateAdj(int ix, int iy, int iz);
public:
	CellVolume(World *world = NULL, const Vec3i &ind = Vec3i(0,0,0)) : world(world), index(ind), _solidcount(0){
		for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < 2; iz++){
			_scanLines[ix][iy][iz] = 0;
			tranScanLines[ix][iy][iz] = 0;
		}
		for(int i = 0; i < Cell::NumTypes; i++)
			bricks[i] = 0;
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
	bool setCell(int ix, int iy, int iz, const Cell &newCell);
	void initialize(const Vec3i &index);
	void updateCache();
	typedef int ScanLinesType[CELLSIZE][CELLSIZE][2];
	const ScanLinesType &getScanLines()const{
		return _scanLines;
	}
	const ScanLinesType &getTranScanLines()const{
		return tranScanLines;
	}
	int getSolidCount()const{return _solidcount;}
	int getBricks(int i)const{return bricks[i];}

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

	int bricks[Cell::NumTypes];

	void initialize();
	static Vec3i real2ind(const Vec3d &pos);
	static Vec3d ind2real(const Vec3i &ipos);

	World(Game &agame);

	const Cell &cell(int ix, int iy, int iz){
		Vec3i ci = Vec3i(SignDiv(ix, CELLSIZE), SignDiv(iy, CELLSIZE), SignDiv(iz, CELLSIZE));
		VolumeMap::iterator it = volume.find(ci);
		if(it != volume.end()){
			return it->second(SignModulo(ix, CELLSIZE), SignModulo(iy, CELLSIZE), SignModulo(iz, CELLSIZE));
		}
		else
			return CellVolume::v0;
	}
	const Cell &cell(const Vec3i &pos){
		return cell(pos[0], pos[1], pos[2]);
	}

	bool setCell(int ix, int iy, int iz, const Cell &newCell){
		Vec3i ci = Vec3i(SignDiv(ix, CELLSIZE), SignDiv(iy, CELLSIZE), SignDiv(iz, CELLSIZE));
		VolumeMap::iterator it = volume.find(ci);
		if(it != volume.end())
			return it->second.setCell(SignModulo(ix, CELLSIZE), SignModulo(iy, CELLSIZE), SignModulo(iz, CELLSIZE), newCell);
		return false;
	}

	bool isSolid(int ix, int iy, int iz){
		return isSolid(Vec3i(ix, iy, iz));
	}

	bool isSolid(const Vec3i &v);
	bool isSolid(const Vec3d &rv);

	int getBricks(int i){
		return bricks[i];
	}

	double boundaryHeight(const Vec3d &rv);

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

inline bool CellVolume::setCell(int ix, int iy, int iz, const Cell &newCell){
	if (ix < 0 || CELLSIZE <= ix || iy < 0 || CELLSIZE <= iy || iz < 0 || CELLSIZE <= iz)
		return false;
	else
	{
		// Update solidcount by difference of solidity before and after cell assignment.
		int before = v[ix][iy][iz].isSolid();
		int after = newCell.isSolid();
		_solidcount += after - before;

		v[ix][iy][iz] = newCell;
		updateCache();
		if(ix <= 0)
			world->volume[Vec3i(index[0] - 1, index[1], index[2])].updateCache();
		else if(CELLSIZE - 1 <= ix)
			world->volume[Vec3i(index[0] + 1, index[1], index[2])].updateCache();
		if(iy <= 0)
			world->volume[Vec3i(index[0], index[1] - 1, index[2])].updateCache();
		else if (CELLSIZE - 1 <= iy)
			world->volume[Vec3i(index[0], index[1] + 1, index[2])].updateCache();
		if(iz <= 0)
			world->volume[Vec3i(index[0], index[1], index[2] - 1)].updateCache();
		else if (CELLSIZE - 1 <= iz)
			world->volume[Vec3i(index[0], index[1], index[2] + 1)].updateCache();
		return true;
	}
}

}

#endif
