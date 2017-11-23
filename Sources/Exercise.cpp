#include "pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Graphics4/Graphics.h>

#include <Kore/Graphics4/PipelineState.h>

#include "ObjLoader.h"
#include "Memory.h"

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
	ShaderProgram(const char* vsFile, const char* fsFile, Graphics4::VertexStructure& structure)
	{
		// Load and link the shaders
		FileReader vs(vsFile);
		FileReader fs(fsFile);
		vertexShader = new Graphics4::Shader(vs.readAll(), vs.size(), Graphics4::VertexShader);
		fragmentShader = new Graphics4::Shader(fs.readAll(), fs.size(), Graphics4::FragmentShader);
		
		pipeline = new Graphics4::PipelineState;
		pipeline->depthWrite = true;
		pipeline->depthMode = Graphics4::ZCompareLess;
		pipeline->inputLayout[0] = &structure;
		pipeline->inputLayout[1] = nullptr;
		pipeline->vertexShader = vertexShader;
		pipeline->fragmentShader = fragmentShader;
		pipeline->compile();
		
		pLocation = pipeline->getConstantLocation("P");
		vLocation = pipeline->getConstantLocation("V");
		mLocation = pipeline->getConstantLocation("M");
	}

	// Update this program from the scene parameters
	virtual void Set(const SceneParameters& parameters, const mat4& M, Graphics4::Texture* diffuse = nullptr, Graphics4::Texture* normalMap = nullptr)
	{
		Graphics4::setPipeline(pipeline);
		Graphics4::setMatrix(mLocation, M);
		Graphics4::setMatrix(vLocation, parameters.V);
		Graphics4::setMatrix(pLocation, parameters.P);
	}


protected:
	Graphics4::Shader* vertexShader;
	Graphics4::Shader* fragmentShader;
	Graphics4::PipelineState* pipeline;
	
	// Uniform locations - add more as you see fit
	Graphics4::ConstantLocation pLocation;
	Graphics4::ConstantLocation vLocation;
	Graphics4::ConstantLocation mLocation;
};


class ShaderProgram_NormalMap : public ShaderProgram {
		
public:
		
	ShaderProgram_NormalMap(const char* vsFile, const char* fsFile, Graphics4::VertexStructure& structure)
	: ShaderProgram(vsFile, fsFile, structure)
	{
		lightLocation = pipeline->getConstantLocation("light");
//		eyeLocation = pipeline->getConstantLocation("eye");
		tex = pipeline->getTextureUnit("tex");
		normalMapTex = pipeline->getTextureUnit("normalMap");
			
		Graphics4::setTextureAddressing(tex, Graphics4::U, Graphics4::Repeat);
		Graphics4::setTextureAddressing(tex, Graphics4::V, Graphics4::Repeat);
	}
		
	virtual void Set(const SceneParameters& parameters, const mat4& M, Graphics4::Texture* diffuse, Graphics4::Texture* normalMap) override
	{
		ShaderProgram::Set(parameters, M);
		Graphics4::setTexture(tex, diffuse);
		Graphics4::setTexture(normalMapTex, normalMap);
		Graphics4::setFloat3(lightLocation, parameters.light);
//		Graphics4::setFloat3(eyeLocation, parameters.eye);
	}
		
		
protected:
	
	// Texture units
	Graphics4::TextureUnit tex;
	Graphics4::TextureUnit normalMapTex;
		
	// Constant locations
	Graphics4::ConstantLocation lightLocation;
//	Graphics4::ConstantLocation eyeLocation;
};


class ShaderProgram_PacMan : public ShaderProgram {

public:

	ShaderProgram_PacMan(const char* vsFile, const char* fsFile, Graphics4::VertexStructure& structure)
		: ShaderProgram(vsFile, fsFile, structure)
	{
		timeLocation = pipeline->getConstantLocation("time");
		durationLocation = pipeline->getConstantLocation("duration");
		openAngleLocation = pipeline->getConstantLocation("openAngle");
		closeAngleLocation = pipeline->getConstantLocation("closeAngle");
	}

	void Set(const SceneParameters& parameters, const mat4& M, Graphics4::Texture* diffuse, Graphics4::Texture* normalMap) override
	{
		ShaderProgram::Set(parameters, M);
		Graphics4::setFloat(timeLocation, parameters.time);
		Graphics4::setFloat(durationLocation, parameters.duration);
		Graphics4::setFloat(openAngleLocation, parameters.openAngle);
		Graphics4::setFloat(closeAngleLocation, parameters.closeAngle);
	}


protected:

