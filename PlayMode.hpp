#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//discrete input tracking: every key press advances the game by exactly one
	// action (a 90-degree turn or one grid step) -- there is no continuous/analog
	// movement or mouse-look in this game:
	struct Button {
		uint8_t downs = 0;
	} turn_left, turn_right, move_forward, move_back, restart_button;

	//scene is empty except for the camera (this is a blackout maze -- there is
	// nothing to draw, only to hear):
	Scene scene;
	Scene::Camera *camera = nullptr;

	//----- maze -----

	//maze is a grid of solid/open cells (a perfect maze: odd-index cells between
	// two "room" cells are walls unless carved as a passage):
	static constexpr int32_t RoomsWide = 6;
	static constexpr int32_t RoomsTall = 6;
	static constexpr int32_t GridWide = RoomsWide * 2 + 1;
	static constexpr int32_t GridTall = RoomsTall * 2 + 1;
	static constexpr float CellSize = 2.0f;
	static constexpr float EyeHeight = 1.0f;

	std::vector< bool > maze_solid; //size GridWide * GridTall; true == wall
	bool is_solid(int32_t row, int32_t col) const;

	glm::ivec2 start_cell = glm::ivec2(1, 1); //(col, row)
	glm::ivec2 exit_cell = glm::ivec2(1, 1); //(col, row)

	//player is grid-locked: a cell plus one of 4 facings (0=north,1=east,2=south,3=west):
	glm::ivec2 player_cell = glm::ivec2(1, 1);
	int32_t facing = 0;

	void update_camera_transform(); //sync camera position/rotation to player_cell/facing

	//attempt to step to player_cell + delta: plays a footstep sound and moves if
	// that cell is open, or plays a bump sound and stays put if it's a wall:
	void try_step(glm::ivec2 const &delta);

	//----- win state -----

	bool won = false;

	//looping beacon sound marking the exit:
	std::shared_ptr< Sound::PlayingSample > exit_beacon_loop;
};
