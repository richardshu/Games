#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_image.h>

#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Load images in any format with STB_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#include <vector>

SDL_Window* displayWindow;
ShaderProgram program;			// For untextured polygons
ShaderProgram texturedProgram;  // For textured polygons

// Constants
size_t MAX_NUM_LASERS  = 15;
size_t MAX_NUM_METEORS = 30;
size_t NUM_ROWS = 3;
size_t NUM_METEORS_PER_ROW = MAX_NUM_METEORS / NUM_ROWS;
float SPACE_BETWEEN_METEORS_X = 2 * 1.5f / NUM_METEORS_PER_ROW;
float SPACE_BETWEEN_METEORS_Y = 0.3f;

bool done = false;				// Game loop
float lastFrameTicks = 0.0f;	// Set time to an initial value of 0
int numMeteorsLeft = MAX_NUM_METEORS;

GLuint asciiSpriteSheetTexture;
GLuint spaceSpriteSheetTexture;

class SheetSprite {
public:
	SheetSprite();
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size);
	void Draw(ShaderProgram &program);

	float u;
	float v;
	float width;
	float height;
	float size;
	unsigned int textureID;
};

SheetSprite::SheetSprite() {}

SheetSprite::SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) {
	this->textureID = textureID;
	this->u = u;
	this->v = v;
	this->width = width;
	this->height = height;
	this->size = size;
}

void SheetSprite::Draw(ShaderProgram &program) {
	program.SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	float aspectRatio = width / height;
	float vertices[] = { 
		-0.5f * size * aspectRatio, -0.5f * size,
		-0.5f * size * aspectRatio,  0.5f * size, 
		 0.5f * size * aspectRatio, -0.5f * size,
		 0.5f * size * aspectRatio, -0.5f * size,
		-0.5f * size * aspectRatio,  0.5f * size,
		 0.5f * size * aspectRatio,  0.5f * size
	};
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	// UV coords are upside down relative to vertices
	// (everything is rotated 180 degrees around the origin)
	float texCoords[] = {
		u + width, v + height,
		u + width, v,
		u, v + height,
		u, v + height,
		u + width, v,
		u, v
	};
	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program.texCoordAttribute);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

class Entity {
public:
	void Update(float elapsed);
	void Draw(ShaderProgram &program);

	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 size;

	float rotation;
	SheetSprite sprite;
};

void Entity::Update(float elapsed) {
	position.x += elapsed * velocity.x;
	position.y += elapsed * velocity.y;
}

void Entity::Draw(ShaderProgram &program) {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, position);
	modelMatrix = glm::scale(modelMatrix, size);
	program.SetModelMatrix(modelMatrix);
	
	// Designate the creation of vertex and texture 
	// coordinates to the sprite's Draw() method
	sprite.Draw(program);
}

void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing) {
	float character_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	for (size_t i = 0; i < text.size(); i++) {
		int spriteIndex = (int)text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size),  0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) +  (0.5f * size),  0.5f * size,
			((size + spacing) * i) +  (0.5f * size), -0.5f * size,
			((size + spacing) * i) +  (0.5f * size),  0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x + character_size, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x, texture_y + character_size,
		});
	}
	glBindTexture(GL_TEXTURE_2D, fontTexture);

	// draw this data (use the .data() method of std::vector to get pointer to data)
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	// draw this yourself, use text.size() * 6 or vertexData.size()/2 to get number of vertices
	glDrawArrays(GL_TRIANGLES, 0, 6 * (int)text.size());
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
;}

GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}
	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);
	return retTexture;
}

enum GameMode { MAIN_MENU, GAME_LEVEL };

struct GameState {
	Entity player;
	std::vector<Entity> lasers;
	std::vector<Entity> meteors;
	size_t currentLaserIndex = 0;
};

GameState state;
GameMode mode;

void shootLaser() {
	state.lasers[state.currentLaserIndex].position = glm::vec3(state.player.position.x, state.player.position.y + 2 * state.player.sprite.height, 0.0f);
	state.currentLaserIndex++;
	state.currentLaserIndex = state.currentLaserIndex % MAX_NUM_LASERS;
}

bool Collides(Entity &entity1, Entity &entity2) {
	float distanceX = abs(entity1.position.x - entity2.position.x) - (entity1.sprite.width + entity2.sprite.width);
	float distanceY = abs(entity1.position.y - entity2.position.y) - (entity1.sprite.height + entity2.sprite.height);
	return distanceX < 0 && distanceY < 0;
}

bool meteorsCollideWithSide() {
	for (size_t i = 0; i < state.meteors.size(); i++) {
		Entity meteor = state.meteors[i];
		if ((meteor.position.x + meteor.sprite.width > 1.777f && meteor.position.x + meteor.sprite.width < 100) || meteor.position.x - meteor.sprite.width < -1.777f) {
			return true;
		}
	}
	return false;
}

void SetupMainMenu() {}

