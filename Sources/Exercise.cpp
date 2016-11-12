#include "pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Audio/Mixer.h>
#include <Kore/Graphics/Image.h>
#include <Kore/Graphics/Graphics.h>
#include "ObjLoader.h"

using namespace Kore;

namespace {

struct SceneParameters {
	// The view projection matrix aka the camera
	mat4 P;
	mat4 V;

	// Position of the camera in world space
	vec3 eye = vec3(0, 0, -3);

	// Position of the light in world space
	vec3 light;

	// Current time
	float time;

	// PacMan parameters:
	float openAngle = 146.0f;
	float closeAngle = 180.0f;
	float duration = 0.5f;
};

SceneParameters sceneParameters;

class ShaderProgram {

public:
	ShaderProgram(const char* vsFile, const char* fsFile, VertexStructure& structure)
	{
		// Load and link the shaders
		FileReader vs(vsFile);
		FileReader fs(fsFile);
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);

		pLocation = program->getConstantLocation("P");
		vLocation = program->getConstantLocation("V");
		mLocation = program->getConstantLocation("M");
	}

	// Update this program from the scene parameters
	virtual void Set(const SceneParameters& parameters, const mat4& M, Texture* diffuse = nullptr, Texture* normalMap = nullptr)
	{
		program->set();
		Graphics::setMatrix(mLocation, M);
		Graphics::setMatrix(vLocation, parameters.V);
		Graphics::setMatrix(pLocation, parameters.P);
	}


protected:

	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	// Uniform locations - add more as you see fit
	ConstantLocation pLocation;
	ConstantLocation vLocation;
	ConstantLocation mLocation;
};


class ShaderProgram_NormalMap : public ShaderProgram {
	
public:

	ShaderProgram_NormalMap(const char* vsFile, const char* fsFile, VertexStructure& structure)
		: ShaderProgram(vsFile, fsFile, structure)
	{
		lightLocation = program->getConstantLocation("light");
		eyeLocation = program->getConstantLocation("eye");
		tex = program->getTextureUnit("tex");
		normalMapTex = program->getTextureUnit("normalMap");

		Graphics::setTextureAddressing(tex, Kore::U, Repeat);
		Graphics::setTextureAddressing(tex, Kore::V, Repeat);
	}

	virtual void Set(const SceneParameters& parameters, const mat4& M, Texture* diffuse, Texture* normalMap) override
	{
		ShaderProgram::Set(parameters, M);
		Graphics::setTexture(tex, diffuse);
		Graphics::setTexture(normalMapTex, normalMap);
		Graphics::setFloat3(lightLocation, parameters.light);
		Graphics::setFloat3(eyeLocation, parameters.eye);
	}


protected:

	// Texture units
	TextureUnit tex;
	TextureUnit normalMapTex;

	// Constant locations
	ConstantLocation lightLocation;
	ConstantLocation eyeLocation;
};


class ShaderProgram_PacMan : public ShaderProgram {

public:

	ShaderProgram_PacMan(const char* vsFile, const char* fsFile, VertexStructure& structure)
		: ShaderProgram(vsFile, fsFile, structure)
	{
		timeLocation = program->getConstantLocation("time");
		durationLocation = program->getConstantLocation("duration");
		openAngleLocation = program->getConstantLocation("openAngle");
		closeAngleLocation = program->getConstantLocation("closeAngle");
	}

	void Set(const SceneParameters& parameters, const mat4& M, Texture* diffuse, Texture* normalMap) override
	{
		ShaderProgram::Set(parameters, M);
		Graphics::setFloat(timeLocation, parameters.time);
		Graphics::setFloat(durationLocation, parameters.duration);
		Graphics::setFloat(openAngleLocation, parameters.openAngle);
		Graphics::setFloat(closeAngleLocation, parameters.closeAngle);
	}


protected:

	// Constant locations
	ConstantLocation timeLocation;
	ConstantLocation durationLocation;
	ConstantLocation openAngleLocation;
	ConstantLocation closeAngleLocation;
};

class MeshObject {
public:

