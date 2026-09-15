#include "PlayMode.hpp"

#include "PixelFont.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>

//----- audio (CC0, from Kenney's "Impact Sounds" pack -- see README-impact-sounds.txt;
// swap these for self-recorded sounds later if you want) -----

// "footstep" plays on every successful step:
Load< Sound::Sample > footstep_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("footstep_concrete.wav"));
});
// "bump" plays when a step is blocked by a wall:
Load< Sound::Sample > bump_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("bump_soft.wav"));
});
// "beacon" is the exit's looping sound -- distinct timbre (a ringing bell) so it
// doesn't get confused with footsteps/bumps:
Load< Sound::Sample > beacon_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("beacon_bell.wav"));
});
// "turn" plays whenever the player rotates 90 degrees:
Load< Sound::Sample > turn_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("turn_cloth.wav"));
});

//----- maze layout (fixed, hand-picked so the start's right side is a wall and its
// south side is open -- see the ASCII art below) -----
//
// generated once by a randomized depth-first "backtracker" (seed 3) and then
// hardcoded here: a procedural generator re-run at build time isn't reproducible
// across compilers/platforms (std::shuffle's exact output isn't standardized to
// match bit-for-bit between standard library implementations), so baking in the
// result is what actually guarantees the same maze every time.
static char const * const MazeRows[PlayMode::GridTall] = {
	"#############",
	"#S#.....#...#",
	"#.###.#.###.#",
	"#.....#...#.#",
	"#########.#.#",
	"#.......#...#",
	"#.#####.###.#",
	"#.#.......#.#",
	"#.#########.#",
	"#.#.......#.#",
	"#.#.#####.#.#",
	"#.......#..E#",
	"#############",
};

static std::vector< bool > generate_maze() {
	constexpr int32_t GridWide = PlayMode::GridWide;
	constexpr int32_t GridTall = PlayMode::GridTall;

	std::vector< bool > solid(GridWide * GridTall, true);
	for (int32_t row = 0; row < GridTall; ++row) {
		for (int32_t col = 0; col < GridWide; ++col) {
			solid[row * GridWide + col] = (MazeRows[row][col] == '#');
		}
	}
	return solid;
}

//----- coordinate helpers -----

static inline glm::vec3 grid_to_world(int32_t row, int32_t col, float y) {
	return glm::vec3(col * PlayMode::CellSize, y, row * PlayMode::CellSize);
}

bool PlayMode::is_solid(int32_t row, int32_t col) const {
	if (row < 0 || row >= GridTall || col < 0 || col >= GridWide) return true; //treat out-of-bounds as walls
	return maze_solid[row * GridWide + col];
}

//----- facing -----

//player_cell is (col, row); these are the (dcol, drow) steps for each facing:
// 0 = north, 1 = east, 2 = south, 3 = west (matches a -90-degree-per-step yaw below)
static std::array< glm::ivec2, 4 > const FacingDelta = {{
	glm::ivec2( 0,-1), //north
	glm::ivec2( 1, 0), //east
	glm::ivec2( 0, 1), //south
	glm::ivec2(-1, 0), //west
}};

void PlayMode::update_camera_transform() {
	camera->transform->position = grid_to_world(player_cell.y, player_cell.x, EyeHeight);
	camera->transform->rotation = glm::angleAxis(glm::radians(-90.0f * float(facing)), glm::vec3(0.0f, 1.0f, 0.0f));
}

//----- stepping -----

void PlayMode::try_step(glm::ivec2 const &delta) {
	glm::ivec2 target = player_cell + delta;
	if (is_solid(target.y, target.x)) {
		Sound::play(*bump_sample, 0.6f);
	} else {
		player_cell = target;
		Sound::play(*footstep_sample, 0.5f);
	}
}

//----- PlayMode -----

PlayMode::PlayMode() {
	maze_solid = generate_maze();

	start_cell = glm::ivec2(1, 1);
	exit_cell = glm::ivec2(GridWide - 2, GridTall - 2);
	player_cell = start_cell;
	facing = 1; //face east, into the maze

	//set up a bare camera (no transform hierarchy / meshes needed -- the screen stays black):
	Scene::Transform *camera_transform = &scene.transforms.emplace_back();
	scene.cameras.emplace_back(camera_transform);
	camera = &scene.cameras.back();
	camera->fovy = glm::radians(60.0f);
	camera->near = 0.01f;
	update_camera_transform();

	//exit beacon: quiet, long-range loop that gets louder as you approach:
	exit_beacon_loop = Sound::loop_3D(*beacon_sample, 0.4f, grid_to_world(exit_cell.y, exit_cell.x, EyeHeight), 6.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_A) {
			turn_left.downs += 1;
			return true;
		} else if (evt.key.key == SDLK_D) {
			turn_right.downs += 1;
			return true;
		} else if (evt.key.key == SDLK_W) {
			move_forward.downs += 1;
			return true;
		} else if (evt.key.key == SDLK_S) {
			move_back.downs += 1;
			return true;
		} else if (evt.key.key == SDLK_R) {
			restart_button.downs += 1;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//every action below is a single discrete step, not a continuous motion,
	// so 'elapsed' isn't needed here:

	if (restart_button.downs > 0) {
		player_cell = start_cell;
		facing = 1;
		won = false;
	}

	if (!won) {
		if (turn_left.downs > 0) {
			facing = (facing + 3) % 4; //rotate 90 degrees counter-clockwise
			Sound::play(*turn_sample, 0.5f);
		}
		if (turn_right.downs > 0) {
			facing = (facing + 1) % 4; //rotate 90 degrees clockwise
			Sound::play(*turn_sample, 0.5f);
		}

		if (move_forward.downs > 0) try_step(FacingDelta[facing]);
		if (move_back.downs > 0) try_step(-FacingDelta[facing]);

		if (player_cell == exit_cell) won = true;
	}

	update_camera_transform();

	{ //update listener to camera position/orientation:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	turn_left.downs = 0;
	turn_right.downs = 0;
	move_forward.downs = 0;
	move_back.downs = 0;
	restart_button.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//pure black -- this is a blackout maze, there is nothing to see:
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	scene.draw(*camera); //no drawables yet, so this is a no-op

	{ //use PixelText to overlay some text (pixel-font.png, reused from game2/game1's own asset pipeline):
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		glm::mat4 clip_from_local(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		//NOTE: pixel-font.png only has uppercase letters, digits, and " /-!:." --
		// keep HUD strings within that character set:
		constexpr float H = 0.09f;
		{
			PixelText text(clip_from_local);
			text.draw("A/D TURN. W/S MOVE. R RESTART.",
				glm::vec3(-aspect + 0.1f * H, -1.0f + 1.1f * H, 0.0f), H);
		}

		{ //current facing, as 1/2/3/4 (north/east/south/west):
			PixelText text(clip_from_local);
			text.draw("DIRECTION: " + std::to_string(facing + 1),
				glm::vec3(-aspect + 0.1f * H, 1.0f - 1.1f * H, 0.0f), H);
		}

		if (won) {
			PixelText text(clip_from_local);
			text.draw("YOU FOUND THE EXIT! PRESS R TO RESTART.",
				glm::vec3(-aspect + 0.1f * H, 1.0f - 2.4f * H, 0.0f), H);
		}
	}
	GL_ERRORS();
}
