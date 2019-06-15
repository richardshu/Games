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
#define GRAVITY -2.0f

SDL_Window* displayWindow;
ShaderProgram texturedProgram;  // For textured polygons

bool done = false;				// Game loop
float lastFrameTicks = 0.0f;	// Set time to an initial value of 0
float accumulator = 0.0f;

int mapHeight;
int mapWidth;
unsigned int** mapData;

GLuint asciiSpriteSheetTexture;
GLuint arneSpriteSheetTexture;

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
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program.texCoordAttribute);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

enum EntityType { ENTITY_PLAYER, ENTITY_COIN, ENTITY_TILE };

class Entity {
public:
	void Update(float elapsed);
	void Draw(ShaderProgram &program);
	bool CollidesWith(Entity &otherEntity);

	SheetSprite sprite;

	glm::vec3 position;
	glm::vec3 size;
	glm::vec3 velocity;
	glm::vec3 acceleration;

	bool isStatic;
	EntityType entityType;

	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;

private:
	void ResolveCollisionX(Entity &otherEntity);
	void ResolveCollisionY(Entity &otherEntity);
};

float lerp(float v0, float v1, float t) {
	return (1.0 - t)*v0 + t * v1;
}

void Entity::Update(float elapsed) {
	velocity.x = lerp(velocity.x, 0.0f, elapsed * FRICTION);
	
	velocity.x += acceleration.x * elapsed;
	velocity.y += acceleration.y * elapsed;

	position.x += elapsed * velocity.x;
	position.y += elapsed * velocity.y;
}

void Entity::Draw(ShaderProgram &program) {
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
	// Remove coins when player collides with them
	if (otherEntity.entityType == ENTITY_COIN) {
		otherEntity.position = glm::vec3(100.0f, 100.0f, 0.0f);
	}
	else {
		ResolveCollisionX(otherEntity);
		ResolveCollisionY(otherEntity);
	}
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
	velocity.y = 0.0f; // Reset
}

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
}

