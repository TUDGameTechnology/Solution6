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
#include <algorithm>
#include <iostream>

using namespace Kore;

class MeshObject {
public:
	MeshObject(const char* meshFile, const char* textureFile, const char* normalMapFile, const VertexStructure& structure, float scale = 1.0f) {
		mesh = loadObj(meshFile);
		// @@TODO: Why readable?
		image = new Texture(textureFile, true);
		normalMap = new Texture(normalMapFile, true);

		vertexBuffer = new VertexBuffer(mesh->numVertices, structure, 0);
		float* vertices = vertexBuffer->lock();
		int stride = 3 + 2 + 3 + 3 + 3;
		int strideInFile = 3 + 2 + 3;
		for (int i = 0; i < mesh->numVertices; ++i) {
			vertices[i * stride + 0] = mesh->vertices[i * strideInFile + 0] * scale;
			vertices[i * stride + 1] = mesh->vertices[i * strideInFile + 1] * scale;
			vertices[i * stride + 2] = mesh->vertices[i * strideInFile + 2] * scale;
			vertices[i * stride + 3] = mesh->vertices[i * strideInFile + 3];
			vertices[i * stride + 4] = 1.0f - mesh->vertices[i * strideInFile + 4];
			vertices[i * stride + 5] = mesh->vertices[i * strideInFile + 5];
			vertices[i * stride + 6] = mesh->vertices[i * strideInFile + 6];
			vertices[i * stride + 7] = mesh->vertices[i * strideInFile + 7];
		}
	//	vertexBuffer->unlock();

		indexBuffer = new IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}
	//	indexBuffer->unlock();

		// Calculate the tangent and bitangent vectors
		// We don't index them here, we just copy them for each vertex
		// @@TODO: There seems to be a one-off problem somewhere
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

			float r = 1.0f / (deltaUV1.x() * deltaUV2.y() - deltaUV1.y() * deltaUV2.x());
			vec3 tangent = (deltaPos1 * deltaUV2.y() - deltaPos2 * deltaUV1.y())*r;
			vec3 bitangent = (deltaPos2 * deltaUV1.x() - deltaPos1 * deltaUV2.x())*r;

			// Write them out
		/*	vData1[8] = vData2[8] = vData3[8] = tangent.x();
			vData1[9] = vData2[9] = vData3[9] = tangent.y();
			vData1[10] = vData2[10] = vData3[10] = tangent.z();
			
			vData1[11] = vData2[11] = vData3[11] = bitangent.x();
			vData1[12] = vData2[12] = vData3[12] = bitangent.y();
			vData1[13] = vData2[13] = vData3[13] = bitangent.z(); */
		} 

		vertexBuffer->unlock();
		indexBuffer->unlock();

		M = mat4::Identity();
	}

	void render(TextureUnit tex, TextureUnit normalMapTex) {
		Graphics::setTexture(tex, image);
		Graphics::setTexture(normalMapTex, normalMap);
		Graphics::setVertexBuffer(*vertexBuffer);
		Graphics::setIndexBuffer(*indexBuffer);
		Graphics::drawIndexedVertices();
	}

	mat4 M;
private:
	VertexBuffer* vertexBuffer;
	IndexBuffer* indexBuffer;
	Mesh* mesh;
	Texture* image;
	Texture* normalMap;
};

namespace {
	const int width = 512;
	const int height = 512;
	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	// null terminated array of MeshObject pointers
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	// The view projection matrix aka the camera
	mat4 P;
	mat4 V;

	// uniform locations - add more as you see fit
	TextureUnit tex;
	TextureUnit normalMapTex;
	ConstantLocation pLocation;
	ConstantLocation vLocation;
	ConstantLocation mLocation;
	ConstantLocation lightLocation;
	ConstantLocation eyeLocation;
	ConstantLocation specLocation;
	ConstantLocation roughnessLocation;
	ConstantLocation modeLocation;

	vec3 eye = vec3(0, 0, -3);
	vec3 globe = vec3(0, 0.0, 0.0);
	//vec3 globe = vec3(0.7f, 1.2f, -0.2f);
	vec3 light = vec3(0, 1.95f, -3.0f);
	
	bool left, right, up, down, forward, backward;
	int mode = 0;
	float roughness = 0.9f;
	float specular = 0.1f;
	bool toggle = false;
	