	MeshObject(const char* meshFile, const char* textureFile, const char* normalMapFile, const VertexStructure& structure, ShaderProgram* shaderProgram, float scale = 1.0f)
		: shaderProgram(shaderProgram), image(nullptr), normalMap(nullptr)
	{
		mesh = loadObj(meshFile);
		if (textureFile)
		{
			image = new Texture(textureFile);
		}
		if (normalMapFile)
		{
			normalMap = new Texture(normalMapFile);
		}

		vertexBuffer = new VertexBuffer(mesh->numVertices, structure, 0);
		float* vertices = vertexBuffer->lock();
		int stride = 3 + 2 + 3 + 3 + 3;
		int strideInFile = 3 + 2 + 3;
		for (int i = 0; i < mesh->numVertices; ++i) {
			float* v = &vertices[i * stride];
			float* meshV = &mesh->vertices[i * strideInFile];
			v[0] = meshV[0] * scale;
			v[1] = meshV[1] * scale;
			v[2] = meshV[2] * scale;
			v[3] = meshV[3];
			v[4] = 1.0f - meshV[4];
			v[5] = meshV[5];
			v[6] = meshV[6];
			v[7] = meshV[7];
		}

		indexBuffer = new IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}

		// Calculate the tangent and bitangent vectors
		// We don't index them here, we just copy them for each vertex
		for (int i = 0; i < mesh->numIndices / 3; i++)
		{
			int index1 = indices[i * 3 + 0];
			int index2 = indices[i * 3 + 1];
			int index3 = indices[i * 3 + 2];

			float* vData1 = &vertices[index1 * stride];
			float* vData2 = &vertices[index2 * stride];
			float* vData3 = &vertices[index3 * stride];

			vec3 v1(vData1[0], vData1[1], vData1[2]);
			vec3 v2(vData2[0], vData2[1], vData2[2]);
			vec3 v3(vData3[0], vData3[1], vData3[2]);

			vec2 uv1(vData1[3], vData1[4]);
			vec2 uv2(vData2[3], vData2[4]);
			vec2 uv3(vData3[3], vData3[4]);

			// Edges of the triangle : position delta
			vec3 deltaPos1 = v2 - v1;
			vec3 deltaPos2 = v3 - v1;

			// UV delta
			vec2 deltaUV1 = uv2 - uv1;
			vec2 deltaUV2 = uv3 - uv1;

			/************************************************************************/
			/* Exercise P6.1 a): Use deltaPos1/2 and deltaUV1/2 to calculate the     /
			 * tangent and bitangent vectors                                         /
			/************************************************************************/
			float r = 1.0f / (deltaUV1.x() * deltaUV2.y() - deltaUV1.y() * deltaUV2.x());
			vec3 normal(vData1[5], vData1[6], vData1[7]);
			vec3 tangent = (deltaPos1 * deltaUV2.y() - deltaPos2 * deltaUV1.y())*r;
			vec3 bitangent = (deltaPos2 * deltaUV1.x() - deltaPos1 * deltaUV2.x())*r;

			// Don't forget to normalize them
			tangent = tangent.normalize();
			bitangent = bitangent.normalize();

			// Gram-Schmidt orthogonalization
			tangent = tangent - normal * normal.dot(tangent);

			// Calculate handedness
			if (normal.cross(tangent).dot(bitangent) < 0.0f)
			{
				tangent = tangent * -1.0f;
			}

			// Write them out
			vData1[8] = vData2[8] = vData3[8] = tangent.x();
			vData1[9] = vData2[9] = vData3[9] = tangent.y();
			vData1[10] = vData2[10] = vData3[10] = tangent.z();
			
			vData1[11] = vData2[11] = vData3[11] = bitangent.x();
			vData1[12] = vData2[12] = vData3[12] = bitangent.y();
			vData1[13] = vData2[13] = vData3[13] = bitangent.z(); 
		} 

		vertexBuffer->unlock();
		indexBuffer->unlock();

		M = mat4::Identity();
	}

	void render() {
		shaderProgram->Set(sceneParameters, M, image, normalMap);
		Graphics::setVertexBuffer(*vertexBuffer);
		Graphics::setIndexBuffer(*indexBuffer);
		Graphics::drawIndexedVertices();
	}

	// Model matrix of this object
	mat4 M;

