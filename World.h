#ifndef DXTEST_WORLD_H
#define DXTEST_WORLD_H
/** \file
 * \brief Header to define World class and its companion classes.
 */

#include "perlinNoise.h"
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>

namespace dxtest{

const int CELLSIZE = 32;

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
		return v[ix][iy][iz];
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
	void initialize(){
		float field[CELLSIZE][CELLSIZE];
		PerlinNoise::perlin_noise<CELLSIZE>(PerlinNoise::PerlinNoiseParams(12321, 0.5), PerlinNoise::FieldAssign<CELLSIZE>(field));
		for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
			v[ix][iy][iz] = CellInt(field[ix][iz] * CELLSIZE / 2 < iy ? Cell::Air : Cell::Grass);
		}
		for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
			updateAdj(ix, iy, iz);
		}
	}
};

class World{
public:
	CellVolume volume;
	static Vec3i real2ind(const Vec3d &pos);
	static Vec3d ind2real(const Vec3i &ipos);
};


}

#endif
