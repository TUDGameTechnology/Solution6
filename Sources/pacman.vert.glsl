#version 450

in vec3 pos;
in vec2 tex;
in vec3 nor;
in vec3 binormal;
in vec3 tangent;
uniform mat4 V;
uniform mat4 P;
uniform mat4 M;

//The time of the frame
uniform float time;

// Add animation parameters you need here
// See the comments in the SceneParameters and ShaderProgram structs/classes for how to add them on the CPU side
uniform float duration;
uniform float closeAngle;
uniform float openAngle;

#define M_PI 3.1415926535897932384626433832795

void main() {

	// Don't remove these dummy calculations when working with DirectX, otherwise, the program will not run since the attributes are optimized away
	vec3 dontremoveme = nor; vec2 meneither = tex; dontremoveme = binormal; dontremoveme = tangent;


	/************************************************************************/
	/* Exercise P6.2: Implement the characteristic PacMan eating animation   /
	 * here. See the video on the website for reference.                     /
	/************************************************************************/

    // Calculate the angle of this vertex
    float l = length(vec2(pos.x, pos.y));
    float a = acos(pos.x/l);

    // Calculate the new angle, depending on the animation parameters and the time
    float angleDiff = closeAngle - openAngle;
    float morph = ( ((sin(time*(2.0*M_PI)/duration) + 1.0)/2.0) * ( angleDiff/openAngle )) + 1.0;
    a = a * morph;

    // Calculate angles over 180 degrees
    float s = sign(pos.y);
    float b = M_PI-a;
    float c = a + (s*0.5 + 0.5) * 2.0 * (b);

    // And set the position of the vertex
	gl_Position = P * V * M * vec4(cos(c)*l, -sin(c)*l, pos.z, 1.0);
}