private:
	VertexBuffer* vertexBuffer;
	IndexBuffer* indexBuffer;
	Mesh* mesh;
	Texture* image;
	Texture* normalMap;
	ShaderProgram* shaderProgram;
};

	const int width = 512;
	const int height = 512;
	double startTime;

	// null terminated array of MeshObject pointers
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	MeshObject* lightMesh = nullptr;

	// Position of the mesh to use for normal mapping
	vec3 normalMapModel = vec3(2.0, 0.0, 0.0);

	// Initial position of the light
	vec3 lightStart = vec3(0, 1.95f, 3.0f);

	// Rotation rate of the light
	float lightRotationRate = 1.0f;

	
	bool left, right, up, down, forward, backward;
	
	void update() {
		float t = (float)(System::time() - startTime);
		sceneParameters.time = t;
		Kore::Audio::update();

		// Animate the light point
		mat3 rotation = mat3::RotationY(t * lightRotationRate);
		sceneParameters.light = rotation * lightStart + normalMapModel;

		const float speed = 0.05f;
		if (left) {
			sceneParameters.eye.x() -= speed;
		}
		if (right) {
			sceneParameters.eye.x() += speed;
		}
		if (forward) {
			sceneParameters.eye.z() += speed;
		}
		if (backward) {
			sceneParameters.eye.z() -= speed;
		}
		if (up) {
			sceneParameters.eye.y() += speed;
		}
		if (down) {
			sceneParameters.eye.y() -= speed;
		}
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff000000, 1000.0f);
		

		sceneParameters.V = mat4::lookAt(sceneParameters.eye, vec3(0.0, 0.0, 0.0), vec3(0, 1.0, 0));
		sceneParameters.P = mat4::Perspective(90.0, (float)width / (float)height, 0.1f, 100);

		// Set the light position
		lightMesh->M = Kore::mat4::Translation(sceneParameters.light.x(), sceneParameters.light.y(), sceneParameters.light.z());

		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// Render the object
			(*current)->render();
			++current;
		}

		Graphics::end();
		Graphics::swapBuffers();
	}

	void keyDown(KeyCode code, wchar_t character) {
		if (code == Key_Left) {
			left = true;
		}
		else if (code == Key_Right) {
			right = true;
		}
		else if (code == Key_Up) {
			forward = true;
		}
		else if (code == Key_Down) {
			backward = true;
		}
		else if (code == Key_W) {
			up = true;
		}
		else if (code == Key_S) {
			down = true;
		}
	}
	
	void keyUp(KeyCode code, wchar_t character) {
		if (code == Key_Left) {
			left = false;
		}
		else if (code == Key_Right) {
			right = false;
		}
		else if (code == Key_Up) {
			forward = false;
		}
		else if (code == Key_Down) {
			backward = false;
		}
		else if (code == Key_W) {
			up = false;
		}
		else if (code == Key_S) {
			down = false;
		}
	}
	
	void mouseMove(int windowId, int x, int y, int movementX, int movementY) {

	}
	
	void mousePress(int windowId, int button, int x, int y) {

	}

	void mouseRelease(int windowId, int button, int x, int y) {

	}

	void init() {
		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);
		// Additional fields for tangent and bitangent
		structure.add("tangent", Float3VertexData);
		structure.add("bitangent", Float3VertexData);

		// Set up the normal mapping shader
		ShaderProgram* normalMappingProgram = new ShaderProgram_NormalMap("shader.vert", "shader.frag", structure);

		// Set up the pacman shader
		ShaderProgram* pacManProgram = new ShaderProgram_PacMan("pacman.vert", "pacman.frag", structure);

		objects[0] = new MeshObject("box.obj", "199.jpg", "199_norm.jpg", structure, normalMappingProgram, 1.0f);
		objects[0]->M = mat4::Translation(normalMapModel.x(), normalMapModel.y(), normalMapModel.z());

		lightMesh = objects[1] = new MeshObject("ball.obj", "light_tex.png", "light_tex.png", structure, normalMappingProgram, 0.3f);
		lightMesh->M = mat4::Translation(sceneParameters.light.x(), sceneParameters.light.y(), sceneParameters.light.z());

		objects[2] = new MeshObject("PacMan.obj", nullptr, nullptr, structure, pacManProgram);
		objects[2]->M = mat4::Translation(-2.0f, 0.0f, 0.0f) * mat4::RotationZ(Kore::pi);

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);
		Graphics::setRenderState(DepthWrite, true);

		Graphics::setRenderState(BackfaceCulling, true);

	}
}

int kore(int argc, char** argv) {
	Kore::System::setName("TUD Game Technology - ");
	Kore::System::setup();
	Kore::WindowOptions options;
	options.title = "Solution 6";
	options.width = width;
	options.height = height;
	options.x = 100;
	options.y = 100;
	options.targetDisplay = -1;
	options.mode = WindowModeWindow;
	options.rendererOptions.depthBufferBits = 16;
	options.rendererOptions.stencilBufferBits = 8;
	options.rendererOptions.textureFormat = 0;
	options.rendererOptions.antialiasing = 0;
	Kore::System::initWindow(options);

	init();

	Kore::System::setCallback(update);

	Kore::Mixer::init();
	Kore::Audio::init();


	startTime = System::time();


	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	Kore::System::start();

	return 0;
}
