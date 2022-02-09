﻿#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
	#include <GLEW/glew.h>
#pragma clang diagnostic pop

#include <GLFW/glfw3.h>

#include"Vector.h"
#include"ShaderLoader.h"
#include"Chunks.h"
#include"ChunkCoord.h"

#include <iostream>
#include<chrono>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include"Read.h"
#include"Misc.h"
#include"PerlinNoise.h"

#include<vector>
#include<array>

#include"MeanCounter.h"


#include<limits>

# define PI 3.14159265358979323846

//#define FULLSCREEN

#ifdef FULLSCREEN
static const vec2<uint32_t> windowSize{ 1920, 1080 };
#else
static const vec2<uint32_t> windowSize{ 1000, 600 };
#endif // FULLSCREEN

static const vec2<double> windowSize_d{ windowSize.convertedTo<double>() };

static vec2<double> mousePos(0, 0), pmousePos(0, 0);

template<typename El, size_t i_, size_t j_> 
static inline void copy2DArray(El const (&in)[i_][j_], El (&out)[i_][j_]) {
	for(size_t i = 0; i < i_; ++i) {
		for(size_t j = 0; j < j_; ++j) {
			out[i][j] = in[i][j];
		}
	}
}

struct Viewport {
    vec2<double> rotation{};
	double aspectRatio;// height / width
	double fov;
	double near, far;

    constexpr vec3<double> rightDir() const {
        return vec3<double>(cos(rotation.x), 0, sin(rotation.x));
    }
    constexpr vec3<double> topDir() const {
        return vec3<double>(-sin(rotation.x) * sin(rotation.y), cos(rotation.y), cos(rotation.x) * sin(rotation.y));
    }
    constexpr vec3<double> forwardDir() const {
        return topDir().cross(rightDir());
		//return vec3<double>(cos(rotation.y) * sin(rotation.x), sin(rotation.y), -cos(rotation.y) * cos(rotation.x));
    }
	
	constexpr vec3<double> flatRightDir() const {
        return vec3<double>(cos(rotation.x), 0, sin(rotation.x));
    }
    constexpr vec3<double> flatTopDir() const {
        return vec3<double>(0, 1, 0);
    }
    constexpr vec3<double> flatForwardDir() const {
        return flatTopDir().cross(flatRightDir());
		//return vec3<double>(cos(rotation.y) * sin(rotation.x), sin(rotation.y), -cos(rotation.y) * cos(rotation.x));
    }
	
	template<typename O, typename = std::enable_if_t<std::is_convertible<double, O>::value>>
	void localToGlobalSpace(O (*mat_out)[3][3]) const {
		auto const rd = rightDir();
		auto const td = topDir();
		auto const fd = forwardDir();
		
		O const pm[3][3] = {
			{ static_cast<O>(rd.x), static_cast<O>(td.x), static_cast<O>(fd.x) },
			{ static_cast<O>(rd.y), static_cast<O>(td.y), static_cast<O>(fd.y) },
			{ static_cast<O>(rd.z), static_cast<O>(td.z), static_cast<O>(fd.z) }
		}; //rotMatrix after scaleMatrix
		copy2DArray<O, 3, 3>(
			pm,
			*mat_out
		);
	}
	
	template<typename O, typename = std::enable_if_t<std::is_convertible<double, O>::value>>
	void globalToLocalSpace(O (*mat_out)[3][3]) const {
		auto const rd = rightDir();
		auto const td = topDir();
		auto const fd = forwardDir();
		
		O const pm[3][3] = {
			{ static_cast<O>(rd.x), static_cast<O>(rd.y), static_cast<O>(rd.z) },
			{ static_cast<O>(td.x), static_cast<O>(td.y), static_cast<O>(td.z) },
			{ static_cast<O>(fd.x), static_cast<O>(fd.y), static_cast<O>(fd.z) }
		};
		copy2DArray<O, 3, 3>(
			pm,
			*mat_out
		);
	}
	
	template<typename O, typename = std::enable_if_t<std::is_convertible<double, O>::value>>
	void projectionMatrix(O (*mat_out)[4][4]) const {
		O const pm[4][4] = {
			{ static_cast<O>(aspectRatio*fov), static_cast<O>(0.0), static_cast<O>(0), static_cast<O>(0.0) },
			{ static_cast<O>(0.0), static_cast<O>(fov), static_cast<O>(0.0), static_cast<O>(0.0) },
			{ static_cast<O>(0.0), static_cast<O>(0.0), static_cast<O>(far/(far-near)), static_cast<O>(-(far*near)/(far-near)) },
			{ static_cast<O>(0.0), static_cast<O>(0.0), static_cast<O>(1.0), static_cast<O>(0.0) }
		};
		copy2DArray<O, 4, 4>(
			pm,
			*mat_out
		);
	}
};

static bool isPan{ false };
static bool isZoomMovement{ false };
static double zoomMovement{ 0 };
static double size = 1;

static bool shift{ false }, ctrl{ false };

static vec3<double> movementDir{};
static double speedModifier = 2.5;

static bool debugChunkBorder{ false };

static double deltaTime{ 16.0/1000.0 };

static double movementSpeed{ 8 };
static bool jump{ false };

static bool isOnGround{false};
static vec3d playerForce{};
static ChunkCoord player{ vec3i{0,0,0}, vec3d{0,15,0}};
static Viewport viewport{ 
		vec2d{ misc::pi / 2.0, 0 },
		windowSize_d.y / windowSize_d.x,
		90.0 / 180.0 * misc::pi,
		0.001,
		1000
};
static vec3d const viewportOffset{0,2,0};
static bool isFreeCam{ false };

static Viewport& viewport_current() {
    return viewport;
}

static void reloadShaders();

