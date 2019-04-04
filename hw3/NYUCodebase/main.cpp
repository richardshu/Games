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

//ShaderProgram program;		// For untextured polygons
ShaderProgram texturedProgram;  // For textured polygons

float lastFrameTicks = 0.0f;	// Set time to an initial value of 0
bool done = false;				// Game loop

glm::mat4 modelMatrix = glm::mat4(1.0f);

class SheetSprite {
public:
	SheetSprite();
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size);
	void Draw(ShaderProgram &program);

	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
	float size;
};

SheetSprite::SheetSprite() {}

SheetSprite::SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) {
	this->u = u;
	this->v = v;
	this->width = width;
	this->height = height;
	this->size = size;
}

void SheetSprite::Draw(ShaderProgram &program) {

}

class Entity {
public:
	// Methods
	void Draw(ShaderProgram &program);
	void Update(float elapsed);
	void SetColor(float r, float g, float b, float a);

	// Attributes
	SheetSprite sprite;
	int textureID;
	float x;
	float y;
	float width;
	float height;
	float rotation;
	float velocity;
	float direction_x;
	float direction_y;
	float r, g, b, a;
};

void Entity::Draw(ShaderProgram &program) {
	float vertices[] = { -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f };
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(x, y, 0.0f));
	program.SetModelMatrix(modelMatrix);
	program.SetColor(r, g, b, a);
	
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
}

void Entity::Update(float elapsed) {
	this->x += elapsed * direction_x;
	this->y += elapsed * direction_y;
}

void Entity::SetColor(float r, float g, float b, float a) {
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = a;
}

Entity playerSpaceship;
std::vector<Entity> lasers;
std::vector<Entity> meteors;

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

	glUseProgram(texturedProgram.programID);

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// "Clamps" down on a texture so that the pixels on the edge repeat
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glm::mat4 viewMatrix = glm::mat4(1.0f);
	glm::mat4 projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

	texturedProgram.SetModelMatrix(modelMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);
	texturedProgram.SetProjectionMatrix(projectionMatrix);

	glClearColor(1.0f, 1.0f, 0.88f, 1.0f); // Set background color to light yellow

	GLuint asciiSpriteSheetTexture = LoadTexture("ascii_spritesheet.png"); // Load ASCII sprite sheet
	GLuint spaceSpriteSheetTexture = LoadTexture("space_spritesheet.png"); // Load space sprite sheet

	// Initialize player spaceship
	playerSpaceship.sprite = SheetSprite(spaceSpriteSheetTexture, 237.0f / 1024.0f, 377.0f / 1024.0f, 99.0f, 75.0f, 0.2f);

	// Initialize lasers
	for (int i = 0; i < 30; i++) {
		Entity laser;
		laser.sprite = SheetSprite(spaceSpriteSheetTexture, 740.0f / 1024.0f, 686.0f / 1024.0f, 37.0f, 38.0f, 0.2f);
		lasers.push_back(laser);
	}
	
	// Initialize meteors
	for (int i = 0; i < 30; i++) {
		Entity meteor;
		meteor.sprite = SheetSprite(spaceSpriteSheetTexture, 224.0f / 1024.0f, 664.0f / 1024.0f, 101.0f / 1024.0f, 84.0f / 1024.0f, 0.2f);
		meteors.push_back(meteor);
	}
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
		playerSpaceship.direction_x = -1.0f;
	}
	else if (keys[SDL_SCANCODE_RIGHT]) {
		playerSpaceship.direction_x = 1.0f;
	}
	else {
		playerSpaceship.direction_x = 0.0f;
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

	for (size_t i = 0; i < meteors.size(); i++) {
		meteors[i].Update(elapsed);
	}

	// Check for collisions

}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);
	playerSpaceship.Draw(texturedProgram);

	// Loop through entities and call their draw methods
	for (size_t i = 0; i < lasers.size(); i++) {
		lasers[i].Draw(texturedProgram);
	}
	for (size_t i = 0; i < meteors.size(); i++) {
		meteors[i].Draw(texturedProgram);
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
