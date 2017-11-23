#version 450

// Textures for diffuse and normal map
uniform sampler2D tex;
uniform sampler2D normalMap;

// Light and camera in world space
uniform vec3 light;
uniform vec3 eye;

// Position in world space
in vec3 position;
// Texture coordinate
in vec2 texCoord;

// Light and camera position in tangent space
in vec3 light_tangentspace;
in vec3 eye_tangentspace;

// Distance between the light and the current point. For the DirectX backend, we need to compute this in the vertex shader, if not, it will get a wrong value
in float lightDistance;

out vec4 frag;

void main() {

	// Color and intensity of the light
	vec3 LightColor = vec3(1,1,1);
	float LightPower = 5.0;
	
	// Material properties
	vec3 MaterialDiffuseColor = texture( tex, texCoord).rgb;
	vec3 MaterialAmbientColor = vec3(0.1,0.1,0.1) * MaterialDiffuseColor;
	// If we had a specular map, we could plug it in here
	vec3 MaterialSpecularColor = vec3(1.0, 1.0, 1.0);

	// Normal from the normal map
	/************************************************************************/
	/* Exercise P6.1 b): Extract the normal from the normal map.             /
	 * For good measure, you can normalize them again                        /
	/************************************************************************/
	vec3 TextureNormal_tangentspace = normalize(texture( normalMap, texCoord ).rgb * 2.0 - 1.0);
	
	// Distance to the light
	// float lightDistance = length( light - position );

	// Normal of the computed fragment, in camera space
	vec3 n = TextureNormal_tangentspace;
	// Direction of the light (from the fragment to the light)
	vec3 l = normalize(light_tangentspace);
	// Cosine of the angle between the normal and the light direction, 
	// clamped above 0
	//  - light is at the vertical of the triangle -> 1
	//  - light is perpendicular to the triangle -> 0
	//  - light is behind the triangle -> 0
	float cosTheta = clamp( dot( n,l ), 0.0, 1.0 );

	// Eye vector (towards the camera)
	vec3 E = normalize(eye_tangentspace);
	// Direction in which the triangle reflects the light
	vec3 R = reflect(-l,n);
	// Cosine of the angle between the Eye vector and the Reflect vector,
	// clamped to 0
	//  - Looking into the reflection -> 1
	//  - Looking elsewhere -> < 1
	float cosAlpha = clamp( dot( E,R ), 0.0, 1.0 );

	float radius = 5.0;
	float a = 2.0 / radius;
	float b = 1.0 / (radius * radius);
	float attenuation = 1.0 / (1.0 + a*lightDistance + b*lightDistance*lightDistance);

	vec3 color = 
		// Ambient : simulates indirect lighting
		MaterialAmbientColor +
		// Diffuse : "color" of the object
		MaterialDiffuseColor * LightColor * LightPower * cosTheta * attenuation + 
		// Specular : reflective highlight, like a mirror
		MaterialSpecularColor * LightColor * LightPower * pow(cosAlpha,5.0) * attenuation;
	frag = vec4(color, 1.0);
}