static bool testInfo = false;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    bool isPress = !(action == GLFW_RELEASE);
	if(key == GLFW_KEY_W)
		movementDir.z = 1 * isPress;
	else if(key == GLFW_KEY_S)
		movementDir.z = -1 * isPress;
	
	if(isFreeCam) {
		if (key == GLFW_KEY_Q)
			movementDir.y = 1 * isPress;
		else if(key == GLFW_KEY_E)
			movementDir.y = -1 * isPress;
	}
	else
		if (key == GLFW_KEY_SPACE) 
			jump |= isOnGround && isPress;

	if(key == GLFW_KEY_D)
		movementDir.x = 1 * isPress;
	else if(key == GLFW_KEY_A)
		movementDir.x = -1 * isPress;
	
	if(key == GLFW_KEY_F5 && action == GLFW_PRESS)
		reloadShaders();
	else if(key == GLFW_KEY_F4 && action == GLFW_PRESS)
		debugChunkBorder = !debugChunkBorder;
	else if(key == GLFW_KEY_F3 && action == GLFW_PRESS)
		isFreeCam = !isFreeCam;
	else if(key == GLFW_KEY_TAB && action == GLFW_PRESS) testInfo = true;

    shift = (mods & GLFW_MOD_SHIFT) != 0;
    ctrl = (mods & GLFW_MOD_CONTROL) != 0;
}

static void cursor_position_callback(GLFWwindow* window, double mousex, double mousey) {
    static vec2<double> relativeTo{ 0, 0 };
    vec2<double> mousePos_{ mousex,  mousey };
    
    relativeTo += mousePos_;
    mousePos_.x = misc::modf(mousePos_.x, windowSize_d.x);
    mousePos_.y = misc::modf(mousePos_.y, windowSize_d.y);
    relativeTo -= mousePos_;

    glfwSetCursorPos(window, mousePos_.x, mousePos_.y);

    mousePos = vec2<double>(relativeTo.x + mousePos_.x, -relativeTo.y + windowSize_d.y - mousePos_.y);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if(button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            isPan = true;
        }
        else if (action == GLFW_RELEASE) {
            isPan = false;
        }
	}
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            zoomMovement = 0;
            isZoomMovement = true;
        }
        else if (action == GLFW_RELEASE) {
            isZoomMovement = false;
            zoomMovement = 0;
        }
	}
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    size -= size * yoffset * 0.07;
}

static GLuint rightDir_u;
static GLuint topDir_u;
static GLuint time_u;
static GLuint mouseX_u, mouseY_u;
static GLuint relativeChunkPos_u;
static GLuint projection_u, db_projection_u, model_matrix_u, db_model_matrix_u;

static GLuint chunkIndex_u;

static GLuint bgProjection_u;
static GLuint bgRightDir_u;
static GLuint bgTopDir_u;

static GLuint mainProgram = 0;
static GLuint debugProgram = 0;
static GLuint bgProgram = 0;

