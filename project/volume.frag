#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;



uniform vec3 camera_pos;


ivec3 volumeSize;

layout(location = 0) out vec4 fragmentColor;

layout(binding = 8) uniform sampler3D gridTex;

in vec3 position;
in vec4 transformedPos;
in vec3 viewSpacePosition;


float max3 (vec3 v) {
  return max (max (abs(v.x), abs(v.y)), abs(v.z));
}


vec4 simpleMarch(vec3 viewVector, vec3 start, ivec3 volumeSize){
    float inVolumeLength = max3(volumeSize);
    viewVector = viewVector / max3(viewVector);
    int maxIter = 30;
    viewVector = (viewVector * inVolumeLength) / maxIter;
    //viewVector /= 2;

    float alpha = 0.0f;
    vec3 current = start + viewVector/2;
    float red = 1.0f;
    float green = 1.0f;

    for(int i = 0; i<maxIter; i++){
        
        if(current.x < -volumeSize.x/2 || current.x > volumeSize.x/2 ||
           current.y < -volumeSize.y/2 || current.y > volumeSize.y/2 ||
           current.z < -volumeSize.z/2 || current.z > volumeSize.z/2 ){
            break;
        }

        float a = texture(gridTex, (current/(volumeSize/2)+1)/2).r;
        if(a > 0.0f) red = 0.0f;
        if(a < 0.0f) green = 0.0f;

        vec3 dist = abs(floor(current+volumeSize/2)+0.5f - (current+volumeSize/2)) *2;
        alpha += (1-sqrt(pow(dist.x,2) + pow(dist.y,2) + pow(dist.z,2)))*abs(a);
        
        
        /*if(texture(gridTex, (current/(volumeSize/2)+1)/2).r >0.1f){
            //alpha += 0.2f;
            //alpha = 1.0f;
            vec3 dist = abs(floor(current+volumeSize/2)+0.5f - (current+volumeSize/2)) *2;
            alpha += (1-sqrt(pow(dist.x,2) + pow(dist.y,2) + pow(dist.z,2))) * (a);
            
        }*/

        //if(alpha >= 1.0f) break;

        current += viewVector;
    }

    return vec4(red, green, 1.0f, alpha);
}


void main(){

    volumeSize = textureSize(gridTex, 0);

	vec3 viewVector = normalize(position*(volumeSize/2)-camera_pos);


    
	fragmentColor = simpleMarch(viewVector, position*(volumeSize/2), volumeSize);
    return;
    


//    vec3 col =  vec3(texture(gridTex, vec3((position+1)/2)).r);
//    if(col.x < 0.1f)
//		fragmentColor = vec4((-1*position+1)/2, 0.1f);
//	else
//		fragmentColor = vec4(vec3((position+1)/2), 1.0f);
	
}


