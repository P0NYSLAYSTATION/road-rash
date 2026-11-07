"""Local-only extraction. Never bundle output with the port."""
import argparse
import hashlib
import json
import math
import pathlib
import shutil
import struct
import subprocess
import wave

def eams_to_midi(data):
    """OMS EAMS tracks have note durations and a fixed 240 Hz audio clock."""
    tracks=[]
    def scan(blob):
        pos=12
        while pos+8<=len(blob):
            tag=blob[pos:pos+4];n=int.from_bytes(blob[pos+4:pos+8],'big')
            payload=blob[pos+8:pos+8+n]
            if tag==b'FORM':scan(blob[pos:pos+8+n])
            elif tag==b'TDat':tracks.append(payload)
            pos+=8+n+(n&1)
    scan(data)
    def encode(v):
        out=[v&127];v>>=7
        while v:out.insert(0,(v&127)|128);v>>=7
        return bytes(out)
    events=[];sequence=0
    for track in tracks:
        cursor=0;tick=0;status=0
        def vlq():
            nonlocal cursor
            value=0
            for _ in range(5):
                b=track[cursor];cursor+=1;value=(value<<7)|(b&127)
                if not b&128:return value
            raise ValueError('Invalid EAMS variable-length number')
        tick=vlq()
        while cursor<len(track):
            if track[cursor]&128:status=track[cursor];cursor+=1
            if status==255:break
            op=status&240
            if op not in (128,144,160,176,192,208,224):raise ValueError('Invalid EAMS status')
            count=1 if op in (192,208) else 2
            message=bytes([status])+track[cursor:cursor+count];cursor+=count
            assert all(b<128 for b in message[1:])
            events.append((tick,sequence,message));sequence+=1
            if op==144:
                duration=vlq()
                events.append((tick+duration,sequence,bytes([128|(status&15),message[1],0])));sequence+=1
            tick+=vlq()
    # total-order: event time, then original encounter order including releases.
    events.sort(key=lambda event:(event[0],event[1]))
    out=bytearray(b'\x00\xff\x51\x03\x0f\x42\x40');previous=0
    for tick,_,message in events:out+=encode(tick-previous)+message;previous=tick
    out+=b'\x00\xff\x2f\x00'
    return b'MThd'+struct.pack('>IHHH',6,0,1,240)+b'MTrk'+struct.pack('>I',len(out))+out

