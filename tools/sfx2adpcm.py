#!/usr/bin/env python3
"""Convierte los SFX del PvZ de PC (ogg/au) a IMA-ADPCM 4 bits mono 22050 Hz (.sfx)
Formato .sfx (little endian):
  char[4] 'SFX1'; u32 num_samples; u16 sample_rate; u16 reserved; u8 data[(n+1)/2]
  Nibble bajo = muestra par. Predictor e indice iniciales = 0.
4:1 frente a PCM16 -> ~11 KB por segundo de audio en RAM."""
import subprocess, sys, os, struct, numpy as np
STEP=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
IDX=[-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8]
def encode(pcm):
    pred=0; idx=0; out=bytearray((len(pcm)+1)//2)
    for i,s in enumerate(pcm):
        step=STEP[idx]; diff=int(s)-pred; code=0
        if diff<0: code=8; diff=-diff
        dq=step>>3
        if diff>=step: code|=4; diff-=step; dq+=step
        step>>=1
        if diff>=step: code|=2; diff-=step; dq+=step
        step>>=1
        if diff>=step: code|=1; dq+=step
        pred = pred-dq if code&8 else pred+dq
        pred=max(-32768,min(32767,pred))
        idx=max(0,min(88,idx+IDX[code]))
        if i&1: out[i>>1]|=code<<4
        else: out[i>>1]=code
    return bytes(out)
def convert(src,dst,rate=22050):
    raw=subprocess.run(['ffmpeg','-v','error','-i',src,'-ac','1','-ar',str(rate),'-f','s16le','-'],capture_output=True,check=True).stdout
    pcm=np.frombuffer(raw,dtype='<i2')
    # recorta silencio final
    nz=np.nonzero(np.abs(pcm)>48)[0]
    if len(nz): pcm=pcm[:nz[-1]+1]
    with open(dst,'wb') as f:
        f.write(b'SFX1'+struct.pack('<IHH',len(pcm),rate,0)); f.write(encode(pcm))
    return len(pcm)
if __name__=='__main__':
    src,dst=sys.argv[1],sys.argv[2]; os.makedirs(dst,exist_ok=True); tot=0
    for fn in sorted(os.listdir(src)):
        b,e=os.path.splitext(fn)
        if e.lower() not in ('.ogg','.au') or b=='ZombiesOnYourLawn': continue
        n=convert(os.path.join(src,fn),os.path.join(dst,b.lower()+'.sfx')); tot+=n
    print('muestras',tot,'segundos',tot/22050,'bytes ~',tot//2)
