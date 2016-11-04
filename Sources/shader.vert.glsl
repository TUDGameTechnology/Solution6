attribute vec3 pos;
attribute vec2 tex;
attribute vec3 nor;
attribute vec3 tangent;
attribute vec3 bitangent;
varying vec3 position;
varying vec2 texCoord;
varying vec3 normal;

varying vec3 LightDirection_tangentspace;
varying vec3 EyeDirection_tangentspace;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform vec3 light;
uniform vec3 eye;

//@@TODO: Check with Robert 
highp mat3 mytranspose(in highp mat3 inMatrix) {
    highp vec3 i0 = inMatrix[0];
    highp vec3 i1 = inMatrix[1];
    highp vec3 i2 = inMatrix[2];

    highp mat3 outMatrix = mat3(
                 vec3(i0.x, i1.x, i2.x),
                 vec3(i0.y, i1.y, i2.y),
                 vec3(i0.z, i1.z, i2.z)
                 );

    return outMatrix;
}

void kore() {

	// Output position of the vertex, in clip space : MVP * position
	mat4 MVP = P * V * M;
	vec4 newPos = MVP * vec4(pos.xyz, 1.0);
	gl_Position = newPos;

	position = newPos.xyz / newPos.w;
	texCoord = tex;
	normal = (V * M * vec4(nor, 0.0)).xyz;
	
	// Position of the vertex, in worldspace : M * position
	vec3 Position_worldspace = (M * vec4(pos, 1.0)).xyz;

	
	// Vector that goes from the vertex to the camera, in camera space.
	// In camera space, the camera is at the origin (0,0,0).
	vec3 vertexPosition_cameraspace = ( V * M * vec4(pos, 1.0)).xyz;
	vec3 EyeDirection_cameraspace = vec3(0,0,0) - vertexPosition_cameraspace;

	
	// Vector that goes from the vertex to the light, in camera space. M is ommited because it's identity.
	vec3 LightPosition_cameraspace = ( V * vec4(light, 1.0)).xyz;
	vec3 LightDirection_cameraspace = LightPosition_cameraspace + EyeDirection_cameraspace;

	// UV of the vertex. No special space for this one.
//	UV = vertexUV;
	
	// model to camera = ModelView
	mat4 MV = V * M;
	vec3 vertexTangent_cameraspace = (MV * vec4(tangent, 1.0)).xyz;
	vec3 vertexBitangent_cameraspace = (MV * vec4(bitangent, 1.0)).xyz;
	vec3 vertexNormal_cameraspace = (MV * vec4(normal, 1.0)).xyz;

	mat3 tempTBN = mat3(
		vertexTangent_cameraspace,
		vertexBitangent_cameraspace,
		vertexNormal_cameraspace	
	);
	
	mat3 TBN = mytranspose(tempTBN); // You can use dot products instead of building this matrix and transposing it. See References for details.

	LightDirection_tangentspace = TBN * LightDirection_cameraspace;
	EyeDirection_tangentspace =  TBN * EyeDirection_cameraspace; 

	


	/* vec4 newPos = M * vec4(pos.x, pos.y, pos.z, 1.0);
	gl_Position = P * V * newPos;
	position = newPos.xyz / newPos.w;
	texCoord = tex;
	normal = (V * M * vec4(nor, 0.0)).xyz; */
}
