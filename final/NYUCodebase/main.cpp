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
#include <SDL_mixer.h>	// For background music

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
#define MAX_BULLETS 50
#define MAX_PARTICLES 20

SDL_Window* displayWindow;
SDL_GLContext context;
ShaderProgram program;
ShaderProgram texturedProgram;  // For textured polygons
const Uint8 *keys;
glm::mat4 projectionMatrix, viewMatrix;

enum GameMode { MAIN_MENU, GAME_LEVEL, GAME_OVER };
enum Direction { LEFT, RIGHT, UP, DOWN };
enum EntityType { PLAYER, ENEMY, BULLET, PARTICLE, BUTTON};

GLuint asciiSpriteSheetTexture;
GLuint bettySpriteSheet, georgeSpriteSheet;
GLuint enemySpaceshipSpriteSheet;
GLuint spaceInvaderSpriteSheet;
GLuint greenButtonSpriteSheet;

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
	int playerScore = 0;

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

	bool dead = false;
	bool canShoot;
	float shootCounter;
	int bulletIndex;
	void ShootBullet(Entity &bullet);

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
	if (position.x + sprite.width * size.x < otherEntity.position.x - otherEntity.sprite.width * otherEntity.size.x || 
		position.x - sprite.width * size.x > otherEntity.position.x + otherEntity.sprite.width * otherEntity.size.x || 
		position.y + sprite.height * size.y < otherEntity.position.y - otherEntity.sprite.height * otherEntity.size.y || 
		position.y - sprite.height * size.y > otherEntity.position.y + otherEntity.sprite.height * otherEntity.size.y) {
		return false;
	}
	collidedBottom = false;
	collidedLeft = false;
	collidedRight = false;
	collidedTop = false;
	if (otherEntity.entityType == PLAYER) {
		ResolveCollisionX(otherEntity);
		ResolveCollisionY(otherEntity);
	}
	return true;
}

void Entity::ResolveCollisionX(Entity& otherEntity) {
	float penetration = fabs(fabs(position.x - otherEntity.position.x) - sprite.width/2 - otherEntity.sprite.width/2);

	if (position.x < otherEntity.position.x) {
		collidedRight = true; // A right collision occurred
	} else {
		collidedLeft = true; // A left collision occurred
	}
}

void Entity::ResolveCollisionY(Entity& otherEntity) {
	float penetration = fabs(fabs(position.y - otherEntity.position.y) - sprite.height / 2 - otherEntity.sprite.height / 2);

	if (position.y < otherEntity.position.y) {
		collidedBottom = true; // A top collision occurred
	} else {
		collidedTop = true; // A bottom collision occurred
	}
}

void Entity::ShootBullet(Entity &bullet) {
	bullet.position = glm::vec3(this->position.x, this->position.y, 1.0f);
	switch (this->faceDirection) {
	case UP:
		bullet.velocity = glm::vec3(0.0f, 1.3f, 0.0f);
		break;
	case DOWN:
		bullet.velocity = glm::vec3(0.0f, -1.3f, 0.0f);
		break;
	case LEFT:
		bullet.velocity = glm::vec3(-1.3f, 0.0f, 0.0f);
		break;
	case RIGHT:
		bullet.velocity = glm::vec3(1.3f, 0.0f, 0.0f);
		break;
	}
}

struct MainMenuState {
	Entity playButton;
	Entity quitButton;

	GLuint backgroundTexture;
	void Setup();
	void ProcessEvents();
	void Render();
};

struct GameState {
	Entity Betty;
	Entity George;
	vector<Entity> BulletsBetty;
	vector<Entity> BulletsGeorge;
	vector<Entity> particles;
	vector<Entity> enemies;

	int numberOfEnemies;
	float spawnRate;
	float enemySpeed;

	GLuint backgroundTexture;

	SheetSprite skull;
	vector<SheetSprite> PlayerOneLeft;
	vector<SheetSprite> PlayerOneRight;
	vector<SheetSprite> PlayerOneUp;
	vector<SheetSprite> PlayerOneDown;

	vector<SheetSprite> PlayerTwoLeft;
	vector<SheetSprite> PlayerTwoRight;
	vector<SheetSprite> PlayerTwoUp;
	vector<SheetSprite> PlayerTwoDown;

	SheetSprite bulletBetty;
	SheetSprite bulletGeorge;
	SheetSprite particleBetty;
	SheetSprite particleGeorge;

