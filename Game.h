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
	static const unsigned char saveFileSignature[];
	static const int saveFileVersion;

	void draw(double dt)const;

	bool save();
	bool load();
	void serialize(std::ostream &o);
	void unserialize(std::istream &i);
};

}

#endif