	void update() {
		float t = (float)(System::time() - startTime);
		Kore::Audio::update();

		const float speed = 0.05f;
		if (left) {
			eye.x() -= speed;
		}
		if (right) {
			eye.x() += speed;
		}
		if (forward) {
			eye.z() += speed;
		}
		if (backward) {
			eye.z() -= speed;
		}
		if (up) {
			eye.y() += speed;
		}
		if (down) {
			eye.y() -= speed;
		}
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff000000, 1000.0f);
		

		program->set();

		/*
		Set your uniforms for the light vector, the roughness and all further constants you encounter in the BRDF terms.
		The BRDF itself should be implemented in the fragment shader.
		*/
		Graphics::setFloat3(lightLocation, light);
		Graphics::setFloat3(eyeLocation, eye);
		Graphics::setFloat(specLocation, specular);
		Graphics::setFloat(roughnessLocation, roughness);
		Graphics::setInt(modeLocation, mode);

		// set the camera
		// vec3(0, 2, -3), vec3(0, 2, 0)

		V = mat4::lookAt(eye, globe, vec3(0, 1.0, 0));
		//V = mat4::lookAt(eye, globe, vec3(0, 1, 0)); //rotation test, can be deleted
		P = mat4::Perspective(40.0, (float)width / (float)height, 0.1f, 20);
		Graphics::setMatrix(vLocation, V);
		Graphics::setMatrix(pLocation, P);

		// iterate the MeshObjects
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// set the model matrix
			Graphics::setMatrix(mLocation, (*current)->M);

			(*current)->render(tex, normalMapTex);
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
		else if (code == Key_B) {
			mode = 0;
			std::cout << "Complete BRDF" << std::endl;
		}
		else if (code == Key_F) {
			mode = 1;
			std::cout << "Schlick's Fresnel approximation" << std::endl;
		}
		else if (code == Key_D) {
			mode = 2;
			std::cout << "Trowbridge-Reitz normal distribution term" << std::endl;
		}
		else if (code == Key_G) {
			mode = 3;
			std::cout << "Cook and Torrance's geometry factor" << std::endl;
		}
		else if (code == Key_T) {
			toggle = !toggle;
		}
		else if (code == Key_R) {
			if (toggle) {
				roughness = std::max(roughness - 0.1f, 0.0f);
			}
			else {
				roughness = std::min(roughness + 0.1f, 1.0f);
			}
			std::cout << "Roughness: " << roughness << std::endl;
		}
		else if (code == Key_E) {
			if (toggle) {
				specular = std::max(specular - 0.1f, 0.0f);
			}
			else {
				specular = std::min(specular + 0.1f, 1.0f);
			}
			std::cout << "Specular: " << specular << std::endl;
		}
		else if (code == Key_Space) {
			std::cout << "hi" << std::endl;
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
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);
		// Additional fields for tangent and bitangent
		structure.add("tangent", Float3VertexData);
		structure.add("bitangent", Float3VertexData);

		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);

		tex = program->getTextureUnit("tex");
		normalMapTex = program->getTextureUnit("normalMapTex");
		pLocation = program->getConstantLocation("P");
		vLocation = program->getConstantLocation("V");
		mLocation = program->getConstantLocation("M");
		lightLocation = program->getConstantLocation("light");
		eyeLocation = program->getConstantLocation("eye");
		specLocation = program->getConstantLocation("spec");
		roughnessLocation = program->getConstantLocation("roughness");
		modeLocation = program->getConstantLocation("mode");
		//@@TODO: Remove light pos
		//@@TODO: Actually add normal map texture
		objects[0] = new MeshObject("ball.obj", "176.jpg", "176_norm.jpg", structure, 1.0f);
		objects[0]->M = mat4::Translation(globe.x(), globe.y(), globe.z());
		//objects[1] = new MeshObject("ball.obj", "light_tex.png", "light_tex.png", structure, 0.3f);
		//objects[1]->M = mat4::Translation(light.x(), light.y(), light.z());

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);

		Graphics::setRenderState(DepthWrite, true);

		Graphics::setTextureAddressing(tex, Kore::U, Repeat);
		Graphics::setTextureAddressing(tex, Kore::V, Repeat);
		
		std::cout << "Showing complete BRDF" << std::endl;
		std::cout << "Roughness: " << roughness << std::endl;
		std::cout << "Specular: " << specular << std::endl;

		// eye = vec3(0, 2, -3);
	}
}

int kore(int argc, char** argv) {
	// @@TODO: Remove SimpleGraphics?
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
