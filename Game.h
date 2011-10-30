#ifndef DXTEST_GAME_H
#define DXTEST_GAME_H

namespace dxtest{

class Player;
class World;

class Game{
public:
	Player *player;
	World *world;
	static const int maxViewDistance;

	void draw()const;
};

}

#endif
