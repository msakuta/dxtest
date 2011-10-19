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
	CellVolume(){}
	const CellInt &operator()(int ix, int iy, int iz)const{
		return
			0 <= ix && ix < CELLSIZE &&
			0 <= iy && iy < CELLSIZE &&
			0 <= iz && iz < CELLSIZE 
			? v[ix][iy][iz] : v0;
	}
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



inline World::World(Game &agame) : game(agame), volume(operator<){game.world = this;}


}

#endif
