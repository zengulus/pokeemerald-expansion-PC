#!/usr/bin/env python3
"""Make animated GIF galleries from actual GLSL readbacks; no dependencies."""
import argparse, html, json, struct, zlib
from collections import Counter
from pathlib import Path
from gif_encode import Palette, histogram, write_gif
FONT = {
'A':'01110 10001 10001 11111 10001 10001 10001','B':'11110 10001 10001 11110 10001 10001 11110',
'C':'01111 10000 10000 10000 10000 10000 01111','D':'11110 10001 10001 10001 10001 10001 11110',
'E':'11111 10000 10000 11110 10000 10000 11111','F':'11111 10000 10000 11110 10000 10000 10000',
'G':'01111 10000 10000 10111 10001 10001 01111','H':'10001 10001 10001 11111 10001 10001 10001',
'I':'11111 00100 00100 00100 00100 00100 11111','J':'00111 00010 00010 00010 10010 10010 01100',
'K':'10001 10010 10100 11000 10100 10010 10001','L':'10000 10000 10000 10000 10000 10000 11111',
'M':'10001 11011 10101 10101 10001 10001 10001','N':'10001 11001 10101 10011 10001 10001 10001',
'O':'01110 10001 10001 10001 10001 10001 01110','P':'11110 10001 10001 11110 10000 10000 10000',
'Q':'01110 10001 10001 10001 10101 10010 01101','R':'11110 10001 10001 11110 10100 10010 10001',
'S':'01111 10000 10000 01110 00001 00001 11110','T':'11111 00100 00100 00100 00100 00100 00100',
'U':'10001 10001 10001 10001 10001 10001 01110','V':'10001 10001 10001 10001 10001 01010 00100',
'W':'10001 10001 10001 10101 10101 10101 01010','X':'10001 10001 01010 00100 01010 10001 10001',
'Y':'10001 10001 01010 00100 00100 00100 00100','Z':'11111 00001 00010 00100 01000 10000 11111',
'+':'00000 00100 00100 11111 00100 00100 00000','-':'00000 00000 00000 11111 00000 00000 00000',
' ':'00000 00000 00000 00000 00000 00000 00000'}
def png(path, data, width, height):
    def chunk(tag, payload):
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I',zlib.crc32(tag+payload))
    scan=b''.join(b'\0'+data[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b''))
def text(data,width,x,y,label):
    for letter in label.upper():
        for dy,row in enumerate(FONT.get(letter,FONT[' ']).split()):
            for dx,value in enumerate(row):
                if value=='1':
                    for yy in range(2):
                        for xx in range(2):
                            at=((y+dy*2+yy)*width+x+dx*2+xx)*3
                            data[at:at+3]=bytes((219,229,239))
        x+=12