static void reloadShaders() {
	glDeleteProgram(mainProgram);
	mainProgram = glCreateProgram();
    ShaderLoader sl{};

	//sl.addShaderFromProjectFileName("shaders/vertex.shader",GL_VERTEX_SHADER,"main vertex");
	//https://gist.github.com/rikusalminen/9393151
	//... usage: glDrawArrays(GL_TRIANGLES, 0, 36), disable all vertex arrays
	sl.addShaderFromCode(
		R"(#version 420
		uniform mat4 projection;
		uniform mat4 model_matrix;
		//uniform vec3 translation;
		
		void main()
		{
			int tri = gl_VertexID / 3;
			int idx = gl_VertexID % 3;
			int face = tri / 2;
			int top = tri % 2;
		
			int dir = face % 3;
			int pos = face / 3;
		
			int nz = dir >> 1;
			int ny = dir & 1;
			int nx = 1 ^ (ny | nz);
		
			vec3 d = vec3(nx, ny, nz);
			float flip = 1 - 2 * pos;
		
			vec3 n = flip * d;
			vec3 u = -d.yzx;
			vec3 v = flip * d.zxy;
		
			float mirror = -1 + 2 * top;
			vec3 xyz = n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;
			xyz = (xyz + 1) / 2 * 16;
		
			gl_Position = projection * (model_matrix * vec4(xyz, 1.0));
		}
		)",
		GL_VERTEX_SHADER,
		"main vertex"
	);
    sl.addShaderFromProjectFileName("shaders/main.shader", GL_FRAGMENT_SHADER, "Main shader");

    sl.attachShaders(mainProgram);

    glLinkProgram(mainProgram);
    glValidateProgram(mainProgram);

    sl.deleteShaders();

    glUseProgram(mainProgram);
	
	glUniform2ui(glGetUniformLocation(mainProgram, "windowSize"), windowSize.x, windowSize.y);
    glUniform2f(glGetUniformLocation(mainProgram, "atlasTileCount"), 512 / 16, 512 / 16);

    time_u   = glGetUniformLocation(mainProgram, "time");
    mouseX_u = glGetUniformLocation(mainProgram, "mouseX");
    mouseY_u = glGetUniformLocation(mainProgram, "mouseY");


    rightDir_u = glGetUniformLocation(mainProgram, "rightDir");
    topDir_u = glGetUniformLocation(mainProgram, "topDir");
	projection_u = glGetUniformLocation(mainProgram, "projection");
	model_matrix_u = glGetUniformLocation(mainProgram, "model_matrix");
	
	//images
    GLuint textures[1];
    char const* (paths)[1]{ "assets/atlas.bmp"
    };
    loadGLImages<1>(textures, paths);

    GLuint const atlasTex_u = glGetUniformLocation(mainProgram, "atlas");

    glUniform1i(atlasTex_u, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);

    /*GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(field), field, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);*/
	
	/*GLuint chunkIndex_b = glGetUniformBlockIndex(mainProgram, "Chunk");
	glGenBuffers(1, &chunkUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, chunkUBO);
	glUniformBlockBinding(mainProgram, chunkIndex_b, 1);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, chunkUBO);
	glBufferData(GL_UNIFORM_BUFFER, 12, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);*/
	
	relativeChunkPos_u = glGetUniformLocation(mainProgram, "relativeChunkPos");
	
    glGenBuffers(1, &chunkIndex_u);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkIndex_u);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 8192, NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkIndex_u);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	
	glDeleteProgram(bgProgram);
	bgProgram = glCreateProgram();
	ShaderLoader bgsl{};
	bgsl.addScreenSizeTriangleStripVertexShader("bg vertex");
	bgsl.addShaderFromCode(R"(#version 430
		in vec4 gl_FragCoord;
		out vec4 color;
		
		uniform mat4 projection; //from local space to screen
		uniform uvec2 windowSize;
		
		uniform vec3 rightDir, topDir;
		
		vec3 background(const vec3 dir) {
			const float t = 0.5 * (dir.y + 1.0);
			return (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
		}
		void main() {
			const vec2 coord = (gl_FragCoord.xy - windowSize.xy / 2) * 2 / windowSize.xy;
	
			const vec3 forwardDir = cross(topDir, rightDir);
			const vec3 rayDir_ = rightDir * coord.x / projection[0].x + topDir * coord.y / projection[1].y + forwardDir;
			const vec3 rayDir = normalize(rayDir_);

			color = vec4(background(rayDir), 1.0);
		}
	
		)",
		GL_FRAGMENT_SHADER,
		"bg fragment"
	);
	
	bgsl.attachShaders(bgProgram);

    glLinkProgram(bgProgram);
    glValidateProgram(bgProgram);
	glUseProgram(bgProgram);
	

    bgsl.deleteShaders();
	
	glUniform2ui(glGetUniformLocation(bgProgram, "windowSize"), windowSize.x, windowSize.y);
	bgProjection_u = glGetUniformLocation(bgProgram, "projection");
	bgRightDir_u = glGetUniformLocation(bgProgram, "rightDir");
	bgTopDir_u = glGetUniformLocation(bgProgram, "topDir");
	
	
	float projection[4][4];
	viewport_current().projectionMatrix(&projection);
	
	{
	glDeleteProgram(debugProgram);
	debugProgram = glCreateProgram();
	ShaderLoader dbsl{};

	//sl.addShaderFromProjectFileName("shaders/vertex.shader",GL_VERTEX_SHADER,"main vertex");
	//https://gist.github.com/rikusalminen/9393151
	//... usage: glDrawArrays(GL_TRIANGLES, 0, 36), disable all vertex arrays
    dbsl.addShaderFromCode(R"(#version 420
		uniform mat4 projection;
		uniform mat4 model_matrix;
		
		void main()
		{
			int tri = gl_VertexID / 3;
			int idx = gl_VertexID % 3;
			int face = tri / 2;
			int top = tri % 2;
		
			int dir = face % 3;
			int pos = face / 3;
		
			int nz = dir >> 1;
			int ny = dir & 1;
			int nx = 1 ^ (ny | nz);
		
			vec3 d = vec3(nx, ny, nz);
			float flip = 1 - 2 * pos;
		
			vec3 n = flip * d;
			vec3 u = -d.yzx;
			vec3 v = flip * d.zxy;
		
			float mirror = -1 + 2 * top;
			vec3 xyz = n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;
			xyz = (xyz + 1) / 2 * 16;
		
			gl_Position = projection * (model_matrix * vec4(xyz, 1.0));
		}
	)", GL_VERTEX_SHADER, "debug vertex");
	dbsl.addShaderFromCode(
		R"(#version 420
		out vec4 color;
		void main() {
			color = vec4(1, 0, 0, 1);
		})",
		GL_FRAGMENT_SHADER, "debug shader"
	);

    dbsl.attachShaders(debugProgram);

    glLinkProgram(debugProgram);
    glValidateProgram(debugProgram);

    dbsl.deleteShaders();

    glUseProgram(debugProgram);
	
	db_model_matrix_u = glGetUniformLocation(debugProgram, "model_matrix");
	db_projection_u = glGetUniformLocation(debugProgram, "projection");
	}
	
	glUseProgram(bgProgram);
	glUniformMatrix4fv(bgProjection_u, 1, GL_TRUE, &projection[0][0]);
	
	glUseProgram(debugProgram);
	glUniformMatrix4fv(db_projection_u, 1, GL_TRUE, &projection[0][0]);
	
	glUseProgram(mainProgram);
	glUniformMatrix4fv(projection_u, 1, GL_TRUE, &projection[0][0]);
}

template<typename T, typename L>
inline void apply(size_t size, T &t, L&& l) {
	for(size_t i = 0; i < size; ++i) 
		if(l(t[i], i) == true) break;
}

double sign(double const val) {
    return (0.0 < val) - (val < 0.0);
}

Chunks chunks{};

siv::PerlinNoise perlin{ (uint32_t)rand() };
static int viewDistance = 3;

void generateChunk(vec3<int> const pos, Chunks::ChunkData &data) {
	for(int y = 0; y < Chunks::chunkDim; ++y) 
		for(int z = 0; z < Chunks::chunkDim; ++z)
			for(int x = 0; x < Chunks::chunkDim; ++x) {
				auto const value = perlin.octave2D(
					(pos.x * Chunks::chunkDim + x) / 20.0, 
					(pos.z * Chunks::chunkDim  + z) / 20.0, 
					3
				);
				auto const height = misc::map<double>(value, -1, 1, 5, 15);
				auto const index{ Chunks::blockIndex(vec3<int32_t>{x, y, z}) };
				if(height > (pos.y * Chunks::chunkDim + y)) {
					data[index] = 1;
				}
				else {
					data[index] = 0;
				}
			}
}

