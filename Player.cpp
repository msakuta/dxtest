#include "Player.h"
#include "World.h"

namespace dxtest{

void Player::think(double dt){
	velo += Vec3d(0,-9.8,0) * dt;

	Vec3i ipos = world.real2ind(pos);
	if(world.volume.isSolid(ipos)/* || world.volume.isSolid(ipos - Vec3i(0,1,0))*/){
		velo.clear();
		pos[1] = ipos[1] - CELLSIZE / 2 + 2.7;
	}
/*	else if(world.volume.isSolid(ipos + Vec3i(0,1,0))){
		player.velo.clear();
		player.pos[1] = ipos[1] - CELLSIZE / 2 + 1.7 + 1;
	}*/

	pos += velo * dt;
	rot = Quatd::slerp(desiredRot, rot, exp(-dt * 5.));
}

bool Player::trymove(const Vec3d &delta, bool setvelo){
	if(setvelo){
		velo += delta;
		return true;
	}
	Vec3d dest = pos + Quatd::rotation(py[1], 0, 1, 0).itrans(delta);
	if(!world.volume.isSolid(world.real2ind(dest + Vec3d(0,.5,0)))){
		pos = dest;
		return true;
	}
	return false;
}


}
