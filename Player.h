#ifndef DXTEST_PLAYER_H
#define DXTEST_PLAYER_H
/** \file
 * \brief Header to define Player class.
 */

#include "World.h"
#include <assert.h>
#include <windows.h>
#include <d3dx9.h>
extern "C"{
#include <clib/c.h>
#include <clib/suf/suf.h>
#include <clib/suf/sufbin.h>
#include <clib/timemeas.h>
}
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>

namespace dxtest{


class Game;

/// <summary>
/// The Player's class, maintaining camera's position and velocity, etc.
/// </summary>
class Player{
public:
	Player(Game &game);
	const Vec3d &getPos()const{return pos;}
	const Quatd &getRot()const{return rot;}
	void setPos(const Vec3d &apos){pos = apos;}
	void setRot(const Quatd &arot){rot = arot;}
	void think(double dt);
	void keyinput(double dt);
	void updateRot(){
		desiredRot = Quatd::rotation(py[0], 1, 0, 0).rotate(py[1], 0, 1, 0);
	}
	bool trymove(const Vec3d &delta, bool setvelo = false);

	void serialize(std::ostream &o);
	void unserialize(std::istream &i);

	/// <summary>
	/// Half-size of the Player along X axis.
	/// </summary>
	static const double boundWidth;

	/// <summary>
	/// Half-size of the Player along Z axis.
	/// </summary>
	static const double boundLength;

	/// <summary>
	/// Full-size of the Player along Z axis.
	/// </summary>
	static const double boundHeight;

	/// <summary>
	/// Height of eyes measured from feet.
	/// </summary>
	static const double eyeHeight;

	static const double movespeed; ///< Walk speed [meters per second]
	static const double runspeed; ///< Run speed [meters per second]
	static const double jumpspeed; ///< Speed set vertically when jumping [meters per second]
	static const double rotatespeed; ///< pi / 2, means it takes 4 seconds to look all the way around.

	BYTE oldKeys[256];

	enum MoveMode{ Walk, Fly, Ghost} moveMode;
	bool isFlying()const{return moveMode == Fly || moveMode == Ghost;}

	Game &game;
	Vec3d pos;
	Vec3d velo;
	Quatd rot;
	Quatd desiredRot;
	double py[2]; ///< Pitch and Yaw
	bool floorTouched;

	/// <summary>
	/// The current type of Cell being placed.
	/// </summary>
	Cell::Type curtype;

	/// <summary>
	/// The brick materials the Player has.
	/// </summary>
	int bricks[4];
};

}

#endif
