#include "Player.h"
#include "World.h"
/** \file
 * \brief Implements Player class
 */

namespace dxtest{

const double Player::boundWidth = 0.4;
const double Player::boundLength = 0.4;
const double Player::boundHeight = 1.7;
const double Player::eyeHeight = 1.5;
const double Player::movespeed = 2.; ///< Walk speed [meters per second]
const double Player::jumpspeed = 5.; ///< Speed set vertically when jumping [meters per second]
const double Player::rotatespeed = acos(0.) / 1.; ///< pi / 2, means it takes 4 seconds to look all the way around.

void Player::think(double dt){
	if(isFlying())
		velo = velo * exp(-10.0 * dt);
	else
		velo += Vec3d(0,-9.8,0) * dt;

	// Initialize with false always
	floorTouched = false;

	Vec3i ipos = world.real2ind(pos);

//	pos += velo * dt;
	trymove(velo * dt);
	rot = Quatd::slerp(desiredRot, rot, exp(-dt * 5.));
}

void Player::keyinput(double dt){
	// Linear movement keys
	if(GetKeyState('W') >> 8)
		trymove(dt * movespeed * Vec3d(0,0,1));
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

	GetKeyboardState(oldKeys);
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
		for(int ix = 0; ix < 2; ix++) for(int iz = 0; iz < 2; iz++){
			// Position to check collision with the walls
			Vec3d hitcheck(dest[0] + (ix * 2 - 1) * boundWidth, dest[1] - eyeHeight, dest[2] + (iz * 2 - 1) * boundLength);
			if(world.volume.isSolid(world.real2ind(hitcheck))){
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


}