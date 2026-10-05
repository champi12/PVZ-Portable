#!/usr/bin/env python3
"""Genera src/fonts_data.h con las metricas de las fuentes bitmap del J2ME (archivo /f)."""
import json, sys
fonts=json.load(open(sys.argv[1])); out=open(sys.argv[2],'w')
out.write('/* Generado por tools/gen_fonts.py - no editar */\n#ifndef FONTS_DATA_H\n#define FONTS_DATA_H\n')
for i,f in enumerate(fonts):
    out.write('static const unsigned short font%d_chars[] = {%s,0};\n'%(i,','.join(str(ord(c)) for c in f['chars'])))
    out.write('static const unsigned short font%d_glyphs[] = {%s};\n'%(i,','.join('%d,%d,%d'%tuple(g) for g in f['glyphs'])))
out.write('typedef struct { int img, h, upper, count; const unsigned short *chars; const unsigned short *glyphs; } FontDef;\n')
out.write('static const FontDef font_defs[%d] = {\n'%len(fonts))
for i,f in enumerate(fonts):
    out.write('  {%d,%d,%d,%d,font%d_chars,font%d_glyphs},\n'%(f['img'],f['h'],int(f['upper']),len(f['chars']),i,i))
out.write('};\n#define FONT_COUNT %d\n#endif\n'%len(fonts))