	// Constant locations
	Graphics4::ConstantLocation timeLocation;
	Graphics4::ConstantLocation durationLocation;
	Graphics4::ConstantLocation openAngleLocation;
	Graphics4::ConstantLocation closeAngleLocation;
};

class MeshObject {
public:

	MeshObject(const char* meshFile, const char* textureFile, const char* normalMapFile, const Graphics4::VertexStructure& structure, ShaderProgram* shaderProgram, float scale = 1.0f)
		: shaderProgram(shaderProgram), image(nullptr), normalMap(nullptr)
	{
		mesh = loadObj(meshFile);
		if (textureFile)
		{
			image = new Graphics4::Texture(textureFile);
		}
		if (normalMapFile)
		{
			normalMap = new Graphics4::Texture(normalMapFile);
		}

		vertexBuffer = new Graphics4::VertexBuffer(mesh->numVertices, structure, 0);
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

		indexBuffer = new Graphics4::IndexBuffer(mesh->numFaces * 3);
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
		Graphics4::setVertexBuffer(*vertexBuffer);
		Graphics4::setIndexBuffer(*indexBuffer);
		Graphics4::drawIndexedVertices();
	}

	// Model matrix of this object
	mat4 M;

private:
	Graphics4::VertexBuffer* vertexBuffer;
	Graphics4::IndexBuffer* indexBuffer;
	Mesh* mesh;
	Graphics4::Texture* image;
	Graphics4::Texture* normalMap;
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
		
		Graphics4::begin();
		Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, 0xff000000, 1000.0f);
		
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

		Graphics4::end();
		Graphics4::swapBuffers();
	}

	void keyDown(KeyCode code) {
		if (code == KeyLeft) {
			left = true;
		}
		else if (code == KeyRight) {
			right = true;
		}
		else if (code == KeyUp) {
			forward = true;
		}
		else if (code == KeyDown) {
			backward = true;
		}
		else if (code == KeyW) {
			up = true;
		}
		else if (code == KeyS) {
			down = true;
		}
	}
	
	void keyUp(KeyCode code) {
		if (code == KeyLeft) {
			left = false;
		}
		else if (code == KeyRight) {
			right = false;
		}
		else if (code == KeyUp) {
			forward = false;
		}
		else if (code == KeyDown) {
			backward = false;
		}
		else if (code == KeyW) {
			up = false;
		}
		else if (code == KeyS) {
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
		Memory::init();
		
		// This defines the structure of your Vertex Buffer
		Graphics4::VertexStructure structure;
		structure.add("pos", Graphics4::Float3VertexData);
		structure.add("tex", Graphics4::Float2VertexData);
		structure.add("nor", Graphics4::Float3VertexData);
		
		// Additional fields for tangent and bitangent
		structure.add("tangent", Graphics4::Float3VertexData);
		structure.add("bitangent", Graphics4::Float3VertexData);

		// Set up the normal mapping shader
		ShaderProgram* normalMappingProgram = new ShaderProgram_NormalMap("shader.vert", "shader.frag", structure);

		// Set up the pacman shader
		ShaderProgram* pacManProgram = new ShaderProgram_PacMan("pacman.vert", "pacman.frag", structure);

		objects[0] = new MeshObject("box.obj", "199.JPG", "199_norm.JPG", structure, normalMappingProgram, 1.0f);
		objects[0]->M = mat4::Translation(normalMapModel.x(), normalMapModel.y(), normalMapModel.z());

		lightMesh = objects[1] = new MeshObject("ball.obj", "light_tex.png", "light_tex.png", structure, normalMappingProgram, 0.3f);
		lightMesh->M = mat4::Translation(sceneParameters.light.x(), sceneParameters.light.y(), sceneParameters.light.z());

		objects[2] = new MeshObject("PacMan.obj", nullptr, nullptr, structure, pacManProgram);
		objects[2]->M = mat4::Translation(-2.0f, 0.0f, 0.0f) * mat4::RotationZ(Kore::pi);
	}
}

int kore(int argc, char** argv) {
	Kore::System::init("Solution 6", width, height);

	init();

	Kore::System::setCallback(update);

	startTime = System::time();

	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	Kore::System::start();

	return 0;
}
