#ifdef GL_ES
precision mediump float;
#endif

// Textures for diffuse and normal map
uniform sampler2D tex;
uniform sampler2D normalMap;

// Light and camera in world space
uniform vec3 light;
uniform vec3 eye;

// Position in world space
varying vec3 position;
// Texture coordinate
varying vec2 texCoord;

// Light and camera position in tangent space
varying vec3 light_tangentspace;
varying vec3 eye_tangentspace;

void kore(){

	// Color and intensity of the light
	vec3 LightColor = vec3(1,1,1);
	float LightPower = 10.0;
	
	// Material properties
	vec3 MaterialDiffuseColor = texture2D( tex, texCoord).rgb;
	vec3 MaterialAmbientColor = vec3(0.1,0.1,0.1) * MaterialDiffuseColor;
	// If we had a specular map, we could plug it in here
	vec3 MaterialSpecularColor = vec3(1.0, 1.0, 1.0);

	// Normal from the normal map
	vec3 TextureNormal_tangentspace = normalize(texture2D( normalMap, texCoord ).rgb*2.0 - 1.0);
	
	// Distance to the light
	float distance = length( light - position );

	// Normal of the computed fragment, in camera space
	vec3 n = TextureNormal_tangentspace;
	// Direction of the light (from the fragment to the light)
	vec3 l = normalize(light_tangentspace);
	// Cosine of the angle between the normal and the light direction, 
	// clamped above 0
	//  - light is at the vertical of the triangle -> 1
	//  - light is perpendicular to the triangle -> 0
	//  - light is behind the triangle -> 0
	float cosTheta = clamp( dot( n,l ), 0.0,1.0 );

	// Eye vector (towards the camera)
	vec3 E = normalize(eye_tangentspace);
	// Direction in which the triangle reflects the light
	vec3 R = reflect(-l,n);
	// Cosine of the angle between the Eye vector and the Reflect vector,
	// clamped to 0
	//  - Looking into the reflection -> 1
	//  - Looking elsewhere -> < 1
	float cosAlpha = clamp( dot( E,R ), 0.0,1.0 );
	
	vec3 color = 
		// Ambient : simulates indirect lighting
		// MaterialAmbientColor +
		// Diffuse : "color" of the object
		MaterialDiffuseColor * LightColor * LightPower * cosTheta / (distance * distance) +
		// Specular : reflective highlight, like a mirror
		MaterialSpecularColor * LightColor * LightPower * pow(cosAlpha,5.0) / (distance * distance);
	gl_FragColor = vec4(color, 1.0);
}