void SetupGameLevel() {
	// Load sprites from sprite sheets
	SheetSprite playerSprite = SheetSprite(spaceSpriteSheetTexture, 112.0f / 1024.0f, 866.0f / 1024.0f, 112.0f / 1024.0f, 75.0f / 1024.0f, 0.2f);
	SheetSprite meteorSprite = SheetSprite(spaceSpriteSheetTexture, 327.0f / 1024.0f, 452.0f / 1024.0f, 98.0f / 1024.0f, 96.0f / 1024.0f, 0.25f);
	SheetSprite laserSprite = SheetSprite(spaceSpriteSheetTexture, 845.0f / 1024.0f, 0.0f / 13.0f, 13.0f / 1024.0f, 57.0f / 1024.0f, 0.2f);

	// Initialize player spaceship
	state.player.sprite = playerSprite;
	state.player.position = glm::vec3(0.0f, -0.75f, 0.0f);
	state.player.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	state.player.size = glm::vec3(1.0f, 1.0f, 1.0f);
	
	// Initialize meteors
	for (int row = 0; row < NUM_ROWS; row++) {
		for (int col = 0; col < NUM_METEORS_PER_ROW; col++) {
			Entity meteor;
			meteor.sprite = meteorSprite;
			meteor.velocity = glm::vec3(0.25f, 0.0f, 0.0f); // Have the meteors go right by default
			meteor.size = glm::vec3(1.0f, 1.0f, 1.0f);
			meteor.position = glm::vec3((col + 1) * SPACE_BETWEEN_METEORS_X - 1.777f, row * SPACE_BETWEEN_METEORS_Y + 0.2f, 0.0f);
			state.meteors.push_back(meteor);
		}
	}

	// Initialize lasers
	for (int i = 0; i < MAX_NUM_LASERS; i++) {
		Entity laser;
		laser.sprite = laserSprite;
		laser.velocity = glm::vec3(0.0f, 1.0f, 0.0f);
		laser.size = glm::vec3(1.0f, 1.0f, 1.0f);
		state.lasers.push_back(laser);
	}
}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Space Invaders by Richard Shu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 360);

	// Load shader programs
	//program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	// Load sprite sheets
	asciiSpriteSheetTexture = LoadTexture(RESOURCE_FOLDER"ascii_spritesheet.png");
	spaceSpriteSheetTexture = LoadTexture(RESOURCE_FOLDER"space_spritesheet.png");

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// "Clamp" down on a texture so that the pixels on the edge repeat
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glm::mat4 viewMatrix = glm::mat4(1.0f);
	glm::mat4 projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

	program.SetViewMatrix(viewMatrix);
	program.SetProjectionMatrix(projectionMatrix);

	texturedProgram.SetViewMatrix(viewMatrix);
	texturedProgram.SetProjectionMatrix(projectionMatrix);

	glUseProgram(texturedProgram.programID);

	mode = MAIN_MENU; // Render the menu when the user opens the game
	SetupMainMenu();
}

void ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}

	// Allow the player to move the spaceship left and right
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	if (mode == MAIN_MENU) {
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			mode = GAME_LEVEL;
			SetupGameLevel();
		}
	}
	else if (mode == GAME_LEVEL) {
		if (keys[SDL_SCANCODE_LEFT]) {
			state.player.velocity.x = -1.0f;
		}
		else if (keys[SDL_SCANCODE_RIGHT]) {
			state.player.velocity.x = 1.0f;
		}
		else {
			state.player.velocity.x = 0.0f;
		}

		// Make shooting lasers an input event (rather than a polling event) since it doesn't require continuous checking
		if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
			shootLaser();
		}
	}
}

void Update() {
	// Calculate elapsed time
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFrameTicks;
	lastFrameTicks = ticks; // Reset

	state.player.Update(elapsed);

	for (size_t i = 0; i < state.lasers.size(); i++) {
		state.lasers[i].Update(elapsed);
		
		// Check for collisions between lasers and meteors
		for (size_t j = 0; j < state.meteors.size(); j++) {
			if (Collides(state.lasers[i], state.meteors[j])) {
				state.lasers[i].position.x = 100.0f;
				state.meteors[j].position.x = 150.0f;
				numMeteorsLeft--;
			}
		}
	}

	for (size_t i = 0; i < state.meteors.size(); i++) {
		state.meteors[i].Update(elapsed);

		// Check for collisions between meteors and player
		if (Collides(state.meteors[i], state.player) || numMeteorsLeft == 0) {
			mode = MAIN_MENU;
			SetupMainMenu();
		}
	}

	if (meteorsCollideWithSide()) {
		for (size_t i = 0; i < state.meteors.size(); i++) {
			state.meteors[i].position.y -= SPACE_BETWEEN_METEORS_Y / 3;
			state.meteors[i].velocity.x = -state.meteors[i].velocity.x;
			state.meteors[i].Update(elapsed);
		}
	}
}

void RenderMainMenu() {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.8f, 0.25f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Space Invaders", 0.3f, -0.175f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.1f, -0.1f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 0.125f, -0.075f);
}

void RenderGameLevel() {
	state.player.Draw(texturedProgram);
	
	// Loop through entities and call their draw methods
	for (size_t i = 0; i < state.meteors.size(); i++) {
		state.meteors[i].Draw(texturedProgram);
	}
	for (size_t i = 0; i < state.lasers.size(); i++) {
		state.lasers[i].Draw(texturedProgram);
	}
}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);
	switch (mode) {
	case MAIN_MENU:
		RenderMainMenu();
		break;
	case GAME_LEVEL:
		RenderGameLevel();
		break;
	}
	SDL_GL_SwapWindow(displayWindow);
}

void Cleanup() {}

int main(int argc, char *argv[])
{
	Setup();
	while (!done) {
		ProcessEvents();
		Update();
		Render();
    }
	Cleanup();
    SDL_Quit();
    return 0;
}