static void loadChunks() {
	static std::vector<int8_t> chunksPresent{};
	auto const viewWidth = (viewDistance*2+1);
	chunksPresent.resize(viewWidth*viewWidth*viewWidth);
	for(size_t i = 0; i != chunksPresent.size(); i++)
		chunksPresent[i] = -1;
	
	auto const currentViewport{ viewport_current() };
	vec3<int32_t> playerChunk{ player.chunk() };
	
	vec3<int32_t> b{viewDistance, viewDistance, viewDistance};
	
	chunks.filterUsed([&](int chunkIndex) -> bool { 
		auto const relativeChunkPos = chunks.chunksPosition()[chunkIndex] - playerChunk;
		if(relativeChunkPos.in(-b, b)) {
			auto const index2 = relativeChunkPos + b;
			chunksPresent[index2.x + index2.y * viewWidth  + index2.z * viewWidth * viewWidth] = 1;
			
			return true;
		} 
		return false;
	});
	
	for(int i = 0; i < viewWidth; i ++) {
		for(int j = 0; j < viewWidth; j ++) {
			for(int k = 0; k < viewWidth; k ++) {
				int index = chunksPresent[i+j*viewWidth+k*viewWidth*viewWidth];
				if(index == -1) {//generate chunk
					vec3<int32_t> const relativeChunkPos{ vec3<int32_t>{i, j, k} - b };
					vec3<int32_t> const chunkPos{ relativeChunkPos + playerChunk };
				
					auto const chunkIndex{ chunks.usedChunks()[chunks.reserve()] };
					
					chunks.chunksPosition()[chunkIndex] = chunkPos;
					generateChunk(chunkPos, chunks.chunksData()[chunkIndex]);
				}
			}
		}
	}
}

/*static float intersectPlane(const Ray r, const vec3 center, const vec3 n) {
    return dot(n, center - r.orig) / dot(n, r.dir);
}

static float intersectSquare(const Ray r, const vec3 center, const vec3 n, const vec3 up, const vec3 left, const float radius, inout vec2 uv) {
    const vec3 c = center + n * radius; //plane center
    const float t = intersectPlane(r, c, n);
    const vec3 p = at(r, t); //point
    vec3 l = p - c; //point local to plane
    float u_ = dot(l, up);
    float v_ = dot(l, left);
    if (abs(u_) <= radius && abs(v_) <= radius) {
        uv = (vec2(u_, v_) / 2 / radius + 0.5) * eps1m;
        return t;
    }
    return 1.0 / 0.0;
}*/

