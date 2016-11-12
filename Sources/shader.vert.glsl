// Position in model space
attribute vec3 pos;
// Texture coordinate
attribute vec2 tex;
// Normal, tangent, bitangent
attribute vec3 nor;
attribute vec3 tangent;
attribute vec3 bitangent;

// Projection, view and model matrices
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

// Light position in world space
uniform vec3 light;
// Camera position in world space
// uniform vec3 eye;

// Position in world space
varying vec3 position;

// Texture coordinate
varying vec2 texCoord;

varying vec3 light_tangentspace;
varying vec3 eye_tangentspace;

varying float lightDistance;

highp mat3 mytranspose(in highp mat3 inMatrix) {

    highp mat3 outMatrix = inMatrix;
	outMatrix[0][1] = inMatrix[1][0];
	outMatrix[0][2] = inMatrix[2][0];
	outMatrix[1][0] = inMatrix[0][1];
	outMatrix[1][2] = inMatrix[2][1];
	outMatrix[2][0] = inMatrix[0][2];
	outMatrix[2][1] = inMatrix[1][2];
    return outMatrix;
}

void kore() {

	// Output position of the vertex, in clip space : MVP * position
	mat4 MVP = P * V * M;
	gl_Position =  MVP * vec4(pos,1);
	
	// Position of the vertex, in worldspace : M * position
	position = (M * vec4(pos,1)).xyz;

	lightDistance = length(light - position);
	
	// Vector that goes from the vertex to the camera, in camera space.
	// In camera space, the camera is at the origin (0,0,0).
	vec3 vertexPosition_cameraspace = ( V * M * vec4(pos,1)).xyz;
	vec3 EyeDirection_cameraspace = vec3(0,0,0) - vertexPosition_cameraspace;

	// Vector that goes from the vertex to the light, in camera space. M is ommited because it's identity.
	vec3 LightPosition_cameraspace = ( V * vec4(light,1)).xyz;
	vec3 LightDirection_cameraspace = LightPosition_cameraspace + EyeDirection_cameraspace;
	
	// UV of the vertex. No special space for this one.
	texCoord = tex;
	
	// model to camera = ModelView
	mat3 MV3x3 = mat3(V * M);
	vec3 vertexTangent_cameraspace = MV3x3 * tangent;
	vec3 vertexBitangent_cameraspace = MV3x3 * bitangent;
	vec3 vertexNormal_cameraspace = MV3x3 * nor;
	
	mat3 TBN = mytranspose(mat3(
		vertexTangent_cameraspace,
		vertexBitangent_cameraspace,
		vertexNormal_cameraspace	
	)); 

	light_tangentspace = TBN * LightDirection_cameraspace;
	eye_tangentspace =  TBN * EyeDirection_cameraspace;
}
