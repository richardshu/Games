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
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;

#define FIXED_TIMESTEP 0.0166666f	// 60 FPS (1.0f/60.0f) (update sixty times a second)
#define MAX_TIMESTEPS 6
#define TILE_SIZE 0.07f
#define SPRITE_COUNT_X 16
#define SPRITE_COUNT_Y 8
#define FRICTION 2.0f

SDL_Window* displayWindow;
SDL_GLContext context;
ShaderProgram program;
ShaderProgram texturedProgram;  // For textured polygons
const Uint8 *keys;
glm::mat4 projectionMatrix, viewMatrix;

enum GameMode { MAIN_MENU, GAME_LEVEL, GAME_OVER };
enum Direction { LEFT, RIGHT, UP, DOWN };
enum EntityType { PLAYER, ENEMY, BULLET };
GLuint asciiSpriteSheetTexture;
GLuint textureSheet;
bool done = false;              // Game loop
float lastFrameTicks = 0.0f;    // Set time to an initial value of 0
float accumulator = 0.0f;

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

class SheetSprite {
public:
	SheetSprite() {};
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size);
	void Draw(ShaderProgram &program);

	float u;
	float v;
	float width;
	float height;
	float size;
	unsigned int textureID;
};

SheetSprite::SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) {
	this->textureID = textureID;
	this->u = u;
	this->v = v;
	this->width = width;
	this->height = height;
	this->size = size;
}

void SheetSprite::Draw(ShaderProgram &program) {
	glBindTexture(GL_TEXTURE_2D, textureID);

	float aspectRatio = width / height;
	float vertices[] = {
		-0.5f * size * aspectRatio, -0.5f * size,
		 0.5f * size * aspectRatio,  0.5f * size,
		-0.5f * size * aspectRatio,  0.5f * size,
		 0.5f * size * aspectRatio,  0.5f * size,
		-0.5f * size * aspectRatio, -0.5f * size,
		 0.5f * size * aspectRatio, -0.5f * size
	};
	float texCoords[] = {
		u, v + height,
		u + width, v,
		u, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};

	glUseProgram(program.programID);

	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

class Entity {
public:
	void Update(float elapsed);
	void Render(ShaderProgram &program);
	bool CollidesWith(Entity &otherEntity);

	SheetSprite sprite;
	Direction direction;
	EntityType entityType;

	glm::vec3 position;
	glm::vec3 size;
	glm::vec3 velocity;
	glm::vec3 acceleration = glm::vec3(0.0f, 0.0f, 0.0f);

	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;

private:
	void Entity::ResolveCollisionX(Entity &otherEntity);
	void Entity::ResolveCollisionY(Entity &otherEntity);
};

void Entity::Update(float elapsed) {
	this->position.x += this->velocity.x * elapsed;
	this->position.y += this->velocity.y * elapsed;
}

void Entity::Render(ShaderProgram &program) {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, position);
	modelMatrix = glm::scale(modelMatrix, size);
	program.SetModelMatrix(modelMatrix);
	
	// Delegate the creation of vertex and texture 
	// coordinates to the sprite's Draw() method
	sprite.Draw(program);
}

bool Entity::CollidesWith(Entity &otherEntity) {
	// There is no collision
	if (position.x + sprite.width / 2 < otherEntity.position.x - otherEntity.sprite.width / 2 || 
		position.x - sprite.width / 2 > otherEntity.position.x + otherEntity.sprite.width / 2 || 
		position.y + sprite.width / 2 < otherEntity.position.y - otherEntity.sprite.width / 2 || 
		position.y - sprite.width / 2 > otherEntity.position.y + otherEntity.sprite.width / 2) {
		return false;
	}
	/*if (otherEntity.entityType == ENEMY) {
		otherEntity.position = glm::vec3(100.0f, 100.0f, 0.0f);
	}
	else {
		ResolveCollisionX(otherEntity);
		ResolveCollisionY(otherEntity);
	}*/
	return true;
}

void Entity::ResolveCollisionX(Entity& otherEntity) {
	float penetration = fabs(fabs(position.x - otherEntity.position.x) - sprite.width/2 - otherEntity.sprite.width/2);

	// A right collision occurred
	if (position.x < otherEntity.position.x) {
		position.x -= penetration - 0.00001f;
		collidedRight = true;
	}

	// A left collision occurred
	else {
		position.x += penetration + 0.00001f;
		collidedLeft = true;
	}
	velocity.x = 0.0f; // Reset
}

void Entity::ResolveCollisionY(Entity& otherEntity) {
	float penetration = fabs(fabs(position.y - otherEntity.position.y) - sprite.height / 2 - otherEntity.sprite.height / 2);

	// A top collision occurred
	if (position.y < otherEntity.position.y) {
		position.y -= penetration + 0.00001f;
		collidedBottom = true;
	}

	// A bottom collision occurred
	else {
		position.y += penetration + 0.00001f;
		collidedTop = true;
	}
	velocity.y = 0.0f; // Resetz
}

struct MainMenuState {
	void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing);

	void Setup();
	void ProcessEvents();
	void Render();
};

struct GameState {
	Entity player1;
	Entity player2;
	vector<Entity> bullets;
	vector<Entity> enemies;

	void Setup();
	void ProcessEvents();
	void Update(float elapsed);
	void Render();
};

GameMode mode;
GameState gameState;
MainMenuState mainMenuState;

void MainMenuState::DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing) {
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
			((size + spacing) * i) + (0.5f * size),  0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size),  0.5f * size,
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
}

void MainMenuState::Setup() {

}

