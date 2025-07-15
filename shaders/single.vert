#version 330

layout (location=0) in vec3 position;

uniform mat4 viewProjectionMatrix;
uniform mat4 inverseViewMatrix;

out vec4 projectedCoords;
out vec3 cameraPos;
out vec3 rayDir;
out vec3 vertexPos;

void main()
{
    vertexPos = vec3(position);
    projectedCoords =  viewProjectionMatrix * vec4(vertexPos,1);
    cameraPos = (inverseViewMatrix * vec4(0,0,0,1)).xyz;
    rayDir = (vertexPos - cameraPos);

    gl_Position = projectedCoords;
}
