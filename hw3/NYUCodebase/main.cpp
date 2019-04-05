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

bool done = false;				// Game loop
float lastFrameTicks = 0.0f;	// Set time to an initial value of 0

int MAX_NUM_LASERS  = 30;
int MAX_NUM_METEORS = 30;

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

	float texCoords[] = {
		u, v,
		u, v + height,
		u + width, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};
	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program.texCoordAttribute);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);

	glUseProgram(program.programID);
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
	
	// Designate the creation of vertices and texture 
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
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
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
	// draw this yourself, use text.size() * 6 or vertexData.size()/2 to get number of vertices
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

enum GameMode { MAIN_MENU, GAME_LEVEL };

struct GameState {
	Entity player;
	std::vector<Entity> lasers;
	std::vector<Entity> meteors;
};

GameState state;
GameMode mode;

void SetupMainMenu() {
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Space Invaders", 1.0f, 0.0f);
	DrawText(texturedProgram, asciiSpriteSheetTexture, "Play", 1.0f, 0.0f);
}

void SetupGameLevel() {

	// Initialize player spaceship
	state.player.position = glm::vec3(0.0f, -0.75f, 0.0f);
	state.player.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	state.player.size = glm::vec3(1.0f, 1.0f, 1.0f);
	state.player.sprite = SheetSprite(spaceSpriteSheetTexture, 237.0f / 1024.0f, 377.0f / 1024.0f, 99.0f / 1024.0f, 75.0f / 1024.0f, 0.2f);

	// Initialize lasers
	for (int i = 0; i < MAX_NUM_LASERS; i++) {
		Entity laser;
		laser.sprite = SheetSprite(spaceSpriteSheetTexture, 740.0f / 1024.0f, 686.0f / 1024.0f, 37.0f / 1024.0f, 38.0f / 1024.0f, 0.2f);
		state.lasers.push_back(laser);
	}

	// Initialize meteors
	for (int i = 0; i < MAX_NUM_METEORS; i++) {
		Entity meteor;
		meteor.sprite = SheetSprite(spaceSpriteSheetTexture, 224.0f / 1024.0f, 664.0f / 1024.0f, 101.0f / 1024.0f, 84.0f / 1024.0f, 0.2f);
		state.meteors.push_back(meteor);
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

	mode = GAME_LEVEL; // Render the menu when the user opens the game
	//SetupMainMenu();
	SetupGameLevel();
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
	if (keys[SDL_SCANCODE_LEFT]) {
		state.player.velocity.x = -1.0f;
	}
	else if (keys[SDL_SCANCODE_RIGHT]) {
		state.player.velocity.x = 1.0f;
	}
	else {
		state.player.velocity.x = 0.0f;
	}

	// Allow the player to shoot lasers
	if (keys[SDL_SCANCODE_SPACE]) {
		
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
	}

	for (size_t i = 0; i < state.meteors.size(); i++) {
		state.meteors[i].Update(elapsed);
	}
	
	// Check for collisions

}

void RenderMainMenu() {
	
}

void RenderGameLevel() {
	state.player.Draw(texturedProgram);
	
	// Loop through entities and call their draw methods
	for (size_t i = 0; i < state.lasers.size(); i++) {
		state.lasers[i].Draw(texturedProgram);
	}
	for (size_t i = 0; i < state.meteors.size(); i++) {
		state.meteors[i].Draw(texturedProgram);
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