	SheetSprite pinkEnemySpaceship;
	SheetSprite blueEnemySpaceship;
	SheetSprite greenEnemySpaceship;
	SheetSprite yellowEnemySpaceship;
	SheetSprite beigeEnemySpaceship;

	void Setup();
	void SpawnEnemies();
	void LoadSprites();
	void CreateBoom(SheetSprite &sheet, float x, float y);
	void ProcessEvents();
	void Update(float elapsed);
	void Render();
	void Reset();
};

struct GameOverState {
	Entity playAgainButton;
	Entity mainMenuButton;
	Entity quitButton;

	SheetSprite greenButton;

	GLuint backgroundTexture;
	void Setup();
	void ProcessEvents();
	void Render();
};

GameMode mode;
MainMenuState mainMenuState;
GameState gameState;
GameOverState gameOverState;

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

	// Play background music
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
	Mix_Music* backgroundMusic = Mix_LoadMUS("assets/background_music.mp3");
	Mix_PlayMusic(backgroundMusic, -1);

	// Play button

	// Quit button

}

void GameState::LoadSprites() {
	// Load Betty sprites
	for (int i = 0; i < 16; i++) {
		int row = i / 4;
		int col = i % 4;
		float u = (row * 48.0f + 5.0f) / 192.0f;
		float v = (col * 48.0f + 5.0f) / 192.0f;
		SheetSprite temp = SheetSprite(bettySpriteSheet, u, v, 38.0f / 192.0f, 38.0f / 192.0f, 1.0f);
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
	// Load George sprites
	for (int i = 0; i < 16; i++) {
		int row = i / 4;
		int col = i % 4;
		float u = (row * 48.0f + 5.0f) / 192.0f;
		float v = (col * 48.0f + 5.0f) / 192.0f;
		SheetSprite temp = SheetSprite(georgeSpriteSheet, u, v, 38.0f / 192.0f, 38.0f / 192.0f, 1.0f);
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

	// Load Player Bullets sprites
	GLuint bulletBettyTexture = LoadTexture("assets/BulletBetty.png");
	bulletBetty = SheetSprite(bulletBettyTexture, 0.0f / 24.0f, 0.0f / 24.0f, 24.0f / 24.0f, 24.0f / 24.0f, 1.0f);
	GLuint bulletGeorgeTexture = LoadTexture("assets/BulletGeorge.png");
	bulletGeorge = SheetSprite(bulletGeorgeTexture, 0.0f / 24.0f, 0.0f / 24.0f, 24.0f / 24.0f, 24.0f / 24.0f, 1.0f);
	
	// Load Skull sprite
	GLuint skullTexture = LoadTexture("assets/skull.png");
	skull = SheetSprite(skullTexture, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	// Load particle sprites
	particleBetty = SheetSprite(spaceInvaderSpriteSheet, 602.0f / 1024.0f, 600.0f / 1024.0f, 48.0f / 1024.0f, 46.0f / 1024.0f, 1.0f);
	particleGeorge = SheetSprite(spaceInvaderSpriteSheet, 434.0f / 1024.0f, 325.0f / 1024.0f, 48.0f / 1024.0f, 46.0f / 1024.0f, 1.0f);
	
	// Load enemy spaceship sprites
	pinkEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 0.0f / 512.0f, 294.0f / 512.0f, 124.0f / 512.0f, 127.0f / 512.0f, 1.0f);
	blueEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 248.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 145.0f / 512.0f, 1.0f);
	greenEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 124.0f / 512.0f, 144.0f / 512.0f, 124.0f / 512.0f, 123.0f / 512.0f, 1.0f);
	yellowEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 0.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 108.0f / 512.0f, 1.0f);
	beigeEnemySpaceship = SheetSprite(enemySpaceshipSpriteSheet, 372.0f / 512.0f, 0.0f / 512.0f, 124.0f / 512.0f, 122.0f / 512.0f, 1.0f);
}

