#version 330

uniform sampler3D tex;

out vec4 fragColor;

uniform float jScale = 1;
uniform float kScale = 1;
uniform float ds0 = .01;

in vec3 cameraPos;
in vec3 rayDir;

float boxSize = 0.5; //using slightly less than .5 removes dots from result when camera is very close in

vec3 box_min = -vec3(boxSize);
vec3 box_max = +vec3(boxSize);

vec2 intersect_box(vec3 orig, vec3 dir)
{
    vec3 inv_dir = 1.0 / dir;
    vec3 tmin_tmp = (box_min - orig) * inv_dir;
    vec3 tmax_tmp = (box_max - orig) * inv_dir;
    vec3 tmin = min(tmin_tmp, tmax_tmp);
    vec3 tmax = max(tmin_tmp, tmax_tmp);
    float t0 = max(tmin.x, max(tmin.y, tmin.z));
    float t1 = min(tmax.x, min(tmax.y, tmax.z));

    t0 = max(t0,0.0); //this makes sure we don't get rays from behind the camera

    return vec2(t0, t1);
}

vec3 getPosition(vec3 ray_origin, vec3 ray_direction, float t)
{
    vec3 pos = (ray_origin + ray_direction * t);
    return(pos);
}

vec4 getJ(in vec3 position, vec3 ray_origin)
{
    vec3 texPos = position.xyz + (0.5);
    vec4 jE = texture(tex, texPos).bgra;
    return(jE);
}

float when_eq(float x, float y)
{
  return 1.0 - abs(sign(x - y));
}

vec3 transfer(vec3 I0, vec3 jE, float k, float ds)
{
    //The k == 0 case
    vec3 IE = when_eq(k,0) * (I0.rgb + jE.rgb * ds);

    //The k != 0 case
    float tau = k * ds;
    float etau = exp(-tau);
    vec3 S = jE.rgb /(k+.0000001);    //can't have divide by 0 here
    vec3 IA = (1 - when_eq(k,0)) * (I0.rgb*etau + S*(1.0-etau));

    return(IE + IA);
}

//Transfers the ray from t0 to t1 and returns the resulting intensity
vec3 rayTransfer(vec3 ray_origin, vec3 ray_direction, vec3 I0, float t0, float t1)
{
    float ray_length = (t1-t0);

    float ds = ds0;

    //The final intensity of the ray
    vec3 I = vec3(I0);

    //for(float t = t0; t <= t1; t += ds)
    for(float t = t1; t >= t0; t -= ds)
    {
        vec3 position = getPosition(ray_origin, ray_direction, t);
        vec4 jE = getJ(position, ray_origin);
        //Add the emission from this cell
        I = transfer(I, jE.rgb * jScale, jE.a * kScale, ds);
    }

    return(I);
}

void main()
{
   //The ray
    vec3 ray_origin = cameraPos*0.5; //this uses the whole volume
    vec3 ray_direction = normalize(rayDir);

    //Intersect our box with the ray
    vec2 tArray =  intersect_box(ray_origin, ray_direction);

    float t0 = tArray[0];
    float t1 = tArray[1];

    if (t0 >= t1)
    {
        discard;
    }

    vec3 I = rayTransfer(ray_origin, ray_direction, vec3(0,0,0), t0, t1);
    fragColor = vec4(I, 1);
}