def u32(b,p):return struct.unpack_from('>I',b,p)[0]
def extract(image,output,ffmpeg,fluidsynth=None,soundfont=None):
    output.mkdir(parents=True,exist_ok=True)
    disc=image.open('rb');header=disc.read(16);raw=header[:12]==b'\x00'+b'\xff'*10+b'\x00'
    sector=2352 if raw else 2048;skip=16 if raw else 0
    def blocks(start,length):
        count=(length+2047)//2048;disc.seek(start*sector);b=disc.read(count*sector)
        return (b if not raw else b''.join(b[p+skip:p+skip+2048] for p in range(0,len(b),sector)))[:length]
    v=blocks(0,2048);assert v[:7]==b'\x01ZZZZZ\x01';entries=[];visited=set()
    def directory(start,parent):
        assert start not in visited;visited.add(start);rel=0
        while True:
            b=blocks(start+rel,2048);pos=u32(b,16);end=u32(b,12)
            while pos+68<=end:
                flags=u32(b,pos);size=u32(b,pos+16);name=b[pos+32:pos+64].split(b'\0')[0].decode('ascii')
                assert name not in ('','..','.') and '/' not in name and '\\' not in name
                address=u32(b,pos+68);kind=flags&255;path=parent+'/'+name
                entries.append((path,address,size,kind))
                if kind==7:directory(address,path)
                pos+=68+4*(u32(b,pos+64)+1)
                if flags&0xc0000000:break
            rel=u32(b,0)
            if rel==0xffffffff:break
    directory(u32(v,100),'')
    manifest={'source_image_sha256':hashlib.file_digest(image.open('rb'),'sha256').hexdigest(),'samples':[],'streams':[],'excluded':[],'native_files':[]}
    native=output/'native';native.mkdir(exist_ok=True)
    for path,address,size,kind in entries:
        if kind==7:continue
        low=path.lower()
        licensed=(('/streams/bgaudio/' in low and low.endswith('.stream')) or '/streams/inter/' in low or '/streams/intro/' in low)
        if licensed:
            manifest['excluded'].append({'path':path,'reason':'licensed soundtrack or music-video/intro container'});continue
        is_stream=low.endswith('.stream')
        # Sampled voices can be embedded in family/global resource banks,
        # and the original in-race compositions are EAMS event streams.
        is_audio=(low.endswith(('.aiff','.aif','.amco','.rsrc','.rsc','.eams','.scon')) or '/system/audio/' in low)
        if not (is_stream or is_audio):continue
        target=native/path.lstrip('/');target.parent.mkdir(parents=True,exist_ok=True)
        data=blocks(address,size);assert len(data)==size;target.write_bytes(data)
        manifest['native_files'].append({'path':path,'bytes':size,'sha256':hashlib.sha256(data).hexdigest()})
        if is_stream:
            target_wav=output/'cinematics'/pathlib.Path(path).relative_to('/Rash/Streams').with_suffix('.wav')
            target_wav.parent.mkdir(parents=True,exist_ok=True)
            result=subprocess.run([ffmpeg,'-v','error','-y','-i',str(target),'-map','0:a:0','-vn','-c:a','pcm_s16le',str(target_wav)],capture_output=True,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            row={'source':path,'wav':str(target_wav.relative_to(output)),'decoded':result.returncode==0}
            if result.returncode and b'SNDS' not in data:
                row={'source':path,'decoded':False,'silent_video':True,'note':'Video-only stream; engine preview is generated live, not stored as audio.'}
            elif result.returncode:row['diagnostic']=result.stderr[-1000:]
            manifest['streams'].append(row)
    container=(native/'Rash/Rash.AIFF').read_bytes();table=u32(container,16);count=u32(container,table+8)
    aiffs=output/'aiff';aiffs.mkdir(exist_ok=True)
    for i in range(count):
        tag,rid,start,size=struct.unpack_from('>4sIII',container,table+16+32*i);assert tag==b'AIFF'
        b=container[start:start+size];(aiffs/('resource%02d.aiff'%rid)).write_bytes(b)
        chunks={};p=12
        while p+8<=len(b):
            tag,n=struct.unpack_from('>4sI',b,p);chunks[tag]=b[p+8:p+8+n];p+=8+n+(n&1)
        comm=chunks[b'COMM'];channels,frames,bits=struct.unpack_from('>hIh',comm);assert bits==16
        exp=int.from_bytes(comm[8:10],'big');mant=int.from_bytes(comm[10:18],'big');rate=round(math.ldexp(mant,(exp&0x7fff)-16383-63))
        sound=chunks[b'SSND'];offset=u32(sound,0);pcm=sound[8+offset:8+offset+frames*channels*2]
        samples=struct.unpack('>'+str(frames*channels)+'h',pcm);little=struct.pack('<'+str(len(samples))+'h',*samples)
        mark=chunks.get(b'MARK',b'\0\0');markers={};p=2
        for _ in range(struct.unpack_from('>H',mark)[0]):
            ident,position=struct.unpack_from('>hI',mark,p);n=mark[p+6];markers[ident]=position;p+=7+n+((1+n)&1)
        inst=chunks.get(b'INST',b'\0'*20);mode,begin,end=struct.unpack_from('>3h',inst,8)
        name='resource%02d.wav'%rid;wav=output/name
        with wave.open(str(wav),'wb') as w:w.setparams((channels,2,rate,0,'NONE','not compressed'));w.writeframes(little)
        loop=None
        if mode and begin in markers and end in markers:
            lo,hi=markers[begin],markers[end];assert 0<=lo<hi<=frames
            # RIFF smpl loop endpoints are inclusive; 3DO sustain end is exclusive.
            smpl=struct.pack('<9I',0,0,round(1e9/rate),inst[0],0,0,0,1,0)+struct.pack('<6I',0,0,lo,hi-1,0,0)
            blob=wav.read_bytes()+b'smpl'+struct.pack('<I',len(smpl))+smpl
            blob=blob[:4]+struct.pack('<I',len(blob)-8)+blob[8:];wav.write_bytes(blob);loop=[lo,hi]
        with wave.open(str(wav)) as w:assert w.readframes(frames)==little and w.getnframes()==frames
        manifest['samples'].append({'id':rid,'file':name,'rate':rate,'frames':frames,'loop':loop,'pcm_sha256':hashlib.sha256(little).hexdigest()})
    disc.close()
    # Interface bank and SDK reference audio also contain standalone AIFFs.
    extras=output/'extras';extras.mkdir(exist_ok=True);manifest['extra_audio']=[]
    for p in native.rglob('*'):
        if not p.is_file() or p.name=='Rash.AIFF' or p.suffix.lower()=='.stream':continue
        b=p.read_bytes();pos=0;index=0
        while True:
            pos=b.find(b'FORM',pos)
            if pos<0:break
            if pos+12<=len(b) and b[pos+8:pos+12] in (b'AIFF',b'AIFC'):
                end=pos+8+u32(b,pos+4)
                if end<=len(b):
                    index+=1;name=p.stem.replace('.','_')+'_%02d'%index
                    aiff=extras/(name+'.aiff');wav=extras/(name+'.wav');aiff.write_bytes(b[pos:end])
                    result=subprocess.run([ffmpeg,'-v','error','-y','-i',str(aiff),'-c:a','pcm_s16le',str(wav)],capture_output=True,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                    if result.returncode:raise RuntimeError(result.stderr)
                    manifest['extra_audio'].append({'source':str(p.relative_to(native)),'wav':str(wav.relative_to(output))})
            pos+=4
    manifest['sequenced_scores']=[]
    scores=output/'scores';scores.mkdir(exist_ok=True)
    for source in native.rglob('*.eams'):
        midi=scores/(source.stem+'.mid');midi.write_bytes(eams_to_midi(source.read_bytes()))
        row={'source':str(source.relative_to(native)),'midi':str(midi.relative_to(output)),
             'timing':'OMS 240 Hz audio clock; explicit duration note-offs',
             'note':'EAMS is a sequence, not recorded audio. WAV preview uses the selected GM bank, not the original SCON DSP instruments.'}
        if fluidsynth and soundfont:
            wav=midi.with_suffix('.wav')
            result=subprocess.run([fluidsynth,'-ni','-r','44100','-F',str(wav),str(soundfont),str(midi)],capture_output=True,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            if result.returncode:raise RuntimeError(result.stderr)
            row['wav']=str(wav.relative_to(output))
        manifest['sequenced_scores'].append(row)
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2))
    failed=[x for x in manifest['streams'] if not x['decoded'] and not x.get('silent_video')]
    decoded=sum(x['decoded'] for x in manifest['streams'])
    print('Converted',len(manifest['samples']),'sampled effects,',len(manifest['extra_audio']),'interface/SDK samples and',decoded,'movie audio streams.')
    print('Video-only shop streams:',sum(bool(x.get('silent_video')) for x in manifest['streams']))
    print('Excluded',len(manifest['excluded']),'licensed soundtrack/music-video containers.')
    if failed:print('Undecoded stream diagnostics:',json.dumps(failed,indent=2));raise RuntimeError('Stream decoding incomplete')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--ffmpeg',default=shutil.which('ffmpeg'));p.add_argument('--fluidsynth');p.add_argument('--soundfont',type=pathlib.Path);a=p.parse_args()
    if not a.ffmpeg:p.error('--ffmpeg is required for movie audio')
    extract(a.image,a.output,a.ffmpeg,a.fluidsynth,a.soundfont)