static void updateCollision(ChunkCoord &player, vec3d &playerForce, bool &isOnGround, int const remaining = 256) {
	if(remaining == 0) { std::cout << "error\n"; std::cout.precision(17); std::cout << player.inChunkPosition() << ' ' << playerForce << '\n'; std::cout.precision(2); return; }
	static std::vector<vec3i> checkCollision{}; //inChunk positions of blocks (relative to playerChunk) that need to be checked
	checkCollision.clear();
	auto const botChunk{ player.chunk() };
	auto const botInChunk{ player.inChunkPosition() };
	auto const height{ 2 };
	auto const rad{ 0.5 };
	
	vec3d const line{ playerForce };
	float const lineLen = line.length();
	vec3d const dir = line.normalized();
	vec3i const dir_{ static_cast<vec3i>(dir.sign()) };
	vec3i const positive_ = (+dir_).max(vec3i{0});
	vec3i const negative_ = (-dir_).max(vec3i{0});
	
	vec3d const lineEnd{botInChunk + dir * lineLen};
		  
	vec3d const stepLength = vec3d{1.0} / dir.abs();
		  
	vec3d const firstCellRow_f = (botInChunk + dir_).floor();
	vec3i const firstCellRow{static_cast<vec3i>(firstCellRow_f)}; 
	vec3d const firstCellDiff  = (botInChunk - firstCellRow_f - negative_).abs(); //distance to the frist border

	vec3i curSteps{0};
	vec3d curLen = stepLength * firstCellDiff;
	
	int i = 0;
	
	double nextMinLength = 0;
	for(; i < 100; i ++) {
		double minCurLen = nextMinLength;
		
		bool const end = minCurLen >= lineLen;
		if(end) { minCurLen = lineLen; curLen = lineLen; }
		
		vec3b const minAxis_b = curLen.equal(vec3d(minCurLen));
		vec3d const minAxis_f{ static_cast<vec3d>(minAxis_b) };
		vec3i const minAxis_i{ static_cast<vec3i>(minAxis_b) };
		
		vec3i const otherAxis_i{ static_cast<vec3i>(minAxis_b.lnot()) };
		vec3d const curCoordF = botInChunk + dir * minCurLen;
		vec2<double> const botXZ{ curCoordF.x, curCoordF.z };
		auto const minXZ{ botXZ - rad };
		auto const maxXZ{ botXZ + rad };
			
		for(int y = floor(curCoordF.y); y <= floor(curCoordF.y+height); y++)
			for(int x = floor(minXZ.x); x <= floor(maxXZ.x); x++)
				for(int z = floor(minXZ.y); z <= floor(maxXZ.y); z++) {
					vec3i const coord{x,y,z};
					
					if(checkCollision.end() == std::find(checkCollision.begin(), checkCollision.end(), coord)) {
						checkCollision.push_back(coord);
					}
				}
		
		if(end) break;
		
		curSteps += minAxis_i;
		curLen += minAxis_f * stepLength;

		nextMinLength = fmin(fmin(curLen.x, curLen.y), curLen.z);
	}
	if(i == 100) { 
		std::cout << "i is huge!\n";
	}

	ChunkCoord newPlayer{ player + line };
	vec3d newForce{ playerForce };
	double newPlayerDist = lineLen;
	bool isCollision{ false };
	char const *side = "";
	
	
	for(auto const &coord : checkCollision) {
		ChunkCoord const coord_{ botChunk, coord };
		int pos = -1;
		for(auto const chunkIndex : chunks.usedChunks())
			if(chunks.chunksPosition()[chunkIndex] == coord_.chunk()) { pos = chunkIndex; break; }
		
		if(pos == -1) { std::cout << "add chunk gen!\n"; continue; }
		auto const chunkData{ chunks.chunksData()[pos] };
		auto const index{ Chunks::blockIndex(coord_.inChunkPosition()) };
		uint16_t const &block{ chunkData[index] };
		if(block != 0) {
			auto const center{ static_cast<vec3d>(coord) + 0.5 };
			vec3i const normals{ (botInChunk - center).sign() };
			auto const facesCoord{ center + vec3d(normals)*0.5 };
			auto const localFacesCoord{ facesCoord - botInChunk };
			double distToTop{ abs(localFacesCoord.y) * stepLength.y }; //distance along the force
			
			vec2d const flatBotInChunk{ botInChunk.x, botInChunk.z };
			vec2d const flatDir{ vec2d{dir.x,dir.z}.normalized() };
			
			vec3d const normalY = vec3d{0,double(normals.y),0};
			double const strengthY{  normalY.dot(-playerForce) };
			if(distToTop >= -0.0 && distToTop <= newPlayerDist && strengthY > 0) { //y
				auto const coordAtTop{ botInChunk + dir*distToTop };
				vec2d closestPointTop{
					misc::clamp<double>(coordAtTop.x, coord.x, coord.x+1),
					misc::clamp<double>(coordAtTop.z, coord.z, coord.z+1)
				};
				bool isIntersection{ (vec2d{coordAtTop.x, coordAtTop.z}-closestPointTop).lengthSquare() < rad*rad };
				if(isIntersection) {
					isCollision = true;
					newPlayerDist = distToTop;
					vec3d v{botInChunk + dir * distToTop};
					newPlayer = ChunkCoord(player.chunk(), v);
					
	
					newForce = (playerForce + normalY * strengthY) * vec3d{0.6, 1, 0.6};
					
					isOnGround = true;

				}
			}
			
			{ //x
				auto  newT = (abs(localFacesCoord.x) - rad) * stepLength.x;
				vec3d normal{ normals.x+0.0, 0, 0 };
				vec3d v{botInChunk + dir * newT};
				

				bool checkZ = v.z >= coord.z && v.z < (coord.z + 1);
	
				if(!checkZ) {
					vec2d minEdge;
					vec2d toMinEdge;
					double minDist, minT;
					
					vec2d const edge1{ vec2d{ facesCoord.x, coord.z+0.0 }};
					vec2d const edge2{ vec2d{ facesCoord.x, coord.z+1.0 }};
					vec2d const toEdge1{ edge1 - flatBotInChunk };
					vec2d const toEdge2{ edge2 - flatBotInChunk };
					
					
					auto const minDist1{ abs(flatDir.cross(toEdge1)) };
					auto const minDist2{ abs(flatDir.cross(toEdge2)) };
					
					auto const minT1{ flatDir.dot(toEdge1) };
					auto const minT2{ flatDir.dot(toEdge2) };
					
					
					if(minDist1 < minDist2) { minT = minT1; minEdge = edge1; toMinEdge = toEdge1; minDist = minDist1; }
					else					{ minT = minT2; minEdge = edge2; toMinEdge = toEdge2; minDist = minDist2; }
					
					if(minDist >= rad) goto next; //no collision
					
					double const dt{sqrt(rad*rad - minDist*minDist)};
					
					newT = minT - dt;
					v = botInChunk + dir * newT;
					vec2d const newToMinEdge{ minEdge - vec2d{v.x, v.z} };
					normal = vec3d{ -newToMinEdge.x, 0, -newToMinEdge.y }.normalized();
					
					checkZ = true;
				}
				
				bool const checkY = (coord.y < v.y+height) && (coord.y+1 > v.y);
				
				double const strengthX{  normal.dot(-playerForce) };
				if(checkZ && checkY && newT >= -0.00 && newT <= newPlayerDist && strengthX > 0) {
					isCollision = true;
					newPlayerDist = newT;
					newPlayer = ChunkCoord(player.chunk(), v);
	
					newForce = playerForce + normal * strengthX;
					//newForce = playerForce + normal * 2 * strengthX; //bounce off
					
					side = "x";
				}
				
			}
			
			next:
			
			{ //z
				auto  newT = (abs(localFacesCoord.z) - rad) * stepLength.z;
				vec3d normal{ 0, 0, normals.z+0.0 };
				vec3d v{botInChunk + dir * newT};

				bool checkX = v.x >= coord.x && v.x < (coord.x + 1);
				
				if(!checkX) {
					vec2d minEdge;
					vec2d toMinEdge;
					double minDist, minT;
					
					vec2d const edge1{ vec2d{ coord.x+0.0, facesCoord.z }};
					vec2d const edge2{ vec2d{ coord.x+1.0, facesCoord.z }};
					vec2d const toEdge1{ edge1 - flatBotInChunk };
					vec2d const toEdge2{ edge2 - flatBotInChunk };
					
					auto const minDist1{ abs(flatDir.cross(toEdge1)) };
					auto const minDist2{ abs(flatDir.cross(toEdge2)) };
					
					auto const minT1{ flatDir.dot(toEdge1) };
					auto const minT2{ flatDir.dot(toEdge2) };
					
					
					if(minDist1 < minDist2) { minT = minT1; minEdge = edge1; toMinEdge = toEdge1; minDist = minDist1; }
					else					{ minT = minT2; minEdge = edge2; toMinEdge = toEdge2; minDist = minDist2; }
					
					if(minDist >= rad) goto next2; //no collision
					
					double const dt{sqrt(rad*rad - minDist*minDist)};
					
					newT = minT - dt;
					v = botInChunk + dir * newT;
					vec2d const newToMinEdge{ minEdge - vec2d{v.x, v.z} };
					normal = vec3d{ -newToMinEdge.x, 0, -newToMinEdge.y }.normalized();
					
					checkX = true;
				}
				
				bool const checkY = (coord.y < v.y+height) && (coord.y+1 > v.y);
				
				double const strengthZ{  normal.dot(-playerForce) };
				if(checkX && checkY && newT >= -0.00 && newT <= newPlayerDist && strengthZ > 0) {
					isCollision = true;
					newPlayerDist = newT;
					newPlayer = ChunkCoord(player.chunk(), v);
	
					newForce = playerForce + normal * strengthZ;
					//newForce = playerForce + normal * 2 * strengthZ; //bounce off
				
					side = "z";
				}
				
			}
			
			next2:;
		}
	}
	
	player = newPlayer;
	playerForce = newForce;
	
	if(!isCollision || newForce.lengthSquare() == 0);
	else updateCollision(player, playerForce, isOnGround, remaining-1);
}

