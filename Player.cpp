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
const double Player::jumpspeed = 4.; ///< Speed set vertically when jumping [meters per second]
const double Player::rotatespeed = acos(0.) / 1.; ///< pi / 2, means it takes 4 seconds to look all the way around.
const double Player::gravity = 9.8;
const double Player::swimUpAccel = 5.;


Player::Player(Game &game) : game(game), pos(0, CELLSIZE, 0), velo(0,0,0), rot(0,0,0,1), desiredRot(0,0,0,1), showMiniMap(false){
	py[0] = py[1] = 0.;
	game.player = this;
	curtype = Cell::Grass;
	bricks[0] = bricks[1] = bricks[2] = bricks[3] = 0;
}

void Player::think(double dt){
	if(isFlying())
		velo = velo * exp(-10.0 * dt);
	else{
		velo += Vec3d(0,-gravity,0) * dt;
		if(game.world->cell(game.world->real2ind(pos)).getType() == Cell::Water)
			velo *= exp(-5. * dt);
	}

	// Initialize with false always
	floorTouched = false;

	Vec3i ipos = game.world->real2ind(pos);

//	pos += velo * dt;

	// Check if the Player's feet are under ground, and if they do, gradually climb up to prevent stucking.
	if(moveMode == Walk) for(int ix = 0; ix < 2; ix++) for(int iz = 0; iz < 2; iz++){
		Vec3d hitcheck(pos[0] + (ix * 2 - 1) * boundWidth, pos[1] - eyeHeight, pos[2] + (iz * 2 - 1) * boundLength);
		if(game.world->isSolid(hitcheck)){
			trymove(Vec3d(0, 2. * dt, 0), false, true);
			ix = 2; // Exit outer iteration
			break;
		}
	}

	trymove(velo * dt, false, false);
	rot = Quatd::slerp(desiredRot, rot, exp(-dt * 5.));

	*game.logwriter << "Player [" << pos[0] << "," << pos[1] << "," << pos[2] << "]" << std::endl;
}

void Player::keyinput(double dt){
	bool inWater = game.world->cell(game.world->real2ind(pos)).getType() == Cell::Water;
	double movespeed = this->movespeed / (1. + inWater);
	bool shift = !!(GetKeyState(VK_SHIFT) >> 8);
	BYTE keys[256];

	GetKeyboardState(keys);

	if(inWater && !isFlying()){
		// Linear movement keys
		if(GetKeyState('W') >> 8)
			velo[2] += dt * swimUpAccel;
		if(GetKeyState('S') >> 8)
			velo[2] += -dt * swimUpAccel;
		if(GetKeyState('A') >> 8)
			velo[0] += -dt * swimUpAccel;
		if(GetKeyState('D') >> 8)
			velo[0] += dt * swimUpAccel;

		if(GetKeyState(VK_SPACE) >> 8)
			velo[1] += dt * (swimUpAccel + gravity);

	}
	else{
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
	if (oldKeys['X'] & 0x80 && !(keys['X'] & 0x80)){
		// Skip invalid type (HalfAir)
		do{
			curtype = (Cell::Type)(curtype % (Cell::NumTypes - 1) + 1);
			const unsigned map = 1 << Cell::Grass | 1 << Cell::Dirt | 1 << Cell::Gravel | 1 << Cell::Rock;
			unsigned bit = 1 << (curtype & ~Cell::HalfBit);
			if(map & bit)
				break;
		} while(true);
	}

	// Dig the cell forward
	if(oldKeys['T'] & 0x80 && !(keys['T'] & 0x80)){
		Vec3d dir = rot.itrans(Vec3d(0,0,1));
		for(int i = 1; i < 8; i++){
			Vec3i ci = World::real2ind(pos + dir * i / 2);
			Cell c = game.world->cell(ci[0], ci[1], ci[2]);
			if (c.isSolid() && game.world->setCell(ci[0], ci[1], ci[2], Cell(Cell::Air)))
			{
				addBricks(c.getType(), 1);
				break;
			}
		}
	}

	// Place a solid cell next to another solid cell.
	// Feasible only if the player has a brick.
	if (oldKeys['G'] & 0x80 && !(keys['G'] & 0x80) && 0 < getBricks(curtype)){
		Vec3d dir = rot.itrans(Vec3d(0,0,1));
		for (int i = 1; i < 8; i++){
			Vec3i ci = World::real2ind(pos + dir * i / 2);

			if(game.world->isSolid(ci[0], ci[1], ci[2]))
				continue;

			// Limit half cells to be on top of full-height cell.
			if(curtype & Cell::HalfBit && game.world->cell(ci + Vec3i(0, -1, 0)).isTranslucent())
				continue;

			// If this placing makes the Player to be stuck in a brick, do not allow it.
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
				addBricks(curtype, -1);
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

	if(oldKeys['M'] & 0x80 && !(GetKeyState('M') >> 8))
		showMiniMap = !showMiniMap;

	memcpy(oldKeys, keys, sizeof oldKeys);
}

/// <summary>
/// Try moving to a position designated by the coordinates pos + delta.
/// </summary>
/// <param name="delta">Vector to add to current position to designate destination.</param>
/// <param name="setvelo">Flag that tells delta really means velocity vector.</param>
/// <param name="ignorefeet">Flag that tells the feet should not be checked for collision detection.</param>
/// <returns>true if the movement is feasible, otherwise false and no movement is performed.</returns>
bool Player::trymove(const Vec3d &delta, bool setvelo, bool ignorefeet){
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
		for(int ix = 0; ix < 2; ix++) for(int iz = 0; iz < 2; iz++) for(int iy = ignorefeet; iy < 2; iy++){
			// Position to check collision with the walls
			Vec3d hitcheck(dest[0] + (ix * 2 - 1) * boundWidth, dest[1] - eyeHeight + iy * boundHeight, dest[2] + (iz * 2 - 1) * boundLength);
			if(game.world->isSolid(hitcheck)){

				// If the player is goning into solid cell laterally, allow shallow sinking into floor, because that stuck state will be resolved automatically.
				if(iy == 0 && worldDelta[1] == 0.){
					double bh = game.world->boundaryHeight(hitcheck);
					if(bh <= 0.5 && !game.world->isSolid(hitcheck + Vec3d(0, bh, 0))){
						velo[1] = 0.;
//						stepped = true;
						floorTouched = true;
						continue;
					}
				}

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


void Player::addBricks(Cell::Type t, int count){
	if(t & Cell::HalfBit)
		bricks[t & ~Cell::HalfBit] += count;
	else
		bricks[t] += count * 2;
}

int Player::getBricks(Cell::Type t){
	if(t & Cell::HalfBit)
		return bricks[t & ~Cell::HalfBit];
	else
		return bricks[t] / 2;
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