def animated_galleries(root):
    videos={p.stem:p.read_bytes() for p in sorted(root.glob('*.rgbv'))}
    frame_bytes=240*160*3
    assert videos, 'Run environment-gallery to capture animation sequences first'
    counts={len(data)//frame_bytes for data in videos.values()}
    assert len(counts)==1 and all(len(data)%frame_bytes==0 for data in videos.values())
    count=counts.pop(); assert count>1
    scene=(root/'source.scene').read_bytes()
    assert scene[:8]==b'RHHSCN01' and len(scene)==8+240*160*3
    mask=scene[8+240*160*2:]
    baseline=videos['clear'][:frame_bytes]
    reserved=list(dict.fromkeys(tuple(baseline[p*3:p*3+3])
                                for p in range(240*160) if mask[p]!=255))
    reserved.extend(((17,23,33),(219,229,239)))
    histograms={key:histogram(data) for key,data in videos.items()}
    files=[]
    for key,data in videos.items():
        palette=Palette(histograms[key],reserved)
        indexed=palette.indices(data)
        unique=len({bytes(indexed[i*240*160:(i+1)*240*160]) for i in range(count)})
        assert key=='clear' or unique>1, key+' GIF lost its animation'
        frames=(indexed[i*240*160:(i+1)*240*160] for i in range(count))
        assert write_gif(root/(key+'.gif'),240,160,frames,palette)==count
        files.append({'file':key+'.gif','width':240,'height':160,'frames':count,'unique_frames':unique})
        print(f'{key}.gif: {count} frames, {unique} distinct frames',flush=True)

    groups={
        'weather':(['rain','sun','sand','snow'],2),
        'terrain':(['grassy','misty','electric','psychic'],2),
        'combined':(['rain-grassy','sand-electric','snow-psychic','suppressed-rain-grassy'],2),
        'all-weather':(['rain','sun','sand','hail','snow','fog','wind','harsh-sun','heavy-rain'],3)}
    for name,(keys,columns) in groups.items():
        merged=Counter()
        for key in keys: merged.update(histograms[key])
        palette=Palette(merged,reserved)
        indexed={key:palette.indices(videos[key]) for key in keys}
        width=columns*248+8; height=((len(keys)+columns-1)//columns)*180+8
        canvas=bytearray([palette.exact[(17,23,33)]])*(width*height)
        for i,key in enumerate(keys):
            x,y=8+(i%columns)*248,8+(i//columns)*180
            for letter in key.replace('-',' ').upper():
                for dy,row in enumerate(FONT.get(letter,FONT[' ']).split()):
                    for dx,value in enumerate(row):
                        if value=='1': canvas[(y+dy)*width+x+dx]=palette.exact[(219,229,239)]
                x+=6
        def frames():
            for frame in range(count):
                result=canvas.copy()
                for i,key in enumerate(keys):
                    x,y=8+(i%columns)*248,20+(i//columns)*180
                    source=indexed[key]
                    for row in range(160):
                        at=((y+row)*width+x); start=frame*240*160+row*240
                        result[at:at+240]=source[start:start+240]
                yield result
        assert write_gif(root/(name+'.gif'),width,height,frames(),palette)==count
        files.append({'file':name+'.gif','width':width,'height':height,'frames':count})
        print(f'{name}.gif: animated comparison sheet',flush=True)
    return files,count

def main():
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);args=parser.parse_args();root=args.directory
    frames={p.stem:p.read_bytes() for p in root.glob('*.rgb')}
    assert frames and all(len(frame)==240*160*3 for frame in frames.values())
    assert frames['rain']!=frames['rain-motion'], 'Rain did not animate'
    assert frames['grassy']==frames['suppressed-rain-grassy'], 'Suppression left weather visible'
    for name,frame in frames.items(): png(root/(name+'.png'),frame,240,160)
    def sheet(name,items,columns=2):
        cellw,cellh=496,356;width=columns*cellw+16;height=((len(items)+columns-1)//columns)*cellh+16
        canvas=bytearray(bytes((17,23,33))*(width*height))
        for i,(key,label) in enumerate(items):
            x,y=16+(i%columns)*cellw,16+(i//columns)*cellh;text(canvas,width,x,y,label)
            frame=frames[key]
            for sy in range(160):
                row=b''.join(frame[(sy*240+sx)*3:(sy*240+sx+1)*3]*2 for sx in range(240))
                for repeat in range(2):
                    at=((y+24+sy*2+repeat)*width+x)*3;canvas[at:at+len(row)]=row
        png(root/(name+'.png'),canvas,width,height)
    sheet('weather',[('rain','RAIN'),('sun','SUN'),('sand','SANDSTORM'),('snow','SNOW')])
    sheet('terrain',[('grassy','GRASSY'),('misty','MISTY'),('electric','ELECTRIC'),('psychic','PSYCHIC')])
    sheet('combined',[('rain-grassy','RAIN + GRASSY'),('sand-electric','SAND + ELECTRIC'),('snow-psychic','SNOW + PSYCHIC'),('suppressed-rain-grassy','RAIN SUPPRESSED + GRASSY')])
    sheet('all-weather',[(key,key.replace('-',' ')) for key in ['rain','sun','sand','hail','snow','fog','wind','harsh-sun','heavy-rain']],3)
    animations,count=animated_galleries(root)
    keys=[entry['file'][:-4] for entry in animations if entry['file'] not in {'weather.gif','terrain.gif','combined.gif','all-weather.gif'}]
    cards=''.join('<figure><a href="'+html.escape(key)+'.gif"><img src="'+html.escape(key)+'.gif" alt="'+html.escape(key.replace('-',' '))+' animated shader"></a><figcaption>'+html.escape(key.replace('-',' ').title())+' · <a href="'+html.escape(key)+'.gif" download>Download GIF</a></figcaption></figure>' for key in keys)
    (root/'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>Battle environment critique gallery</title>
<style>body{background:#111721;color:#dbe5ef;font:16px system-ui;margin:32px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(480px,1fr));gap:20px}figure{margin:0}img{width:100%;image-rendering:pixelated}figcaption{padding:12px 0}p{max-width:850px;line-height:1.5}</style>
<h1>Animated battle environment shaders</h1><p>Looping GIFs rendered by the actual GLSL at 20 fps over the original grass battle background, captured in-game after the send-out animation finishes. Pokémon and the battle interface come from that real scene; the shader layer animates at normal game speed.</p>
<p><a href="weather.gif">Weather GIF</a> · <a href="terrain.gif">Terrain GIF</a> · <a href="combined.gif">Combined GIF</a> · <a href="all-weather.gif">All weather GIF</a></p>
<p>HP bars and dialogue are protected on every rendered frame. Cloud Nine / Air Lock suppress the weather layer while terrain remains.</p><main>'''+cards+'</main>')
    (root/'gallery.json').write_text(json.dumps({'source':'source.scene','source_report':'source.json','renderer_log':'gallery.log','controlled_preview':True,'start_frame':720,'fps':20,'frames':count,'duration_seconds':count/20,'background':'Original grass battle background, action menu after send-out','images':[entry['file'] for entry in animations],'animations':animations,'sheets':['weather.gif','terrain.gif','combined.gif','all-weather.gif'],'ui_preserved':True,'rain_animates':True,'suppressed_weather_leaves_terrain':True},indent=2)+'\n')
    print(root/'index.html')
if __name__=='__main__': main()
