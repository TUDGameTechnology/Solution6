#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D tex;
uniform sampler2D normalMap;


uniform vec3 light;
uniform vec3 eye;


varying vec3 position;
varying vec2 texCoord;
varying vec3 normal;

varying vec3 LightDirection_tangentspace;
varying vec3 EyeDirection_tangentspace;

void kore(){

	// Light emission properties
	// You probably want to put them as uniforms
	vec3 LightColor = vec3(1,1,1);
	float LightPower = 40.0;
	
	// Material properties
	vec3 MaterialDiffuseColor = texture2D(tex, texCoord).rgb;
	vec3 MaterialAmbientColor = vec3(0.1,0.1,0.1) * MaterialDiffuseColor;
	vec3 MaterialSpecularColor = texture2D(tex, texCoord ).rgb * 0.3;

	// Local normal, in tangent space. V tex coordinate is inverted because normal map is in TGA (not in DDS) for better quality
	//@@TODO: Check for this
	vec3 TextureNormal_tangentspace = normalize(texture2D(normalMap, vec2(texCoord.x,-texCoord.y)).rgb*2.0 - 1.0);
	
	// Distance to the light
	float distance = length( light - position );
	 
	// Normal of the computed fragment, in camera space
	vec3 n = TextureNormal_tangentspace;

	
	// Direction of the light (from the fragment to the light)
	vec3 l = normalize(LightDirection_tangentspace);
	// Cosine of the angle between the normal and the light direction, 
	// clamped above 0
	//  - light is at the vertical of the triangle -> 1
	//  - light is perpendicular to the triangle -> 0
	//  - light is behind the triangle -> 0
	//@@TODO: Why no clamp?
	// float cosTheta = clamp( dot( n,l ), 0,1 );
	float cosTheta = dot( n,l );
	if (cosTheta < 0.0) cosTheta = 0.0;
	if (cosTheta > 1.0) cosTheta = 1.0;

	
	// Eye vector (towards the camera)
	vec3 E = normalize(EyeDirection_tangentspace);
	// Direction in which the triangle reflects the light
	vec3 R = reflect(-l,n);
	// Cosine of the angle between the Eye vector and the Reflect vector,
	// clamped to 0
	//  - Looking into the reflection -> 1
	//  - Looking elsewhere -> < 1
	float cosAlpha = clamp( dot( E,R ), 0.0,1.0);

	
	vec3 color = 
		// Ambient : simulates indirect lighting
		MaterialAmbientColor +
		// Diffuse : "color" of the object
		MaterialDiffuseColor * LightColor * LightPower * cosTheta / (distance*distance) +
		// Specular : reflective highlight, like a mirror
		MaterialSpecularColor * LightColor * LightPower * pow(cosAlpha,5.0) / (distance*distance);

	gl_FragColor = vec4(MaterialDiffuseColor, 1);
}