void GameState::SpawnEnemies() {
	for (int i = 0; i < this->numberOfEnemies; i++) {
		Entity enemy;
		float enemySize = 0.2 + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 0.12f));;
		enemy.size = glm::vec3(enemySize, enemySize, 1.0f);
		enemy.velocity = glm::vec3(0.0f, -0.1f, 0.0f);

		// Randomly pick the starting position of the enemy
		float x = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 2.0f));	// Get a random float between 0.0 and 2.0
		enemy.position.x = -1.0f + x;
		enemy.position.z = 0.0f;
		int topOrBottom = rand() % 2;
		if (topOrBottom) {
			enemy.position = glm::vec3(-1.0f + x, 2.0f, 0.0f);
			enemy.velocity = glm::vec3(0.0f, 0 - this->enemySpeed, 0.0f);
		} else {
			enemy.position = glm::vec3(-1.0f + x, -2.0f, 0.0f);
			enemy.velocity = glm::vec3(0.0f, this->enemySpeed, 0.0f);
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

void GameState::CreateBoom(SheetSprite &sheet, float x, float y) {
	Entity particle;
	particle.sprite = sheet;
	particle.entityType = PARTICLE;
	particle.position = glm::vec3(x, y, 0.0f);
	particle.size = glm::vec3(0.0f, 0.0f, 0.0f);
	particle.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	this->particles.push_back(particle);
}

void GameState::Setup() {
	this->backgroundTexture = LoadTexture("assets/game_background.png");
	this->LoadSprites();

	this->Betty.sprite = this->PlayerOneDown.at(0);
	this->Betty.faceDirection = DOWN;
	this->Betty.moveDirection = DOWN;
	this->Betty.entityType = PLAYER;
	this->Betty.position = glm::vec3(-0.2f, 0.0f, 0.0f);
	this->Betty.size = glm::vec3(0.25f, 0.25f, 1.0f);
	this->Betty.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	this->George.sprite = this->PlayerTwoDown.at(0);
	this->George.faceDirection = DOWN;
	this->George.moveDirection = DOWN;
	this->George.entityType = PLAYER;
	this->George.position = glm::vec3(0.2f, 0.0f, 0.0f);
	this->George.size = glm::vec3(0.25f, 0.25f, 1.0f);
	this->George.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < MAX_BULLETS; i++) {
		Entity bullet;
		bullet.sprite = this->bulletBetty;
		bullet.entityType = BULLET;
		bullet.position = glm::vec3(-1000.0f, 0.0f, 0.0f);
		bullet.size = glm::vec3(0.05f, 0.05f, 1.0f);
		bullet.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		this->BulletsBetty.push_back(bullet);
	}

	for (int i = 0; i < MAX_BULLETS; i++) {
		Entity bullet;
		bullet.sprite = this->bulletGeorge;
		bullet.entityType = BULLET;
		bullet.position = glm::vec3(1000.0f, 0.0f, 0.0f);
		bullet.size = glm::vec3(0.05f, 0.05f, 1.0f);
		bullet.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		this->BulletsGeorge.push_back(bullet);
	}

	// Initialize enemy attributes
	this->spawnRate = 0.0f;
	this->numberOfEnemies = 5;
	this->enemySpeed = 0.1f;
	this->SpawnEnemies();
}

void GameOverState::Setup() {
	Mix_PauseMusic();

	greenButton = SheetSprite(greenButtonSpriteSheet, 0.0f / 512.0f, 0.0f / 256.0f, 190.0f / 512.0f, 49.0f / 256.0f, 1.0f);;

	playAgainButton.sprite = greenButton;
	playAgainButton.entityType = BUTTON;
	playAgainButton.position = glm::vec3(0.0f, 0.5f, 0.0f);
	playAgainButton.size = glm::vec3(0.5f, 0.25f, 1.0f);	// 2:1 ratio for x:y to match the dimensions of the sprite sheet
	
	mainMenuButton.sprite = greenButton;
	mainMenuButton.entityType = BUTTON;
	mainMenuButton.position = glm::vec3(0.0f, 0.0f, 0.0f);
	mainMenuButton.size = glm::vec3(0.5f, 0.25f, 1.0f);

	quitButton.sprite = greenButton;
	quitButton.entityType = BUTTON;
	quitButton.position = glm::vec3(0.0f, -0.5f, 0.0f);
	quitButton.size = glm::vec3(0.5f, 0.25f, 1.0f);
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
	greenButtonSpriteSheet = LoadTexture("assets/green_buttons_spritesheet.png");
	spaceInvaderSpriteSheet = LoadTexture("assets/SpaceShooter/SpriteSheet/sheet.png");

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

	mode = GAME_OVER; // Render the menu when the user opens the game
	//mainMenuState.Setup();
	gameOverState.Setup();
}

bool clicked(Entity &entity, float cursorX, float cursorY) {
	if (cursorX < entity.position.x + entity.sprite.width + 0.15 &&
		cursorX > entity.position.x - entity.sprite.width - 0.15 &&
		cursorY < entity.position.y + entity.sprite.height / 2 &&
		cursorY > entity.position.y - entity.sprite.height / 2) {
		return true;
	}
	return false;
}

