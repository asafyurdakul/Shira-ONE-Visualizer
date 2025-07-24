#version 330

const int MAX_TEXTURES = 10; //maximum number of textures we can have

uniform int numValidTextures = 1;

uniform sampler3D textures[MAX_TEXTURES]; //All of the nested textures
uniform mat4 texture_transform[MAX_TEXTURES]; //For rotating and translating the nested volumes
uniform mat4 texture_iTransform[MAX_TEXTURES]; //inverse transform
uniform int texture_replace[MAX_TEXTURES]; //Should the texture replace any that are higher order,  or add to them
uniform float texture_blend[MAX_TEXTURES]; //the amount of blending (0->1) with the textures of higher order
uniform float texture_jscale[MAX_TEXTURES]; //scales the emission of the texture
uniform float texture_kscale[MAX_TEXTURES]; //scales the absorption of the texture

//For normalization
uniform int texture_normalize = 0; //Should we normalize the textures to 256
uniform float texture_norm_grey = 1; //max grey value to normalize texture to if required
uniform float texture_norm_alpha = 1; //max alpha value to normalize texture to if required
uniform float texture_norm_exp = 1; //exponent for normalization mapping

uniform float jScale = 1;
uniform float kScale = 1;
uniform float ds0 = .01; //The step size

uniform vec3 backgroundColor = vec3(0,0,0);
uniform float exposure = 0.0; // Exposure control (e.g., -2.0 to +2.0 for

out vec4 fragColor;

in vec3 cameraPos;
in vec3 rayDir;

bool isOrtho = false;
float boxSize = 0.5; //using slightly less than .5 removes dots from result when camera is very close in

vec3 box_min0 = -vec3(boxSize);
vec3 box_max0 = +vec3(boxSize);

//------------------------------------------------------------------------------
// EMITTER Properties
//------------------------------------------------------------------------------

const int MAX_EMITTERS = 10; //maximum number stars we can support

uniform sampler3D shadowTextures[MAX_EMITTERS]; //Texture holding beam intensities from the star for scattering
uniform vec3 emitterPositions[MAX_EMITTERS]; //star positions
uniform float emitterSizes[MAX_EMITTERS]; //star radii
uniform vec3 emitterColors[MAX_EMITTERS]; //star colors
uniform int numEmitters = 0; //how many emitters do we have?
uniform float phase_g = 0; //phase factor
uniform float scatterBrightness = 1; //scattering brightness factor

//------------------------------------------------------------------------------
//Star Properties
//------------------------------------------------------------------------------

struct Star
{
    vec3 pos;
    float size;
    vec3 color;
    float mag;
    float brightness;
    bool valid;
};

uniform sampler3D star_index_tex;  //Gives the star index at the given 3D position
uniform sampler3D star_info_tex; //Contains the attributes for the stars
uniform float star_brightness; //Contains the brightness scale for each star texture

ivec3 getIndex3D(int i, int xSize, int ySize, int zSize)
{
    ivec3 p;

    p.z = (i / (xSize * ySize));

    i -= p.z * (xSize * ySize);

    p.y = (i / xSize);

    i -= p.y * (xSize);

    p.x = int(i);

    return (p);
}

ivec3 getStarInfoIndex(int starIndex, int attNum)
{
    ivec3 tSize = textureSize(star_info_tex, 0);

    //Convert the linear star index into a 2D index into the texture
    ivec3 starIndex3D = getIndex3D(starIndex, tSize.y, tSize.z, 1);

    ivec3 index = ivec3(attNum, starIndex3D.x, starIndex3D.y);
    return (index);
}

/**
 * Returns the closest star at the given position
 * @param pos
 * @return
 */
void getStar(vec3 pos, inout Star star)
{
    star.valid = false;

    if(star_brightness <= 0)
        return;

    //Get the size of the index texture
    ivec3 tSize = textureSize(star_index_tex, 0);
    ivec3 iSize = textureSize(star_info_tex, 0);

    //Get the value in the index texture
    int starIndex = int(texture(star_index_tex, pos+0.5).r - 1);

    //No star here
    if(starIndex < 0)
        return;

    int numAttributes = 8;

    float xPos = texelFetch(star_info_tex, getStarInfoIndex(starIndex,0), 0).r - 0.5;
    float yPos = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,1), 0).r - 0.5;
    float zPos = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,2), 0).r - 0.5;

    float mag = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,3), 0).r;
    float rad = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,4), 0).r;
    float r = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,5), 0).r;
    float g = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,6), 0).r;
    float b = texelFetch(star_info_tex,  getStarInfoIndex(starIndex,7), 0).r;

    star.pos = vec3(xPos, yPos, zPos);
    star.size = rad;
    star.mag = mag;
    star.brightness = star_brightness;
    star.color = vec3(r, g, b);
    star.valid = true;

}

