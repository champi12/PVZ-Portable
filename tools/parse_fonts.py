import struct, json
d=open('jar/f','rb').read(); p=0
def rdutf():
    global p; n=struct.unpack('>H',d[p:p+2])[0]; s=d[p+2:p+2+n]; p+=2+n
    # modified UTF-8 de Java
    return s.replace(b'\xc0\x80',b'\x00').decode('utf-8','surrogatepass')
n=d[0]; p=1; fonts=[]
for i in range(n):
    img,h,up=struct.unpack('>HBB',d[p:p+4]); p+=4
    glyphs=rdutf(); ranges=rdutf()
    g=[(ord(glyphs[k]),ord(glyphs[k+1]),ord(glyphs[k+2])) for k in range(0,len(glyphs),3)]
    rg=[(ranges[k],ranges[k+1]) for k in range(0,len(ranges),2)]
    chars=[]
    for a,b in rg: chars+= [chr(c) for c in range(ord(a),ord(b)+1)]
    fonts.append(dict(img=img,h=h,upper=bool(up),chars=''.join(chars),glyphs=g))
    print(i,'img',img,'h',h,'upper',up,'n',len(g),len(chars),repr(''.join(chars)))
json.dump(fonts,open('imgs/fonts.json','w'),ensure_ascii=False)
