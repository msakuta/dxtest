#include "Player.h"
#include "World.h"
#include "Game.h"
extern "C"{
#include <clib/mathdef.h>
}
/** \file
 * \brief Implements Player class
 */

namespace dxtest{

const double Player::boundWidth = 0.4;
const double Player::boundLength = 0.4;
const double Player::boundHeight = 1.7;
const double Player::eyeHeight = 1.5;
const double Player::movespeed = 2.; ///< Walk speed [meters per second]
const double Player::runspeed = 5.;
const double Player::jumpspeed = 5.; ///< Speed set vertically when jumping [meters per second]
const double Player::rotatespeed = acos(0.) / 1.; ///< pi / 2, means it takes 4 seconds to look all the way around.


Player::Player(Game &game) : game(game), pos(0, CELLSIZE, 0), velo(0,0,0), rot(0,0,0,1), desiredRot(0,0,0,1){
	py[0] = py[1] = 0.;
	game.player = this;
	curtype = Cell::Grass;
	bricks[0] = bricks[1] = bricks[2] = bricks[3] = 0;
}

void Player::think(double dt){
	if(isFlying())
		velo = velo * exp(-10.0 * dt);
	else
		velo += Vec3d(0,-9.8,0) * dt;

	// Initialize with false always
	floorTouched = false;

	Vec3i ipos = game.world->real2ind(pos);

//	pos += velo * dt;
	trymove(velo * dt);
	rot = Quatd::slerp(desiredRot, rot, exp(-dt * 5.));

	*game.logwriter << "Player [" << pos[0] << "," << pos[1] << "," << pos[2] << "]" << std::endl;
}

void Player::keyinput(double dt){
	bool shift = !!(GetKeyState(VK_SHIFT) >> 8);
	BYTE keys[256];

	GetKeyboardState(keys);

	// Linear movement keys
	if(GetKeyState('W') >> 8)
		trymove(dt * (shift ? runspeed : movespeed) * Vec3d(0,0,1));
	if(GetKeyState('S') >> 8)
		trymove(dt * movespeed * Vec3d(0,0,-1));
	if(GetKeyState('A') >> 8)
		trymove(dt * movespeed * Vec3d(-1,0,0));
	if(GetKeyState('D') >> 8)
		trymove(dt * movespeed * Vec3d(1,0,0));

	// You cannot jump upward without feet on ground. What about jump downward?
	if(floorTouched){
		if(GetKeyState(VK_SPACE) >> 8)
			trymove(jumpspeed * Vec3d(0,1,0), true);
	}

	if(isFlying()){
		if(GetKeyState('Q') >> 8)
			trymove(dt * movespeed * Vec3d(0,1,0));
		if(GetKeyState('Z') >> 8)
			trymove(dt * movespeed * Vec3d(0,-1,0));
	}

	// Rotation keys
	if(GetKeyState(VK_NUMPAD4) >> 8)
		py[1] += dt * rotatespeed, updateRot();
	if(GetKeyState(VK_NUMPAD6) >> 8)
		py[1] -= dt * rotatespeed, updateRot();
	if(GetKeyState(VK_NUMPAD8) >> 8)
		py[0] += dt * rotatespeed, updateRot();
	if(GetKeyState(VK_NUMPAD2) >> 8)
		py[0] -= dt * rotatespeed, updateRot();

	if(oldKeys['F'] & 0x80 && !(GetKeyState('F') >> 8))
		moveMode = moveMode == Fly ? Walk : Fly;
	if(oldKeys['C'] & 0x80 && !(GetKeyState('C') >> 8))
		moveMode = moveMode == Ghost ? Walk : Ghost;

	// Toggle curtype
	if (oldKeys['X'] & 0x80 && !(keys['X'] & 0x80))
		curtype = (Cell::Type)(curtype % 3 + 1);

	// Dig the cell forward
	if(oldKeys['T'] & 0x80 && !(keys['T'] & 0x80)){
		Vec3d dir = rot.itrans(Vec3d(0,0,1));
		for(int i = 1; i < 8; i++){
			Vec3i ci = World::real2ind(pos + dir * i / 2);
			Cell c = game.world->cell(ci[0], ci[1], ci[2]);
			if (c.isSolid() && game.world->setCell(ci[0], ci[1], ci[2], Cell(Cell::Air)))
			{
				bricks[c.getType()] += 1;
				break;
			}
		}
	}

	// Place a solid cell next to another solid cell.
	// Feasible only if the player has a brick.
	if (oldKeys['G'] & 0x80 && !(keys['G'] & 0x80) && 0 < bricks[curtype]){
		Vec3d dir = rot.itrans(Vec3d(0,0,1));
		for (int i = 1; i < 8; i++){
			Vec3i ci = World::real2ind(pos + dir * i / 2);

			if(game.world->isSolid(ci[0], ci[1], ci[2]))
				continue;

			bool buried = false;
			for(int ix = 0; ix < 2 && !buried; ix++) for(int iz = 0; iz < 2 && !buried; iz++) for(int iy = 0; iy < 2 && !buried; iy++){
				// Position to check collision with the walls
				Vec3d hitcheck(pos[0] + (ix * 2 - 1) * boundWidth, pos[1] - eyeHeight + iy * boundHeight, pos[2] + (iz * 2 - 1) * boundLength);

				if(ci == World::real2ind(hitcheck))
					buried = true;
			}
			if(buried)
				continue;

			static const Vec3i directions[] = {
				Vec3i(1,0,0),
				Vec3i(-1,0,0),
				Vec3i(0,1,0),
				Vec3i(0,-1,0),
				Vec3i(0,0,1),
				Vec3i(0,0,-1),
			};

			bool supported = false;
			for(int j = 0; j < numof(directions); j++)
				if (game.world->isSolid(ci + directions[j])){
					supported = true;
					break;
				}
			if(!supported)
				continue;

			if(game.world->setCell(ci[0], ci[1], ci[2], Cell(curtype)))
			{
				bricks[curtype] -= 1;
				break;
			}
		}
	}

	if (oldKeys['K'] & 0x80 && !(GetKeyState('K') >> 8)){
		game.save();
	}

	if (oldKeys['L'] & 0x80 && !(GetKeyState('L') >> 8)){
		game.load();
	}

	memcpy(oldKeys, keys, sizeof oldKeys);
}

/// <summary>
/// Try moving to a position designated by the coordinates pos + delta.
/// </summary>
/// <param name="delta">Vector to add to current position to designate destination.</param>
/// <param name="setvelo">Flag that tells delta really means velocity vector.</param>
/// <returns>true if the movement is feasible, otherwise false and no movement is performed.</returns>
bool Player::trymove(const Vec3d &delta, bool setvelo){
	if(setvelo){
		velo += delta;
		return true;
	}

	// Retrieve vector in world coordinates
	Vec3d worldDelta = Quatd::rotation(py[1], 0, 1, 0).itrans(delta);

	// Destination position
	Vec3d dest = pos + worldDelta;

	Vec3d worldDeltaDir = worldDelta.norm();

	if(moveMode != Ghost){
		for(int ix = 0; ix < 2; ix++) for(int iz = 0; iz < 2; iz++) for(int iy = 0; iy < 2; iy++){
			// Position to check collision with the walls
			Vec3d hitcheck(dest[0] + (ix * 2 - 1) * boundWidth, dest[1] - eyeHeight + iy * boundHeight, dest[2] + (iz * 2 - 1) * boundLength);
			if(game.world->isSolid(game.world->real2ind(hitcheck))){
				// Clear velocity component along delta direction
				double vad = velo.sp(worldDeltaDir);
				if(0 < vad){
					if(worldDeltaDir[1] < 0)
						floorTouched = true;
					velo -= vad * worldDeltaDir;
				}
				return false;
			}
		}
	}

	pos = dest;
	return true;
}

void Player::rotateLook(double dx, double dy){
	py[0] -= dy * M_PI / 2000.;
	py[1] -= dx * M_PI / 2000.;
	updateRot();
}


void Player::serialize(std::ostream &o){
	o.write((char*)&pos, sizeof pos);
	o.write((char*)&velo, sizeof velo);
	o.write((char*)&py, sizeof py);
	o.write((char*)&rot, sizeof rot);
	o.write((char*)&desiredRot, sizeof desiredRot);
	o.write((char*)&bricks, sizeof bricks);
	o.write((char*)&moveMode, sizeof moveMode);
}

void Player::unserialize(std::istream &is){
	is.read((char*)&pos, sizeof pos);
	is.read((char*)&velo, sizeof velo);
	is.read((char*)&py, sizeof py);
	is.read((char*)&rot, sizeof rot);
	is.read((char*)&desiredRot, sizeof desiredRot);
	is.read((char*)&bricks, sizeof bricks);
	is.read((char*)&moveMode, sizeof moveMode);
}

}