static void update() {
    auto& currentViewport = viewport_current();

    vec2<double> diff = (mousePos - pmousePos) / windowSize_d;
	if(isFreeCam) playerForce = 0;
    if (isZoomMovement && isFreeCam) {
        zoomMovement += diff.x;

        const auto forwardDir = currentViewport.forwardDir();
		playerForce += forwardDir * zoomMovement * deltaTime * size;
    }
    if (isPan) {
        currentViewport.rotation += diff * (2 * misc::pi);
        currentViewport.rotation.y = misc::clamp(currentViewport.rotation.y, -misc::pi / 2 + 0.001, misc::pi / 2 - 0.001);
    }
    if (movementDir.lengthSquare() != 0) {
		vec3<double> movement;
		if(isFreeCam) {
			double projection[3][3];
			currentViewport.localToGlobalSpace(&projection);
			
			movement = vecMult(projection, movementDir);
		}
		else {
			movement = currentViewport.flatForwardDir()*movementDir.z + currentViewport.flatTopDir()*movementDir.y + currentViewport.flatRightDir()*movementDir.x;
		}
		
		vec3d const movementForce{ movement.normalized() * movementSpeed * deltaTime
				* (shift ? 1.0*speedModifier : 1)
				* (ctrl  ? 1.0/speedModifier : 1) };
		playerForce = playerForce.applied([&](double const coord, auto const index) -> double { 
			return misc::clamp(coord + movementForce[index], fmin(coord, movementForce[index]), fmax(coord, movementForce[index]));
		});
    }
	
	if(!isFreeCam) { playerForce+=vec3d{0,-1,0}*deltaTime; }
	if(!isFreeCam && jump) { playerForce+=vec3d{0,1,0}*16*deltaTime; }
	jump = false;
	
	isOnGround = false;
	
	if(!isFreeCam) {
		updateCollision(player, playerForce, isOnGround);
	}
	else {
		player += playerForce;
		playerForce = 0;
	}
	
	loadChunks();

    pmousePos = mousePos;
}

/*static std::array<std::array<double, 4>, 8> projectChunk(double const (&projectionMatrix)[4][4], double const (&chunkToLocal)[4][4], int &projectionVertsCount) { //the function returns convex hull of projected chunk
	static constexpr std::array<vec3<double>,8> const verts {
		vec3<double>{ 0, 0, 0 }, vec3<double>{ 0, 0, Chunks::chunkDim }, vec3<double>{ 0, Chunks::chunkDim, 0 }, vec3<double>{ 0, Chunks::chunkDim, Chunks::chunkDim }, 
		vec3<double>{ Chunks::chunkDim, 0, 0 }, vec3<double>{ Chunks::chunkDim, 0, Chunks::chunkDim }, vec3<double>{ Chunks::chunkDim, Chunks::chunkDim, 0 }, vec3<double>{ Chunks::chunkDim, Chunks::chunkDim, Chunks::chunkDim }, 
	};
	
	std::array<vec2<double>,8> projectedVerts{};
	std::array<std::array<double, 4>, 8> projectedVertsOrigs{};
	for(int i{0}; i < verts.size(); ++i) {
		double vert_1[4][1] { verts[i].x, verts[i].y, verts[i].z, 1 };
		double vert_2[4][1];
		
		misc::matMult(chunkToLocal, vert_1, &vert_2);
		misc::matMult(projectionMatrix, vert_2, &vert_1);

		projectedVerts[i] = vec2<double>{ vert_1[0][0], vert_1[1][0] } / vert_1[3][0];
		projectedVertsOrigs[i] = { vert_1[0][0], vert_1[1][0], vert_1[2][0], vert_1[3][0] };
	}
	
	std::array<std::array<double, 4>, 8> convexHull{};
	
	{ //first vertex must be part of the convex hull
		auto minX{ std::numeric_limits<double>::infinity() }; 
		int minI{ -1 };
		
		for(int i{0}; i < projectedVerts.size(); ++i) {
			auto const curX{ projectedVerts[i].x };
			if(curX < minX) {
				minX = curX;
				minI = i;
			}
		}
		
		if(minI == -1) { //something went horribly wrong
			projectionVertsCount = 0;
			return convexHull;
		}
		std::swap(projectedVerts[0], projectedVerts[minI]); 
		std::swap(projectedVertsOrigs[0], projectedVertsOrigs[minI]); 
		convexHull[0] = projectedVertsOrigs[0];
	}
	
	
	
	for(int projectionIndex{1}; projectionIndex < convexHull.size(); ++projectionIndex) { 
		vec2<double> const prev{ projectedVerts[projectionIndex-1] };
		
		vec2<double> secondCandidate;
		vec2<double> toCand;
		for(int i = projectionIndex; i < projectedVerts.size(); ++i) {
			secondCandidate = { projectedVerts[i] };
			toCand = { secondCandidate - prev };
			
			for(int j{projectionIndex}; j < projectedVerts.size(); j ++) {
				if(j == i) continue;
				auto const otherVerex{ projectedVerts[j] };
				auto const toOther{ otherVerex - prev };
			
				if(toCand.cross(toOther) < -0.0) goto wrongVertex; //all (remaining) vertices must lie on the same side of the line
			}
			
			vec2<double> const first{ projectedVerts[0] };
			auto const toFirst{ first - prev };
			if(toCand.cross(toFirst) < -0.0) { projectionVertsCount = projectionIndex; return convexHull; };
			
			std::swap(projectedVerts[projectionIndex], projectedVerts[i]); //move all vertices that define projected chunk boundary to the beginning of the array
			std::swap(projectedVertsOrigs[projectionIndex], projectedVertsOrigs[i]); //move all vertices that define projected chunk boundary to the beginning of the array
			convexHull[projectionIndex] = projectedVertsOrigs[projectionIndex];
			
			//break;
			
			
			wrongVertex: continue;
		}
	}
	
	projectionVertsCount = 8;
	return convexHull;
}
*/