void DrawTileMap(ShaderProgram &program, unsigned int spriteSheetTexture) {
	vector<float> vertexData;
	vector<float> texCoordData;
	for (int y = 0; y < mapHeight; y++) {
		for (int x = 0; x < mapWidth; x++) {
			float u = (float)(((int)mapData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
			float v = (float)(((int)mapData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;
			float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
			float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;
			vertexData.insert(vertexData.end(), {
				TILE_SIZE * x, -TILE_SIZE * y,
				TILE_SIZE * x, (-TILE_SIZE * y) - TILE_SIZE,
				(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,
				TILE_SIZE * x, -TILE_SIZE * y,
				(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,
				(TILE_SIZE * x) + TILE_SIZE, -TILE_SIZE * y
			});
			texCoordData.insert(texCoordData.end(), {
				u, v,
				u, v + (spriteHeight),
				u + spriteWidth, v + (spriteHeight),
				u, v,
				u + spriteWidth, v + (spriteHeight),
				u + spriteWidth, v
			});
		}
	}
	glBindTexture(GL_TEXTURE_2D, spriteSheetTexture);

	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

enum GameMode { MAIN_MENU, GAME_LEVEL };

struct GameState {
	Entity player;
	std::vector<Entity> coins;
	std::vector<Entity> tiles;
};

GameState state;
GameMode mode;

void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(worldY / -TILE_SIZE);
}

bool readHeader(std::ifstream &inputFileStream) {
	string line;
	mapWidth = -1;
	mapHeight = -1;
	while (getline(inputFileStream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "width") {
			mapWidth = atoi(value.c_str()); // Convert the string "value" into an int and assign it to mapWidth
		}
		else if (key == "height") {
			mapHeight = atoi(value.c_str());
		}
	}
	if (mapWidth == -1 || mapHeight == -1) {
		return false;
	}
	// Allocate space for our level map data
	mapData = new unsigned int*[mapHeight];
	for (int i = 0; i < mapHeight; ++i) {
		mapData[i] = new unsigned int[mapWidth];
	}
	return true;
}

bool readLayerData(std::ifstream &inputFileStream) {
	string line;
	while (getline(inputFileStream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data") {
			for (int y = 0; y < mapHeight; y++) {
				getline(inputFileStream, line);
				istringstream lineStream(line);
				string tile;
				for (int x = 0; x < mapWidth; x++) {
					getline(lineStream, tile, ',');
					unsigned int val = (unsigned int)atoi(tile.c_str());
					if (val > 0) {
						// be careful, the tiles in this format are indexed from 1 not 0
						mapData[y][x] = val - 1;
					}
					else {
						mapData[y][x] = 0;
					}
				}
			}
		}
	}
	return true;
}

bool placeEntity(const string& type, float placeX, float placeY) {
	// Shift the x and y coordinates from the center to the top left corner of the screen
	// This way the map starts at the top left corner
	placeX -= 1.777f;
	placeY += 1.0f;
	if (type == "Coin") {
		Entity coin;
		coin.position = glm::vec3(placeX, placeY, 0.0f);
		state.coins.push_back(coin);
	}
	else if (type == "Player") {
		state.player.position = glm::vec3(placeX, placeY, 0.0f);
	}
	return true;
}

bool readEntityData(std::ifstream &inputFileStream) {
	string line;
	string type;
	while (getline(inputFileStream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "type") {
			type = value;
		}
		else if (key == "location") {
			istringstream lineStream(value);
			string xPosition, yPosition;
			getline(lineStream, xPosition, ',');
			getline(lineStream, yPosition, ',');
			float placeX = atoi(xPosition.c_str()) * TILE_SIZE;
			float placeY = atoi(yPosition.c_str()) * -TILE_SIZE;
			placeEntity(type, placeX, placeY);
		}
	}
	return true;
}

void readFlaremap() {
	ifstream inputFileStream("flaremap.txt");
	string line;
	while (getline(inputFileStream, line)) {
		if (line == "[header]") {
			if (!readHeader(inputFileStream)) {
				return;
			}
		}
		else if (line == "[layer]") {
			readLayerData(inputFileStream);
		}
		else if (line == "[ObjectsLayer]") {
			readEntityData(inputFileStream);
		}
	}
}

void SetupMainMenu() {}

void SetupGameLevel() {
	readFlaremap();
	DrawTileMap(texturedProgram, arneSpriteSheetTexture);

	// Initialize player attributes
	state.player.sprite = SheetSprite(arneSpriteSheetTexture, 3.0f * 16.0f / 256.0f, 6.0f * 16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.15f);
	state.player.size = glm::vec3(2.0f, 1.0f, 1.0f); // x:y ratio is 2:1 due the sprite sheet image dimensions
	state.player.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	state.player.acceleration = glm::vec3(0.0f, GRAVITY, 0.0f);
	state.player.isStatic = false;
	state.player.entityType = ENTITY_PLAYER;
	state.player.collidedTop = false;
	state.player.collidedBottom = false;
	state.player.collidedLeft = false;
	state.player.collidedRight = false;

	// Initialize tile attributes
	for (int y = 0; y < mapHeight; y++) {
		for (int x = 0; x < mapWidth; x++) {
			if (mapData[y][x] != 0) {
				// Pass in mapData[y][x] (an index) into the SheetSprite constructor and let it handle the correct sprite image to render
				Entity tile;
				if (mapData[y][x] == 1) {
					tile.sprite = SheetSprite(arneSpriteSheetTexture, 1.0f * 16.0f / 256.0f, 0.0f * 16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.15f);
				}
				else if (mapData[y][x] == 2) {
					tile.sprite = SheetSprite(arneSpriteSheetTexture, 2.0f * 16.0f / 256.0f, 0.0f * 16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.15f);
				}
				else if (mapData[y][x] == 20) {
					tile.sprite = SheetSprite(arneSpriteSheetTexture, 4.0f * 16.0f / 256.0f, 1.0f * 16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.15f);
				}
				tile.size = glm::vec3(2.0f, 1.0f, 1.0f);
				// Convert tile coordinates to world coordinates
				tile.position = glm::vec3(x * TILE_SIZE - 1.777f, y * -TILE_SIZE + 1.0f, 0.0f);
				tile.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
				tile.acceleration = glm::vec3(0.0f, 0.0f, 0.0f);
				tile.isStatic = true;
				tile.entityType = ENTITY_TILE;
				tile.collidedTop = false;
				tile.collidedBottom = false;
				tile.collidedLeft = false;
				tile.collidedRight = false;
				state.tiles.push_back(tile);
			}
		}
	}

	// Initialize coin attributes
	for (size_t i = 0; i < state.coins.size(); i++) {
		state.coins[i].sprite = SheetSprite(arneSpriteSheetTexture, 4.0f * 16.0f / 256.0f, 3.0f * 16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.15f);
		state.coins[i].size = glm::vec3(2.0f, 1.0f, 1.0f);
		state.coins[i].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		state.coins[i].acceleration = glm::vec3(0.0f, 0.0f, 0.0f);
		state.coins[i].isStatic = true;
		state.coins[i].entityType = ENTITY_COIN;
		state.coins[i].collidedTop = false;
		state.coins[i].collidedBottom = false;
		state.coins[i].collidedLeft = false;
		state.coins[i].collidedRight = false;
	}
}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Platformer by Richard Shu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 360);

	// Load shader program
	texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	// Load sprite sheets
	asciiSpriteSheetTexture = LoadTexture(RESOURCE_FOLDER"ascii_spritesheet.png");
	arneSpriteSheetTexture = LoadTexture(RESOURCE_FOLDER"arne_spritesheet.png");

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// "Clamp" down on a texture so that the pixels on the edge repeat
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Set background color to sky blue
	glClearColor(0.6f, 0.9f, 1.0f, 1.0f);

	glm::mat4 viewMatrix = glm::mat4(1.0f);
	glm::mat4 projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

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
		// Move left
		if (keys[SDL_SCANCODE_LEFT]) {
			state.player.acceleration.x = -1.0f;
		}
		// Move right
		else if (keys[SDL_SCANCODE_RIGHT]) {
			state.player.acceleration.x = 1.0f;
		}
		else {
			state.player.acceleration.x = 0.0f;
		}

		// Jump
		if (event.key.keysym.scancode == SDL_SCANCODE_UP) {
			state.player.velocity.y = 1.0f;
		}
	}
}

void Update(float elapsed) {
	// Call each NONSTATIC entities' Update() method
	state.player.Update(elapsed);
	for (size_t i = 0; i < state.tiles.size(); i++) {
		state.player.CollidesWith(state.tiles[i]);
	}
	for (size_t i = 0; i < state.coins.size(); i++) {
		state.player.CollidesWith(state.coins[i]);
	}
}

void RenderMainMenu() {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.625f, 0.25f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Platformer", 0.3f, -0.16f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.1f, -0.1f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 0.125f, -0.075f);
}

void RenderGameLevel() {
	// Loop through entities and call their draw methods
	state.player.Draw(texturedProgram);
	for (size_t i = 0; i < state.tiles.size(); i++) {
		state.tiles[i].Draw(texturedProgram);
	}
	for (size_t i = 0; i < state.coins.size(); i++) {
		state.coins[i].Draw(texturedProgram);
	}

	// Allow scrolling by setting the view matrix to the inverse of the player's position coordinates
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::translate(viewMatrix, glm::vec3(-state.player.position.x, -state.player.position.y, -state.player.position.z));
	texturedProgram.SetViewMatrix(viewMatrix);
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

void Cleanup() {
	
}

int main(int argc, char *argv[])
{
	Setup();
	while (!done) {
		ProcessEvents();

		// Calculate elapsed time
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks; // Reset

		// Use fixed timestep (instead of variable timestep)
		elapsed += accumulator;
		if (elapsed < FIXED_TIMESTEP) {
			accumulator = elapsed;
			continue;
		}
		while (elapsed >= FIXED_TIMESTEP) {
			Update(FIXED_TIMESTEP);
			elapsed -= FIXED_TIMESTEP;
		}
		accumulator = elapsed;
		Render();
    }
	Cleanup();
	SDL_Quit();
    return 0;
}
