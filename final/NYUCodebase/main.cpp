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
GLuint bettySpriteSheet, georgeSpriteSheet;
GLuint enemySpaceshipSpriteSheet;
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
	Direction faceDirection;
	Direction moveDirection;
	float moveCounter;
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
	GLuint backgroundTexture;
	void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing);

	void Setup();
	void ProcessEvents();
	void Render();
};

struct GameState {
	GLuint backgroundTexture;

	Entity player1;
	Entity player2;
	vector<Entity> bullets;
	vector<Entity> enemies;
	int numEnemies;

	vector<SheetSprite> PlayerOneLeft;
	vector<SheetSprite> PlayerOneRight;
	vector<SheetSprite> PlayerOneUp;
	vector<SheetSprite> PlayerOneDown;

	vector<SheetSprite> PlayerTwoLeft;
	vector<SheetSprite> PlayerTwoRight;
	vector<SheetSprite> PlayerTwoUp;
	vector<SheetSprite> PlayerTwoDown;

	SheetSprite pinkEnemySpaceship;
	SheetSprite blueEnemySpaceship;
	SheetSprite greenEnemySpaceship;
	SheetSprite yellowEnemySpaceship;
	SheetSprite beigeEnemySpaceship;

	void Setup();
	void LoadSprites();
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

void setBackgroundTexture(GLuint backgroundTexture) {
	glUseProgram(texturedProgram.programID);
	glBindTexture(GL_TEXTURE_2D, backgroundTexture);

	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::scale(modelMatrix, glm::vec3(3.75f, 3.75f, 1.0f));
	texturedProgram.SetModelMatrix(modelMatrix);

	float vertices[] = { -0.5f, -0.5f, 0.5f,  0.5f,	-0.5f,  0.5f, 0.5f,  0.5f, -0.5f, -0.5f, 0.5f, -0.5f };
	glVertexAttribPointer(texturedProgram.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(texturedProgram.positionAttribute);

	float texCoords[] = { 0.0, 1.0, 1.0, 0.0, 0.0, 0.0,	1.0, 0.0, 0.0, 1.0,	1.0, 1.0 };
	glVertexAttribPointer(texturedProgram.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(texturedProgram.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(texturedProgram.positionAttribute);
	glDisableVertexAttribArray(texturedProgram.texCoordAttribute);
}

void MainMenuState::Setup() {
	backgroundTexture = LoadTexture("assets/main_menu_background.jpg");
	setBackgroundTexture(backgroundTexture);

	// Play button

	// Quit button

}

void GameState::LoadSprites() {
	// Load Player One sprites
	for (int i = 0; i < 16; i++) {
		int row = i / 4;
		int col = i % 4;
		float u = row * 48.0f / 192.0f;
		float v = col * 48.0f / 192.0f;
		SheetSprite temp = SheetSprite(bettySpriteSheet, u, v, 48.0f / 192.0f, 48.0f / 192.0f, 1.0f);
		switch (row) {
		case 0:
			this->PlayerOneDown.push_back(temp);
			break;
		case 1:
			this->PlayerOneLeft.push_back(temp);
			break;
		case 2:
			this->PlayerOneUp.push_back(temp);
			break;
		case 3:
			this->PlayerOneRight.push_back(temp);
			break;
		}
	}
	// Load Player Two sprites
	for (int i = 0; i < 16; i++) {
		int row = i / 4;
		int col = i % 4;
		float u = row * 48.0f / 192.0f;
		float v = col * 48.0f / 192.0f;
		SheetSprite temp = SheetSprite(georgeSpriteSheet, u, v, 48.0f / 192.0f, 48.0f / 192.0f, 1.0f);
		switch (row) {
		case 0:
			this->PlayerTwoDown.push_back(temp);
			break;
		case 1:
			this->PlayerTwoLeft.push_back(temp);
			break;
		case 2:
			this->PlayerTwoUp.push_back(temp);
			break;
		case 3:
			this->PlayerTwoRight.push_back(temp);
			break;
		}
	}

	// Load enemy spaceship sprites
	pinkEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 0.0f / 512.0f, 294.0f / 512.0f, 124.0f / 512.0f, 127.0f / 512.0f, 1.0f);
	blueEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet,248.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 145.0f / 512.0f, 1.0f);
	greenEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 124.0f / 512.0f, 144.0f / 512.0f, 124.0f / 512.0f, 123.0f / 512.0f, 1.0f);
	yellowEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 0.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 108.0f / 512.0f, 1.0f);
	beigeEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 372.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 122.0f / 512.0f, 1.0f);
}