void MainMenuState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		// Get coordinates of where the user clicked
		float cursorX = (((float)event.button.x / 640.0f) * 3.554f) - 1.777f;
		float cursorY = (((float)(640.0f - event.button.y) / 640.0f) * 3.554f) - 1.777f;

		//if (clicked(playButton, cursorX, cursorY)) {
			mode = GAME_LEVEL;
			gameState.Setup();
		//}
	//	else if (clicked(quitButton, cursorX, cursorY)) {
		//	done = true;
		//}
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
	this->Betty.velocity.x = 0.0f;
	this->Betty.velocity.y = 0.0f;
	if (!this->Betty.dead) {
		if (keys[SDL_SCANCODE_LEFT]) {
			if (this->Betty.position.x > -1.22f) {
				this->Betty.velocity.x = -1.0f;
			}
			if (!keys[SDL_SCANCODE_M]) {
				this->Betty.faceDirection = LEFT;
			}
		}
		if (keys[SDL_SCANCODE_RIGHT]) {
			if (this->Betty.position.x < 1.27f) {
				this->Betty.velocity.x = 1.0f;
			}
			if (!keys[SDL_SCANCODE_M]) {
				this->Betty.faceDirection = RIGHT;
			}
		}
		if (keys[SDL_SCANCODE_UP]) {
			if (this->Betty.position.y + this->Betty.sprite.height / 2 < 1.777f) {
				this->Betty.velocity.y = 1.0f;
			}
			if (!keys[SDL_SCANCODE_M]) {
				this->Betty.faceDirection = UP;
			}
		}
		if (keys[SDL_SCANCODE_DOWN]) {
			if (this->Betty.position.y - this->Betty.sprite.height / 2 > -1.777f) {
				this->Betty.velocity.y = -1.0f;
			}
			if (!keys[SDL_SCANCODE_M]) {
				this->Betty.faceDirection = DOWN;
			}
		}
		if (!keys[SDL_SCANCODE_LEFT] &&
			!keys[SDL_SCANCODE_RIGHT] &&
			!keys[SDL_SCANCODE_UP] &&
			!keys[SDL_SCANCODE_DOWN]) {
			this->Betty.moveCounter = 0.0f;
		}
		else {
			if (this->Betty.moveDirection == this->Betty.faceDirection || keys[SDL_SCANCODE_M]) {
				this->Betty.moveCounter += 0.005f;
			}
			else {
				this->Betty.moveCounter = 0.0f;
				this->Betty.moveDirection = this->Betty.faceDirection;
			}
		}

		switch (this->Betty.faceDirection) {
		case UP:
			this->Betty.sprite = this->PlayerOneUp.at((int)this->Betty.moveCounter % 4);
			break;
		case DOWN:
			this->Betty.sprite = this->PlayerOneDown.at((int)this->Betty.moveCounter % 4);
			break;
		case LEFT:
			this->Betty.sprite = this->PlayerOneLeft.at((int)this->Betty.moveCounter % 4);
			break;
		case RIGHT:
			this->Betty.sprite = this->PlayerOneRight.at((int)this->Betty.moveCounter % 4);
			break;
		}

		if (keys[SDL_SCANCODE_N] && this->Betty.canShoot) {
			this->Betty.canShoot = false;
			this->Betty.ShootBullet(this->BulletsBetty.at(this->Betty.bulletIndex % MAX_BULLETS));
			this->Betty.bulletIndex++;
		}
	}

	// Player Two movement
	this->George.velocity.x = 0.0f;
	this->George.velocity.y = 0.0f;
	if (!this->George.dead) {
		if (keys[SDL_SCANCODE_A]) {
			if (this->George.position.x > -1.22f) {
				this->George.velocity.x = -1.0f;
			}
			if (!keys[SDL_SCANCODE_G]) {
				this->George.faceDirection = LEFT;
			}
		}
		if (keys[SDL_SCANCODE_D]) {
			if (this->George.position.x < 1.27f) {
				this->George.velocity.x = 1.0f;
			}
			if (!keys[SDL_SCANCODE_G]) {
				this->George.faceDirection = RIGHT;
			}
		}
		if (keys[SDL_SCANCODE_W]) {
			if (this->George.position.y + this->George.sprite.height / 2 < 1.777f) {
				this->George.velocity.y = 1.0f;
			}
			if (!keys[SDL_SCANCODE_G]) {
				this->George.faceDirection = UP;
			}
		}
		if (keys[SDL_SCANCODE_S]) {
			if (this->George.position.y - this->George.sprite.height / 2 > -1.777f) {
				this->George.velocity.y = -1.0f;
			}
			if (!keys[SDL_SCANCODE_G]) {
				this->George.faceDirection = DOWN;
			}
		}
		if (!keys[SDL_SCANCODE_A] &&
			!keys[SDL_SCANCODE_D] &&
			!keys[SDL_SCANCODE_W] &&
			!keys[SDL_SCANCODE_S]) {
			this->George.moveCounter = 0.0f;
		}
		else {
			if (this->George.moveDirection == this->George.faceDirection || keys[SDL_SCANCODE_G]) {
				this->George.moveCounter += 0.005f;
			}
			else {
				this->George.moveCounter = 0.0f;
				this->George.moveDirection = this->George.faceDirection;
			}
		}

		switch (this->George.faceDirection) {
		case UP:
			this->George.sprite = this->PlayerTwoUp.at((int)this->George.moveCounter % 4);
			break;
		case DOWN:
			this->George.sprite = this->PlayerTwoDown.at((int)this->George.moveCounter % 4);
			break;
		case LEFT:
			this->George.sprite = this->PlayerTwoLeft.at((int)this->George.moveCounter % 4);
			break;
		case RIGHT:
			this->George.sprite = this->PlayerTwoRight.at((int)this->George.moveCounter % 4);
			break;
		}

		if (keys[SDL_SCANCODE_F] && this->George.canShoot) {
			this->George.canShoot = false;
			this->George.ShootBullet(this->BulletsGeorge.at(this->George.bulletIndex % MAX_BULLETS));
			this->George.bulletIndex++;
		}
	}
}