vec4 addStar(Star star, vec3 pos, vec3 cameraPos)
{
    float d = distance(pos, star.pos); //distance from the ray to the star center
    float dist = distance(cameraPos, star.pos); //distance of camera to star

    float dR = d / (star.size); //normalized distance from the star center

    float m = star.mag; //magnitude of star
    float m_a = 7.0; //average magnitude
    float delta_m = pow(2.512, m_a - m); //difference in magnitude

    //We want some extent to the center, not just a pin prick
    float mindR = .05 ;
    dR = max(mindR, dR);

    float v_t = 1.167;
    float cq = 1;
    float q = dR;
    float i_t = (cq / v_t) * delta_m / (q * q);
    i_t = min(v_t, i_t);
    float i_g = delta_m * (m + (v_t - 1)) - 1;

    float glare_scale = 1;
    float t = 1.0 - smoothstep(0.0, 1.0, dR);
    float g = 1.0 - pow(dR, glare_scale / 64.0);

    float k = max(sqrt(i_g), q);
    float intensity = i_t * t * (k / q);
    float glare = i_g * g;

    float scale = star.brightness;

    //Scale by distance of the camera to the star
    float v = intensity * glare * scale;
    vec3 v_vec = vec3(star.color * v);

    vec4 rVec = vec4(0);
    rVec.rgb = v_vec;
    rVec.a = 0;//intensity*1;

    return rVec;
}

//------------------------------------------------------------------------------


float custom_min(float x, float y)
{
    if(x < y)
        return(x);

    return(y);
}

vec3 custom_min(vec3 v0, vec3 v1)
{
    vec3 m;
    m.x = custom_min(v0.x, v1.x);
    m.y = custom_min(v0.y, v1.y);
    m.z = custom_min(v0.z, v1.z);

    return(m);
}

float custom_max(float x, float y)
{
    if(x > y)
        return(x);

    return(y);
}

vec3 custom_max(vec3 v0, vec3 v1)
{
    vec3 m;
    m.x = custom_max(v0.x, v1.x);
    m.y = custom_max(v0.y, v1.y);
    m.z = custom_max(v0.z, v1.z);

    return(m);
}

//No intersection if t1 >= t0
vec2 intersect_box(vec3 orig, vec3 dir, vec3 box_min, vec3 box_max)
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

bool isValidPosition(vec3 pos)
{
     return!(((pos.x < 0 || pos.x > 1) || (pos.y < 0 || pos.y > 1) || (pos.z < 0 || pos.z > 1)));
}


vec3 getPosition(vec3 ray_origin, vec3 ray_direction, float t)
{
    vec3 pos = (ray_origin + ray_direction * t);
    return(pos);
}

/**
 * This function provides a transparency based on where the p vector lies within a unit box.  At 0 it has full transparency, at transition_area
 * it has no transparency
 * @param p
 * @param transition_area
 * @return
 */
float blendFactor(vec3 p, float transition_area)
{
    if(transition_area <= 0)
        return(1);

    if(transition_area >= 1)
        return(0);

    vec3 bottomLeft = vec3(0, 0, 0);
    vec3 topRight = vec3(1.0, 1.0, 1.0);

    vec3 s0 = smoothstep(bottomLeft, bottomLeft + vec3(transition_area), p);
    vec3 s1 = smoothstep(topRight - vec3(transition_area), topRight, p);

    vec3 sv = s0-s1;

    float s = abs(sv.x * sv.y * sv.z);
    s = custom_max(s, 0);
    return(s);
}

vec4 getTexture(int textureIndex, vec3 texPos)
{
    vec4 jE = texture(textures[textureIndex], texPos);
    if(texture_normalize == 1)
    {
        jE.rgb /= texture_norm_grey;
        jE.r = 255.0  * pow(jE.r, texture_norm_exp);
        jE.g = 255.0  * pow(jE.g, texture_norm_exp);
        jE.b = 255.0  * pow(jE.b, texture_norm_exp);

        jE.a /= texture_norm_alpha;
        jE.a = 255.0 * pow(jE.a, texture_norm_exp);

        jE = floor(jE);
        jE = clamp(jE, 0, 255.0);
    }


    //the following is for debugging without interpolation
   // ivec3 tSize = textureSize(textures[textureIndex], 0);
    //vec4 jE = texelFetch(textures[textureIndex], ivec3(tSize*texPos), 0);

    return(jE);
}

/**
 * Returns the angle in radiants between the two vectors
 */
float angle(vec3 v0, vec3 v1)
{
    float dotProduct = dot(v0, v1);
    float mag = length(v0) * length(v1);

    float cosTheta = dotProduct / mag;
    float theta = acos(cosTheta);
    return (theta);

}

/**
 Calculates the phase function for the given angle
 */
