#version 120
// Animated atmospheric effects over the original battle artwork.
uniform sampler2D uScene;
uniform sampler2D uMask;
uniform vec2 uResolution;
uniform float uTime, uWeather, uTerrain, uSuppressed, uFade, uDoubles;
varying vec2 vUv;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(i), hash(i+vec2(1.,0.)), f.x),
               mix(hash(i+vec2(0.,1.)), hash(i+vec2(1.,1.)), f.x), f.y);
}
float cloud(vec2 p) { return noise(p)*0.62 + noise(p*2.03+17.4)*0.27 + noise(p*4.1-8.7)*0.11; }
float glow(float d, float radius) { return 1.0-smoothstep(0.0, radius, abs(d)); }
float rainLayer(vec2 p, float t, vec2 size, float speed, float seed) {
    vec2 q = vec2(p.x + p.y*0.24 + t*speed*0.24, p.y + t*speed);
    vec2 cell = floor(q/size), f = fract(q/size);
    float r = hash(cell+seed);
    float x = glow(f.x-(0.22+r*0.56), 0.08);
    float y = smoothstep(0.18,0.34,f.y)*(1.0-smoothstep(0.78,0.94,f.y));
    return x*y*step(0.27,r);
}
float flakes(vec2 p, float t, float size, float speed, float seed, float radius) {
    vec2 q=vec2(p.x + sin(t*0.7+p.y*0.035+seed)*3.5+t*5.0, p.y-t*speed);
    vec2 cell=floor(q/size), local=mod(q,size);
    float r=hash(cell+seed);
    vec2 pos=vec2(2.0+r*(size-4.0),2.0+hash(cell+seed+19.0)*(size-4.0));
    return (1.0-smoothstep(radius*0.45,radius,length(local-pos)))*step(0.18,r);
}
void main() {
    vec2 uv=(floor(vUv*uResolution)+0.5)/uResolution;
    vec2 p=floor(uv*uResolution);
    vec3 base=texture2D(uScene,uv).rgb, c=base;
    float layer=texture2D(uMask,uv).r;
    float visible=step(0.25,layer);
    if(visible<0.5 || uFade<=0.) { gl_FragColor=vec4(base,1.); return; }
    float t=uTime;
    // Battle backdrops combine sky and floor in one image. A feathered horizon
    // covers the entire visible floor while excluding sprites and the UI.
    float ground=smoothstep(38.,62.,p.y)*step(0.75,layer);
    vec2 surface=vec2(p.x/120., p.y/48.);

    if(uTerrain>0.5 && uTerrain<1.5) {
        float breeze=cloud(surface*2.1+vec2(-t*0.12,t*0.035));
        float veins=pow(max(0.,sin(p.x*0.12+p.y*0.20+noise(surface*3.)*2.0-t*0.55)),8.);
        float pollen=pow(max(0.,sin(p.x*0.09-p.y*0.16+t*0.8)),18.)*step(0.65,noise(surface*7.));
        c=mix(c,vec3(0.12,0.68,0.29),ground*(0.31+0.11*breeze));
        c=mix(c,vec3(0.65,1.0,0.46),ground*(veins*0.18+pollen*0.13));
    } else if(uTerrain>1.5 && uTerrain<2.5) {
        float mist=cloud(surface*2.5+vec2(-t*0.13,t*0.035));
        float ribbon=pow(max(0.,sin(p.y*0.13+noise(vec2(p.x*0.025,t*0.12))*3.0-t*0.45)),5.);
        c=mix(c,vec3(0.94,0.53,0.79),ground*(0.26+0.16*mist));
        c=mix(c,vec3(1.0,0.86,0.96),ground*ribbon*0.18);
    } else if(uTerrain>2.5 && uTerrain<3.5) {
        float current=sin(p.x*0.075+p.y*0.16+noise(surface*4.+vec2(t*0.08,0.))*4.0-t*1.1);
        float trace=pow(max(0.,current),16.);
        float shimmer=cloud(surface*4.+vec2(t*0.18,0.));
        c=mix(c,vec3(0.96,0.76,0.08),ground*(0.29+0.10*shimmer));
        c=mix(c,vec3(1.0,0.98,0.50),ground*trace*0.34);
    } else if(uTerrain>3.5) {
        vec2 q=(p-vec2(120.,62.))/vec2(1.,0.48);
        float rings=pow(max(0.,cos(length(q)*0.075-t*1.05)),12.);
        float veil=cloud(surface*2.6+vec2(t*0.07,-t*0.05));
        c=mix(c,vec3(0.64,0.22,0.78),ground*(0.28+0.12*veil));
        c=mix(c,vec3(0.96,0.67,1.0),ground*rings*0.28);
    }

    float w=uSuppressed>0.5 ? 0. : uWeather;
    if((w>0.5 && w<1.5) || w>8.5) {
        float heavy=step(8.5,w);
        float farRain=rainLayer(p,t,vec2(19.,29.),100.,3.0);
        float nearRain=rainLayer(p,t,vec2(12.,22.),155.,21.0);
        float haze=cloud(p*0.014+vec2(-t*0.08,t*0.025));
        c=mix(c,vec3(0.32,0.46,0.58),0.07+heavy*0.05+haze*0.035);
        c=mix(c,vec3(0.76,0.89,0.98),clamp(farRain*0.21+nearRain*(0.31+heavy*0.12),0.,0.55));
        float sheen=pow(max(0.,sin(p.x*0.10+p.y*0.24-t*1.9)),10.);
        c=mix(c,vec3(0.72,0.88,0.96),ground*sheen*0.06);
    } else if((w>1.5 && w<2.5) || (w>7.5 && w<8.5)) {
        float harsh=step(7.5,w);
        float shafts=pow(max(0.,sin(p.x*0.026+p.y*0.039+t*0.15)),4.);
        float warmth=0.15+shafts*0.17+harsh*0.09;
        c=mix(c,vec3(1.0,0.75-harsh*0.09,0.35-harsh*0.08),warmth);
        c+=vec3(0.055,0.038,0.012)*shafts;
    } else if(w>2.5 && w<3.5) {
        float dust=cloud(vec2(p.x*0.018-t*0.45,p.y*0.035+t*0.07));
        float bands=pow(max(0.,sin(p.y*0.11+noise(vec2(p.x*0.015-t*0.3,p.y*0.015))*2.5-t*1.1)),5.);
        float grain=step(0.965,hash(floor((p+vec2(t*75.,-t*10.))/3.)));
        c=mix(c,vec3(0.72,0.55,0.31),0.08+dust*0.13+bands*0.035);
        c=mix(c,vec3(0.98,0.85,0.60),grain*0.25);
    } else if(w>3.5 && w<5.5) {
        float snow=step(4.5,w);
        float farFlakes=flakes(p,t,19.,snow>0.5?23.:95.,4.,snow>0.5?1.2:0.8);
        float nearFlakes=flakes(p,t,28.,snow>0.5?38.:125.,23.,snow>0.5?2.1:1.2);
        c=mix(c,vec3(0.62,0.76,0.87),0.055+snow*0.025);
        c=mix(c,vec3(0.94,0.98,1.0),clamp(farFlakes*0.45+nearFlakes*0.70,0.,0.8));
    } else if(w>5.5 && w<6.5) {
        float fog=cloud(vec2(p.x*0.015-t*0.10,p.y*0.035+t*0.025));
        float low=cloud(vec2(p.x*0.010+t*0.075,p.y*0.022-t*0.03));
        float bank=0.5+0.5*cos(p.y*0.075+sin(p.x*0.015-t*0.18)*1.3+t*0.16);
        bank=smoothstep(0.38,0.85,bank)*smoothstep(0.28,0.68,fog);
        c=mix(c,vec3(0.66,0.75,0.80),0.20+fog*0.22+low*0.12+bank*0.14);
        c=mix(c,vec3(0.95,0.98,0.98),bank*0.14);
    } else if(w>6.5 && w<7.5) {
        float stream=p.y+5.0*sin(p.x*0.024-t*1.15)+2.0*sin(p.x*0.052+t*0.55);
        float lane=floor(stream/23.0);
        float distance=abs(fract(stream/23.0)-0.5);
        float core=1.0-smoothstep(0.025,0.095,distance);
        float halo=1.0-smoothstep(0.06,0.22,distance);
        float phase=fract(p.x*0.014-t*0.55+hash(vec2(lane,7.0)));
        float segment=smoothstep(0.03,0.15,phase)*(1.0-smoothstep(0.48,0.88,phase));
        float gust=segment*(core*0.53+halo*0.15);
        c=mix(c,vec3(0.76,0.94,0.96),clamp(gust,0.,0.68));
    }
    gl_FragColor=vec4(mix(base,clamp(c,0.,1.),visible*uFade),1.);
}
