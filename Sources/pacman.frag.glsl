#version 450

uniform vec4 time;

out vec4 frag;

void main() {
	// Not much here yet. Feel free to port over other lighting models to this shader.
	frag = vec4(1.0, 1.0, 0.0, 1.0);
}
