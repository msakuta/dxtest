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

void Player::think(double dt){
	velo += Vec3d(0,-9.8,0) * dt;

	// Initialize with false always
	floorTouched = false;

	Vec3i ipos = world.real2ind(pos);

//	pos += velo * dt;
	trymove(velo * dt);
	rot = Quatd::slerp(desiredRot, rot, exp(-dt * 5.));
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

	pos = dest;
	return true;
}


}