void GameOverState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		// Get coordinates of where the user clicked
		float cursorX = (((float)event.button.x / 640.0f) * 3.554f) - 1.777f;
		float cursorY = (((float)(640.0f - event.button.y) / 640.0f) * 3.554f) - 1.777f;

		/*playAgainButton.position.x = cursorX - playAgainButton.sprite.width;
		playAgainButton.position.y = cursorY - playAgainButton.sprite.height / 2;*/

		if (clicked(playAgainButton, cursorX, cursorY)) {
			//reset();
			Mix_RewindMusic();
			mode = GAME_LEVEL;
			gameState.Setup();
		}
		else if (clicked(mainMenuButton, cursorX, cursorY)) {
			//reset();
			Mix_RewindMusic();
			mode = MAIN_MENU;
			mainMenuState.Setup();
		}
		else if (clicked(quitButton, cursorX, cursorY)) {
			done = true;
		}
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
	case GAME_OVER:
		gameOverState.ProcessEvents();
	}
}

void GameState::Update(float elapsed) {
	if (!this->Betty.dead && !this->George.dead) {
		if (Betty.CollidesWith(George)) {
			if (keys[SDL_SCANCODE_RIGHT] && this->Betty.collidedRight) {
				this->Betty.velocity.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_LEFT] && this->Betty.collidedLeft) {
				this->Betty.velocity.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_UP] && this->Betty.collidedBottom) {
				this->Betty.velocity.y = 0.0f;
			}
			if (keys[SDL_SCANCODE_DOWN] && this->Betty.collidedTop) {
				this->Betty.velocity.y = 0.0f;
			}
		}
		if (George.CollidesWith(Betty)) {
			if (keys[SDL_SCANCODE_D] && this->George.collidedRight) {
				this->George.velocity.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_A] && this->George.collidedLeft) {
				this->George.velocity.x = 0.0f;
			}
			if (keys[SDL_SCANCODE_W] && this->George.collidedBottom) {
				this->George.velocity.y = 0.0f;
			}
			if (keys[SDL_SCANCODE_S] && this->George.collidedTop) {
				this->George.velocity.y = 0.0f;
			}
		}
	}
	
	this->Betty.Update(elapsed);
	this->George.Update(elapsed);

	if (!this->Betty.canShoot) {
		this->Betty.shootCounter += elapsed;
	}
	if (this->Betty.shootCounter > 0.5f && !this->Betty.canShoot) {
		this->Betty.canShoot = true;
		this->Betty.shootCounter = 0.0f;
	}

	if (!this->George.canShoot) {
		this->George.shootCounter += elapsed;
	}
	if (this->George.shootCounter > 0.5f && !this->George.canShoot) {
		this->George.canShoot = true;
		this->George.shootCounter = 0.0f;
	}

	for (int i = 0; i < this->BulletsBetty.size(); i++) {
		this->BulletsBetty.at(i).Update(elapsed);
		for (int j = 0; j < this->enemies.size(); j++) {
			if (this->BulletsBetty.at(i).CollidesWith(this->enemies.at(j))) {
				this->CreateBoom(this->particleBetty, this->BulletsBetty.at(i).position.x, this->BulletsBetty.at(i).position.y);
				this->Betty.playerScore++;
				this->BulletsBetty.at(i).position = glm::vec3(-1000.0f, 0.0f, 0.0f);
				this->BulletsBetty.at(i).velocity = glm::vec3(0.0f, 0.0f, 0.0f);
				this->enemies.erase(this->enemies.begin() + j);
			}
		}
		if (this->BulletsBetty.at(i).CollidesWith(this->George)) {
			this->George.sprite = skull;
			this->George.dead = true;
			this->BulletsBetty.at(i).position = glm::vec3(-1000.0f, 0.0f, 0.0f);
			this->BulletsBetty.at(i).velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		}
	}
	for (int i = 0; i < this->BulletsGeorge.size(); i++) {
		this->BulletsGeorge.at(i).Update(elapsed);
		for (int j = 0; j < this->enemies.size(); j++) {
			if (this->BulletsGeorge.at(i).CollidesWith(this->enemies.at(j))) {
				this->CreateBoom(this->particleGeorge, this->BulletsGeorge.at(i).position.x, this->BulletsGeorge.at(i).position.y);
				this->George.playerScore++;
				this->BulletsGeorge.at(i).position = glm::vec3(1000.0f, 0.0f, 0.0f);
				this->BulletsGeorge.at(i).velocity = glm::vec3(0.0f, 0.0f, 0.0f);
				this->enemies.erase(this->enemies.begin() + j);
			}
		}
		if (this->BulletsGeorge.at(i).CollidesWith(this->Betty)) {
			this->Betty.sprite = skull;
			this->Betty.dead = true;
			this->BulletsGeorge.at(i).position = glm::vec3(1000.0f, 0.0f, 0.0f);
			this->BulletsGeorge.at(i).velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		}
	}
	for (int i = 0; i < this->particles.size(); i++) {
		if (this->particles.at(i).size.x < 0.4f) {
			this->particles.at(i).size.x += 0.004f;
			this->particles.at(i).size.y += 0.004f;
		} else {
			this->particles.erase(this->particles.begin() + i);
		}
	}
	this->spawnRate += elapsed;
	if (this->spawnRate > 4.0f) {
		this->spawnRate = 0.0f;
		this->numberOfEnemies++;
		this->enemySpeed += 0.05f;
		this->SpawnEnemies();
	}
	for (int i = 0; i < this->enemies.size(); i++) {
		this->enemies.at(i).Update(elapsed);
		if (this->enemies.at(i).CollidesWith(this->Betty)) {
			this->Betty.sprite = this->skull;
			this->Betty.dead = true;
		}
		if (this->enemies.at(i).CollidesWith(this->George)) {
			this->George.sprite = this->skull;
			this->George.dead = true;
		}
		if (this->enemies.at(i).position.y < -2.0f || this->enemies.at(i).position.y > 2.0f) {
			this->enemies.erase(this->enemies.begin() + i);
		}
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
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Alien Invasion", 0.3f, -0.16f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.1f, -0.1f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 0.125f, -0.075f);
}

void GameState::Render() {
	setBackgroundTexture(this->backgroundTexture);
	this->Betty.Render(texturedProgram);
	this->George.Render(texturedProgram);
	for (int i = 0; i < this->BulletsBetty.size(); i++) {
		this->BulletsBetty.at(i).Render(texturedProgram);
	}
	for (int i = 0; i < this->BulletsGeorge.size(); i++) {
		this->BulletsGeorge.at(i).Render(texturedProgram);
	}
	for (int i = 0; i < this->particles.size(); i++) {
		this->particles.at(i).Render(texturedProgram);
	}
	for (int i = 0; i < this->enemies.size(); i++) {
		this->enemies.at(i).Render(texturedProgram);
	}
}

void GameOverState::Render() {
	// If player 1 has a higher score
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.85f, 1.0f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Player 1 wins!", 0.3f, -0.16f);

	playAgainButton.Render(texturedProgram);
	mainMenuButton.Render(texturedProgram);
	quitButton.Render(texturedProgram);
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
	case GAME_OVER:
		gameOverState.Render();
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
    }
	Cleanup();
	SDL_Quit();
    return 0;
}