void GameState::Setup() {
	SheetSprite playerBlueSprite = SheetSprite(textureSheet, 211.0f / 1024.0f, 941.0f / 1024.0f, 99.0f / 1024.0f, 75.0f / 1024.0f, 0.2f);
	SheetSprite playerGreenSprite = SheetSprite(textureSheet, 237.0f / 1024.0f, 377.0f / 1024.0f, 99.0f / 1024.0f, 75.0f / 1024.0f, 0.2f);

	this->player1.sprite = playerBlueSprite;
	this->player1.direction = DOWN;
	this->player1.entityType = PLAYER;
	this->player1.position = glm::vec3(-0.2f, 0.0f, 0.0f);
	this->player1.size = glm::vec3(1.0f, 1.0f, 1.0f);
	this->player1.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	this->player2.sprite = playerGreenSprite;
	this->player2.direction = DOWN;
	this->player2.entityType = PLAYER;
	this->player2.position = glm::vec3(0.2f, 0.0f, 0.0f);
	this->player2.size = glm::vec3(1.0f, 1.0f, 1.0f);
	this->player2.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Final Project", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 640, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 640);

	// Load shader program
	program.Load("vertex.glsl", "fragment.glsl");
	texturedProgram.Load("vertex_textured.glsl", "fragment_textured.glsl");

	// Load sprite sheets
	asciiSpriteSheetTexture = LoadTexture("assets/ascii_spritesheet.png");
	textureSheet = LoadTexture("assets/SpaceShooter/Spritesheet/sheet.png");

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// "Clamp" down on a texture so that the pixels on the edge repeat
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Set background color to sky blue
	glClearColor(0.6f, 0.9f, 1.0f, 1.0f);

	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	viewMatrix = glm::mat4(1.0f);

	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);

	texturedProgram.SetProjectionMatrix(projectionMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);

	glUseProgram(texturedProgram.programID);

	keys = SDL_GetKeyboardState(NULL);

	mode = MAIN_MENU; // Render the menu when the user opens the game
	mainMenuState.Setup();
}

void MainMenuState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		mode = GAME_LEVEL;
		gameState.Setup();
	}
}

void GameState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}

	// Player One movement
	this->player1.velocity.x = 0.0f;
	this->player1.velocity.y = 0.0f;
	if (keys[SDL_SCANCODE_LEFT]) {
		this->player1.velocity.x = -1.0f;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		this->player1.velocity.x = 1.0f;
	}
	if (keys[SDL_SCANCODE_UP]) {
		this->player1.velocity.y = 1.0f;
	}
	if (keys[SDL_SCANCODE_DOWN]) {
		this->player1.velocity.y = -1.0f;
	}

	// Player Two movement
	this->player2.velocity.x = 0.0f;
	this->player2.velocity.y = 0.0f;
	if (keys[SDL_SCANCODE_A]) {
		this->player2.velocity.x = -1.0f;
	}
	if (keys[SDL_SCANCODE_D]) {
		this->player2.velocity.x = 1.0f;
	}
	if (keys[SDL_SCANCODE_W]) {
		this->player2.velocity.y = 1.0f;
	}
	if (keys[SDL_SCANCODE_S]) {
		this->player2.velocity.y = -1.0f;
	}
}

void ProcessEvents() {
	switch (mode) {
	case MAIN_MENU:
		mainMenuState.ProcessEvents();
		break;
	case GAME_LEVEL:
		gameState.ProcessEvents();
		break;
	}
}

void GameState::Update(float elapsed) {
	this->player1.Update(elapsed);
	this->player2.Update(elapsed);

	for (size_t i = 0; i < this->enemies.size(); i++) {
		this->enemies[i].Update(elapsed);
	}
	for (size_t i = 0; i < this->bullets.size(); i++) {
		this->bullets[i].Update(elapsed);
	}
}

void Update() {
	float ticks = (float) SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFrameTicks;
	lastFrameTicks = ticks;

	switch (mode) {
	case GAME_LEVEL:
		gameState.Update(elapsed);
		break;
	}
}

void MainMenuState::Render() {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.625f, 0.25f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	this->DrawText(texturedProgram, asciiSpriteSheetTexture, "Final Project", 0.3f, -0.16f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.1f, -0.1f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	this->DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 0.125f, -0.075f);
}

void GameState::Render() {
	this->player1.Render(texturedProgram);
	this->player2.Render(texturedProgram);
	for (size_t i = 0; i < this->enemies.size(); i++) {
		this->enemies[i].Render(texturedProgram);
	}
	for (size_t i = 0; i < this->bullets.size(); i++) {
		this->bullets[i].Render(texturedProgram);
	}
}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);
	switch (mode) {
	case MAIN_MENU:
		mainMenuState.Render();
		break;
	case GAME_LEVEL:
		gameState.Render();
		break;
	}
	SDL_GL_SwapWindow(displayWindow);
}

void Cleanup() {
	
}

int main(int argc, char *argv[]) {
	Setup();
	while (!done) {
		ProcessEvents();
		Update();
		Render();

		//// Calculate elapsed time
		//float ticks = (float)SDL_GetTicks() / 1000.0f;
		//float elapsed = ticks - lastFrameTicks;
		//lastFrameTicks = ticks; // Reset

		//// Use fixed timestep (instead of variable timestep)
		//elapsed += accumulator;
		//if (elapsed < FIXED_TIMESTEP) {
		//	accumulator = elapsed;
		//	continue;
		//}
		//while (elapsed >= FIXED_TIMESTEP) {
		//	Update(FIXED_TIMESTEP);
		//	elapsed -= FIXED_TIMESTEP;
		//}
		//accumulator = elapsed;
    }
	Cleanup();
	SDL_Quit();
    return 0;
}