void GameState::Setup() {
	backgroundTexture = LoadTexture("assets/game_background.png");
	setBackgroundTexture(backgroundTexture);

	this->LoadSprites();

	this->player1.sprite = this->PlayerOneDown.at(0);
	this->player1.faceDirection = DOWN;
	this->player1.moveDirection = DOWN;
	this->player1.entityType = PLAYER;
	this->player1.position = glm::vec3(-0.2f, 0.0f, 0.0f);
	this->player1.size = glm::vec3(0.3f, 0.3f, 1.0f);
	this->player1.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	this->player2.sprite = this->PlayerTwoDown.at(0);
	this->player2.faceDirection = DOWN;
	this->player2.moveDirection = DOWN;
	this->player2.entityType = PLAYER;
	this->player2.position = glm::vec3(0.2f, 0.0f, 0.0f);
	this->player2.size = glm::vec3(0.3f, 0.3f, 1.0f);
	this->player2.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	// Initialize enemy attributes
	this->numEnemies = 10;
	for (size_t i = 0; i < this->numEnemies; i++) {
		Entity enemy;
		enemy.entityType = ENEMY;
		enemy.size = glm::vec3(0.3f, 0.3f, 1.0f);
		enemy.velocity = glm::vec3(0.0f, -0.1f, 0.0f);

		// Randomly pick the starting position of the enemy
		float x = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 2.0f));	// Get a random float between 0.0 and 2.0
		enemy.position.x = -1.0f + x;
		enemy.position.z = 0.0f;
		int topOrBottom = rand() % 2;
		if (topOrBottom) {
			enemy.position.y = 2.0f;	// Top
			enemy.velocity.y = -0.1f;	// Go down
		}
		else {
			enemy.position.y = -2.0f;	// Bottom
			enemy.velocity.y = 0.1f;	// Go up
		}

		// Randomly pick the color of the enemy
		int enemySpriteIndex = rand() % 5;
		switch (enemySpriteIndex) {
			case 0: enemy.sprite = this->pinkEnemySpaceship; break;
			case 1: enemy.sprite = this->blueEnemySpaceship; break;
			case 2: enemy.sprite = this->greenEnemySpaceship; break;
			case 3: enemy.sprite = this->yellowEnemySpaceship; break;
			case 4: enemy.sprite = this->beigeEnemySpaceship; break;
		}

		this->enemies.push_back(enemy);
	}
}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Alien Invasion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 640, SDL_WINDOW_OPENGL);
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
	bettySpriteSheet = LoadTexture("assets/betty_0.png");
	georgeSpriteSheet = LoadTexture("assets/george_0.png");
	enemySpaceshipSpriteSheet = LoadTexture("assets/SpaceShips/enemy_spaceship_spritesheet.png");

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// "Clamp" down on a texture so that the pixels on the edge repeat
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Set background color to sky blue
	glClearColor(0.6f, 0.9f, 1.0f, 1.0f);

	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.777f, 1.777f, -1.0f, 1.0f);
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
		if (this->player1.position.x > -1.0f) { // Player must be in bounds
			this->player1.velocity.x = -1.0f;
			if (!keys[SDL_SCANCODE_M]) {
				this->player1.faceDirection = LEFT;
			}
		}
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		if (this->player1.position.x < 1.0f) { // Player must be in bounds
			this->player1.velocity.x = 1.0f;
			if (!keys[SDL_SCANCODE_M]) {
				this->player1.faceDirection = RIGHT;
			}
		}
	}
	if (keys[SDL_SCANCODE_UP]) {
		if (this->player1.position.y + this->player1.sprite.height/2 < 1.777f) { // Player must be in bounds
			this->player1.velocity.y = 1.0f;
			if (!keys[SDL_SCANCODE_M]) {
				this->player1.faceDirection = UP;
			}
		}
	}
	if (keys[SDL_SCANCODE_DOWN]) {
		if (this->player1.position.y - this->player1.sprite.height/2 > -1.777f) { // Player must be in bounds
			this->player1.velocity.y = -1.0f;
			if (!keys[SDL_SCANCODE_M]) {
				this->player1.faceDirection = DOWN;
			}
		}
	}
	if (!keys[SDL_SCANCODE_LEFT] &&
		!keys[SDL_SCANCODE_RIGHT] &&
		!keys[SDL_SCANCODE_UP] &&
		!keys[SDL_SCANCODE_DOWN]) {
		this->player1.moveCounter = 0.0f;
	} else {
		if (this->player1.moveDirection == this->player1.faceDirection || keys[SDL_SCANCODE_M]) {
			this->player1.moveCounter += 0.005f;
		} else {
			this->player1.moveCounter = 0.0f;
			this->player1.moveDirection = this->player1.faceDirection;
		}
	}

	switch (this->player1.faceDirection) {
	case UP:
		this->player1.sprite = this->PlayerOneUp.at((int) this->player1.moveCounter % 4);
		break;
	case DOWN:
		this->player1.sprite = this->PlayerOneDown.at((int) this->player1.moveCounter % 4);
		break;
	case LEFT:
		this->player1.sprite = this->PlayerOneLeft.at((int) this->player1.moveCounter % 4);
		break;
	case RIGHT:
		this->player1.sprite = this->PlayerOneRight.at((int) this->player1.moveCounter % 4);
		break;
	}

	// Player Two movement
	this->player2.velocity.x = 0.0f;
	this->player2.velocity.y = 0.0f;
	if (keys[SDL_SCANCODE_A]) {
		if (this->player2.position.x > -1.0f) { // Player must be in bounds
			this->player2.velocity.x = -1.0f;
			if (!keys[SDL_SCANCODE_G]) {
				this->player2.faceDirection = LEFT;
			}
		}
	}
	if (keys[SDL_SCANCODE_D]) {
		if (this->player2.position.x < 1.0f) { // Player must be in bounds
			this->player2.velocity.x = 1.0f;
			if (!keys[SDL_SCANCODE_G]) {
				this->player2.faceDirection = RIGHT;
			}
		}
	}
	if (keys[SDL_SCANCODE_W]) {
		if (this->player2.position.y + this->player2.sprite.height/2 < 1.777f) { // Player must be in bounds
			this->player2.velocity.y = 1.0f;
			if (!keys[SDL_SCANCODE_G]) {
				this->player2.faceDirection = UP;
			}
		}
	}
	if (keys[SDL_SCANCODE_S]) {
		if (this->player2.position.y - this->player2.sprite.height/2 > -1.777f) { // Player must be in bounds
			this->player2.velocity.y = -1.0f;
			if (!keys[SDL_SCANCODE_G]) {
				this->player2.faceDirection = DOWN;
			}
		}
	}
	if (!keys[SDL_SCANCODE_A] &&
		!keys[SDL_SCANCODE_D] &&
		!keys[SDL_SCANCODE_W] &&
		!keys[SDL_SCANCODE_S]) {
		this->player2.moveCounter = 0.0f;
	}
	else {
		if (this->player2.moveDirection == this->player2.faceDirection || keys[SDL_SCANCODE_G]) {
			this->player2.moveCounter += 0.005f;
		}
		else {
			this->player2.moveCounter = 0.0f;
			this->player2.moveDirection = this->player2.faceDirection;
		}
	}

	switch (this->player2.faceDirection) {
	case UP:
		this->player2.sprite = this->PlayerTwoUp.at((int) this->player2.moveCounter % 4);
		break;
	case DOWN:
		this->player2.sprite = this->PlayerTwoDown.at((int) this->player2.moveCounter % 4);
		break;
	case LEFT:
		this->player2.sprite = this->PlayerTwoLeft.at((int) this->player2.moveCounter % 4);
		break;
	case RIGHT:
		this->player2.sprite = this->PlayerTwoRight.at((int) this->player2.moveCounter % 4);
		break;
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
	setBackgroundTexture(this->backgroundTexture);

	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.85f, 0.25f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	this->DrawText(texturedProgram, asciiSpriteSheetTexture, "Alien Invasion", 0.3f, -0.16f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.1f, -0.1f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	this->DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 0.125f, -0.075f);
}

void GameState::Render() {
	setBackgroundTexture(this->backgroundTexture);

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