float phaseFunction(float theta)
{
    float g = phase_g;
    float v = (1 - g * g) / pow((1 + g * g - 2 * g * cos(theta)),  3.0 / 2.0);
    return (v);
}


// Yeni: Yüzey normalini yaklaşık olarak hesapla (dokudan gradient ile)
vec3 calculateNormal(vec3 pos, int textureIndex, float ds) {
    vec3 texPos = (texture_transform[textureIndex] * vec4(pos, 1)).xyz + 0.5;
    if (!isValidPosition(texPos)) return vec3(0.0);

    // Merkezi fark yöntemiyle normal hesaplama
    float delta = ds * 0.5;
    vec4 jE_x0 = getTexture(textureIndex, texPos - vec3(delta, 0, 0));
    vec4 jE_x1 = getTexture(textureIndex, texPos + vec3(delta, 0, 0));
    vec4 jE_y0 = getTexture(textureIndex, texPos - vec3(0, delta, 0));
    vec4 jE_y1 = getTexture(textureIndex, texPos + vec3(0, delta, 0));
    vec4 jE_z0 = getTexture(textureIndex, texPos - vec3(0, 0, delta));
    vec4 jE_z1 = getTexture(textureIndex, texPos + vec3(0, 0, delta));

    // Yoğunluk (alpha) üzerinden gradient
    vec3 normal = vec3(
        jE_x1.a - jE_x0.a,
        jE_y1.a - jE_y0.a,
        jE_z1.a - jE_z0.a
    );
    return normalize(normal);
}

vec3 scatter(vec3 position, vec4 jE)
{
    vec3 texPos = position + 0.5; //Figure out where we need to sample the texture from
    vec3 cameraDir = cameraPos - position;
    vec3 alphaTotal = vec3(0);

    for(int i = 0; i < MAX_EMITTERS; i++)
    {
        if(i >= numEmitters)
            break;

        vec3 emitterPos = emitterPositions[i];
        vec3 emitterDir =  position-(emitterPos - 0.5);
        float d = length(emitterDir);
        float r = emitterSizes[i];

        float dropOffFactor = pow((d/r+r),-2);
        if(d < r)
            dropOffFactor *= d/r;

        float theta = angle(cameraDir, emitterDir);
        float pF = phaseFunction(theta);

        vec4 IEmm = texture(shadowTextures[i], texPos);      //star emission at this point
        vec3 alpha = IEmm.rgb * pF * dropOffFactor;
        alpha = max(vec3(0), alpha);
        alphaTotal += alpha;
    }

    vec3 ks = vec3(jE.a);
    //Rayleigh scattering
    vec3 bands = vec3(687.5, 532.5, 467.5);
    bands = normalize(bands);
    float scatteringPow = -4;

    bands.x = pow(bands.x, scatteringPow);
    bands.y = pow(bands.y, scatteringPow);
    bands.z = pow(bands.z, scatteringPow);

    float norm = length(bands);
    ks *= bands /norm;

    vec3 jP = alphaTotal * ks * scatterBrightness;

    return(jP);
}

/**
 Returns the RGBA values in the volume for the given position
 */
vec4 getJ(in vec3 position, inout float ds)
{
    vec4 jE = vec4(0); //final emission we return
    float maxScale = 1.0; //the maximum texture scale, we must adjust ds by this

    vec4 blend0 = vec4(0);

    //The total weight
    float totalWeight = 1;

    vec4 jEUnscaled = vec4(0); //unscaled, to be passed to scattering

    //Texture are sorted from low order to high order.  We start at the highest order and go down.
    for (int i = 0; i < numValidTextures; i++)
    {
        mat4 matrix = texture_transform[i]; //Any rotations or translations of the texture.
        vec3 texPos = (matrix * vec4(position, 1)).xyz + 0.5; //Figure out where we need to sample the texture from

        //Find out if we should render it.  If the position is outside of the cube (after our transformation above)
        //then the texture does not cover this space.
        bool valid = isValidPosition(texPos);
        if(!valid)
            continue;

        float scale = matrix[0][0]; //This will be the scale of the volume

        //Figure out the transparency of this point based on distance from edge of volume
        float weight = blendFactor(texPos, texture_blend[i]) * totalWeight;

        vec4 jETex = getTexture(i, texPos).brga;     //Get the texture value at this position
        jETex *= weight; //multiply by the transparency.

        jEUnscaled += jETex;

        jETex.rgb *= texture_jscale[i]; //Scale by any emission factors
        jETex.a *= texture_kscale[i]; //Scale by any absorption factors

        jE += jETex;

        //keep track of this scale, we need to take smaller ray steps if we are inside a low order texture in order to get higher resolution
        maxScale = custom_max(scale, maxScale);

        totalWeight -= weight;

        if(texture_replace[i] == 1 && totalWeight <= 0)
        {
            break;
        }
    }

    //Now calculate the new ray step size based on which textures we passed through.
    ds = ds * custom_min(1.0, 1.0 / maxScale)*0.5;

    //Add any scattering
    jE.rgb += scatter(position, jEUnscaled);

    //Now do the stars
     vec4 starjE = vec4(0);

    Star star;
    getStar(position, star);
    if(star.valid)
        starjE += addStar(star, position, cameraPos);

    jE += starjE;

    return (jE);
}