int main(void) {
    GLFWwindow* window;

    if (!glfwInit())
        return -1;

    GLFWmonitor* monitor;
#ifdef FULLSCREEN
    monitor = glfwGetPrimaryMonitor();
#else
    monitor = NULL;
#endif // !FULLSCREEN

    window = glfwCreateWindow(windowSize.x, windowSize.y, "VMC", monitor, NULL);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }
	
	glEnable(GL_CULL_FACE); 
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
    fprintf(stdout, "Using GLEW %s\n", glewGetString(GLEW_VERSION));

    //callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);


	//load shaders
	reloadShaders();
	
	loadChunks();
	
	glUseProgram(mainProgram);
	
	
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(std::array<std::array<float, 7>, 16>{}), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	GLuint vao;
	glGenVertexArrays(1, &vao);
	
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 7, nullptr);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 7, (void*) (sizeof(GLfloat) * 4));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0); 
	
	while ((err = glGetError()) != GL_NO_ERROR)
    {
        std::cout << err << std::endl;
    }

    auto start = std::chrono::steady_clock::now();
	
	MeanCounter<150> mc{};
	int _i_ = 50;
	
    while (!glfwWindowShouldClose(window))
    {
		auto startFrame = std::chrono::steady_clock::now();
		
		auto &currentViewport = viewport_current();
        auto const rightDir{ currentViewport.rightDir() };
        auto const topDir{ currentViewport.topDir() };
		
		//glClear(GL_COLOR_BUFFER_BIT);
		
		glUseProgram(bgProgram);
		glUniform3f(bgRightDir_u, rightDir.x, rightDir.y, rightDir.z);
        glUniform3f(bgTopDir_u, topDir.x, topDir.y, topDir.z);
		glCullFace(GL_FRONT); 
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
		
		
		glUseProgram(mainProgram);
        glUniform3f(rightDir_u, rightDir.x, rightDir.y, rightDir.z);
        glUniform3f(topDir_u, topDir.x, topDir.y, topDir.z);

        glUniform1f(mouseX_u, mousePos.x / windowSize_d.x);
        glUniform1f(mouseY_u, mousePos.y / windowSize_d.y);

        glUniform1f(time_u,
            sin(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count()
            )
        );
		
		
		float toLoc[3][3];
		double toGlob[3][3];
		currentViewport.localToGlobalSpace(&toGlob);
		currentViewport.globalToLocalSpace(&toLoc);
		vec3<int32_t> const playerChunk{ player.chunk() };
		auto const cameraPosInChunk{ player.inChunkPosition()+viewportOffset };
		
		if(testInfo) {
					std::cout << (mousePos.x / windowSize_d.x) << ' '
		                      << (mousePos.y / windowSize_d.y) << '\n'
							  << cameraPosInChunk << '\n';
		}
		
		float const toLoc4[4][4] = {
			{ toLoc[0][0], toLoc[0][1], toLoc[0][2], 0 },
			{ toLoc[1][0], toLoc[1][1], toLoc[1][2], 0 },
			{ toLoc[2][0], toLoc[2][1], toLoc[2][2], 0 },
			{ 0          , 0          , 0          , 1 },
		};
		
		struct ChunkSort {
			double sqDistance;
			int chunkIndex;
		};
		std::vector<ChunkSort> chunksSorted{};
		chunksSorted.reserve(chunks.usedChunks().size());
		for(auto const chunkIndex : chunks.usedChunks()) {
			auto const chunkPos{ chunks.chunksPosition()[chunkIndex] };
			vec3<double> const relativeChunkPos{ 
				static_cast<decltype(cameraPosInChunk)>(chunkPos-playerChunk)*Chunks::chunkDim
				+(Chunks::chunkDim/2.0f)
				-cameraPosInChunk
			};
			chunksSorted.push_back({ relativeChunkPos.lengthSquare(), chunkIndex });
		}
		std::sort(chunksSorted.begin(), chunksSorted.end(), [](ChunkSort const c1, ChunkSort const c2) -> bool {
			return c1.sqDistance > c2.sqDistance; //chunks located nearer are rendered last
		});
		
		float projection[4][4];
		viewport_current().projectionMatrix(&projection);
		
		for(auto const [_ignore_, chunkIndex] : chunksSorted) {
			auto const chunkPos{ chunks.chunksPosition()[chunkIndex] };
			auto const &chunkData{ chunks.chunksData()[chunkIndex] };
			vec3<int32_t> const relativeChunkPos_{ chunkPos-playerChunk };
			vec3<float> const relativeChunkPos{ 
				static_cast<decltype(cameraPosInChunk)>(relativeChunkPos_)*Chunks::chunkDim
				-cameraPosInChunk
			};
			glUniform3f(relativeChunkPos_u, relativeChunkPos.x, relativeChunkPos.y, relativeChunkPos.z);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkIndex_u); 
				
			static_assert(sizeof chunkData == (8192));
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8192, &chunkData);
				
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			//if(chunkPos == vec3<int32_t>{ 1, 0, 1 })
			//if(relativeChunkPos_ != 0) 
			{
				float const translation4[4][4] ={
					{ 1, 0, 0, relativeChunkPos.x },
					{ 0, 1, 0, relativeChunkPos.y },
					{ 0, 0, 1, relativeChunkPos.z },
					{ 0, 0, 0, 1                  },
				};
		
				float chunkToLocal[4][4];
				
				misc::matMult(toLoc4, translation4, &chunkToLocal);
				
				int projectionVertsCount;
				//std::array<vec2<double>,8> origs;
				/*std::array<std::array<double, 4>, 8> const verts = projectChunk(projection, chunkToLocal, projectionVertsCount, origs);
				if(projectionVertsCount <= 2) (std::cout << "err:"<<projectionVertsCount <<'\n'), exit(-1);
				
				std::array<std::array<GLfloat, 3>, 4> colors;
				if(chunkPos == vec3<int32_t>{ 1, 0, 1 }) {
					colors = {
					std::array<GLfloat, 3>{ 1, 0, 0 },
					std::array<GLfloat, 3>{ 0, 1, 0 },
					std::array<GLfloat, 3>{ 0, 0, 1 },
					std::array<GLfloat, 3>{ 1, 0, 1 }
					
					
				};
				if(testInfo) {
					testInfo = false;
					std::cout << "origs:\n";
					for(int i = 0; i < 8; i ++) {
						std::cout << (origs[i].x) << ' ' << origs[i].y; 
						std::cout << '\n';
					}
					std::cout << "vertices (" << projectionVertsCount << "):\n";
					for(int i = 0; i < projectionVertsCount; i ++) {
						auto const a = [&](int j) -> void {std::cout << (verts[i][j]) << ' '; };
						a(0), a(1), a(2), a(3);
						std::cout << '\n';
					}
					std::cout << "viewport:\npos: ";
					std::cout << currentViewport.position << "\nrot:" << currentViewport.rotation << "\n\n";
				}
				}
				else colors = {
					std::array<GLfloat, 3>{ 1, 1, 1 },
					std::array<GLfloat, 3>{ 1, 1, 1 },
					std::array<GLfloat, 3>{ 1, 1, 1 },
					std::array<GLfloat, 3>{ 1, 1, 1 }
				};
				
				std::array<std::array<GLfloat, 7>, 16> tris{};
				for(int i = 0; i < projectionVertsCount-2; ++i) {
					tris[i*3+0] = std::array<GLfloat, 7>{(float)verts[  0][0], (float)verts[  0][1], (float)verts[  0][2], (float)verts[  0][3], colors[i][0], colors[i][1], colors[i][2] };
					tris[i*3+1] = std::array<GLfloat, 7>{(float)verts[i+1][0], (float)verts[i+1][1], (float)verts[i+1][2], (float)verts[i+1][3], colors[i][0], colors[i][1], colors[i][2] };
					tris[i*3+2] = std::array<GLfloat, 7>{(float)verts[i+2][0], (float)verts[i+2][1], (float)verts[i+2][2], (float)verts[i+2][3], colors[i][0], colors[i][1], colors[i][2] };
				}*/
				
				//glBindBuffer(GL_ARRAY_BUFFER, vbo);
				/*glBufferSubData(
					GL_ARRAY_BUFFER, 0, sizeof(tris),
					&tris[0]
				);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindBuffer(GL_ARRAY_BUFFER, 1);*/
				
				//glBindVertexArray(vao);
				//glEnableVertexAttribArray(0);
				//glEnableVertexAttribArray(1);
				//glDrawArrays(GL_TRIANGLES, 0, (projectionVertsCount-2)*3);
				
				if(debugChunkBorder) {
					glUseProgram(debugProgram);
					glUniformMatrix4fv(db_model_matrix_u, 1, GL_TRUE, &chunkToLocal[0][0]);
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					glDrawArrays(GL_TRIANGLES, 0, 36);
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					glUseProgram(mainProgram);
				}
				
				glUniformMatrix4fv(model_matrix_u, 1, GL_TRUE, &chunkToLocal[0][0]);
				glDrawArrays(GL_TRIANGLES, 0, 36);
				
				
				//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				//glDrawArrays(GL_TRIANGLES, 0, (projectionVertsCount-2)*3);
				//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				//glDisableVertexAttribArray(0);
				//glDisableVertexAttribArray(1);
				//glBindVertexArray(0);
			}
			/*else {
				float const tim = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000000.0;
				GLfloat const floats[] = {
					-1, -1, 0, 1, sinf(tim), cosf(tim), 1,
					-1, +1, 0, 1, sinf(tim), cosf(tim), 1,
					+1, -1, 0, 1, sinf(tim), cosf(tim), 1,
					+1, +1, 0, 1, sinf(tim), cosf(tim), 1
				};					
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferSubData(
					GL_ARRAY_BUFFER, 0, sizeof(floats),
					&floats[0]
				);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				
				glBindVertexArray(vao);
				glEnableVertexAttribArray(0);
				glEnableVertexAttribArray(1);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glDisableVertexAttribArray(1);
				glDisableVertexAttribArray(0);
				glBindVertexArray(0);
			}*/
		}
		
		glfwSwapBuffers(window);
		glfwPollEvents();
		
		if(_i_ == 0) {
			mc.add(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startFrame).count());
			//if(mc.index() == 0) std::cout << "mean:" << mc.mean() << '\n';
		}
		else _i_--;
		
		static GLenum lastError;
        while ((err = glGetError()) != GL_NO_ERROR) {
			if(lastError == err) {
				
			}
            else { 
				std::cout << "glError: " << err << std::endl;
				lastError = err;
			}
		}

        update();
		testInfo = false; 
    }

    glfwTerminate();
	
	std::cin.ignore();
    return 0;
}
