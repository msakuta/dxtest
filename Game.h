#ifndef DXTEST_GAME_H
#define DXTEST_GAME_H
#include <fstream>

namespace dxtest{

class Player;
class World;

class Game{
public:
	Player *player;
	World *world;
	std::ostream *logwriter;
	static const int maxViewDistance;

	void draw()const;
};

}

#endif