vec3 transfer(vec3 I0, vec3 jE, float k, float ds)
{
    //The k == 0 case
    if(k < 1E-3)  //having this lower sometimes adds a funny black line around the nested regions
    {
        vec3 IE = (I0.rgb + jE.rgb * ds);
        return(IE);
    }

    //The k != 0 case
    float tau = k * ds;
    float etau = exp(-tau);
    vec3 S = jE.rgb /k;
    vec3 IA = (I0.rgb*etau + S*(1.0-etau));

    return(IA);
}

//Transfers the ray from t0 to t1 and returns the resulting intensity
vec3 rayTransfer(vec3 ray_origin, vec3 ray_direction, vec3 I0, float t0, float t1)
{
    float ray_length = (t1-t0);
    float ds = ds0;

    //The final intensity of the ray
    vec3 I = vec3(I0);

    //The previous step
    float tPrev = t1;

    float dsInit = ds;

    //Adjust the step size for distance to volume.  The further away we are, the larger the step sizes can be.
    if(!isOrtho)
        dsInit *= (1+t0/0.5);

    for(float t = t1; t >= t0; t -= ds)
    {
        ds = dsInit;
        vec3 position = getPosition(ray_origin, ray_direction, t);
        vec4 jE = getJ(position, ds);

        //If there is no emission/absorption, we can just keep going.  try to speed up a little
        if(length(jE) == 0)
        {
            ds = ds * 2.0; //adjust this lower if you see artifacts
            continue;
        }

        //make sure we are not at 0
        ds = max(.00001, ds) ;

        //Add the emission from this cell
        I = transfer(I, jE.rgb*jScale, jE.a*kScale, ds);
    }

    return(I);
}

void sortIntersections(inout float array[MAX_TEXTURES * 2], int size)
{
    for (int j = 1; j < size; ++j)
    {
        float key = array[j];

        int i = j - 1;
        while (i >= 0 && array[i] > key)
        {
                array[i+1] = array[i];
                --i;
        }

        array[i+1] = key;
    }
}

void main()
{
   //The ray
    vec3 ray_origin = cameraPos*0.5; //this uses the whole volume
    vec3 ray_direction = normalize(rayDir);
    vec4 fragCoord = gl_FragCoord;

    //Normalize to screen
    fragCoord.x *= 1.0/640;
    fragCoord.y *= 1.0/480;
    //distance of pixel from center, r = 0 to 1
    float r = length(fragCoord.xy-0.5);

    float array[MAX_TEXTURES * 2];
    int numTs = 0;

    //If we have stars around, we have to cover the entire volume
    if(star_brightness > 0)
    {
        vec2 intersectionArray =  intersect_box(ray_origin, ray_direction, box_min0, box_max0);

        float t = intersectionArray[0];
        float s = intersectionArray[1];

        //No intersection so we don't need this volume
        if(t >= s)
        {
            discard;
        }

        array[numTs++] = t;
        array[numTs++] = s;
    }

    else
    {

        for(int i = 0; i < numValidTextures; i++)
        {
             //Figure out where this volume intersects the ray
            mat4 iMatrix =  texture_iTransform[i];
            vec3 box_min = (iMatrix * vec4(box_min0, 1) ).xyz;
            vec3 box_max = (iMatrix * vec4(box_max0, 1) ).xyz;

            vec2 intersectionArray =  intersect_box(ray_origin, ray_direction, box_min, box_max);

            float t = intersectionArray[0];
            float s = intersectionArray[1];

            //No intersection so we don't need this volume
            if(t >= s)
            {
                continue;
            }

            array[numTs++] = t;
            array[numTs++] = s;
        }


    }

    //No volume intersections.  See if there are stars around
    if(numTs == 0)
    {
        discard;
    }

    sortIntersections(array, numTs);

    //If the first hit is beyond 10, we can assume we are ortho
    if(array[0] > 10.0)
        isOrtho = true;

    vec3 I = backgroundColor;
    //Now we cast a ray for each pair of intersections (i.e. volumes) that we travel through
    for(int i = numTs-1; i > 0; i--)
    {
        int index0 = i-1;
        int index1 = i;

        float t0 = array[index0];
        float t1 = array[index1];

        I = rayTransfer(ray_origin, ray_direction, I, t0, t1);
    }

    fragColor = vec4(I * exposure, 1);
}
