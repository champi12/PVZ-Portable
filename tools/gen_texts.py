#!/usr/bin/env python3
"""Genera src/texts.h con las 6 tablas de texto del PvZ J2ME (t-eng, t-fre, t-ger, t-ita, t-ptr, t-spa:
las 281 cadenas con los mismos indices en todos los idiomas).  uso: gen_texts.py carpeta_del_jar src/texts.h"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from parse_texts import read
src, out = sys.argv[1], sys.argv[2]
langs = [('EN', 'eng'), ('FR', 'fre'), ('DE', 'ger'), ('IT', 'ita'), ('PT', 'ptr'), ('ES', 'spa')]
def c(s):
    s = s.replace('\u2026', '...').replace('\u201c', '"').replace('\u201d', '"').replace('\u20ac', 'EUR').replace('\u00a8', '')
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '|').replace('\r', '') + '"'
with open(out, 'w') as f:
    f.write('/* Generado por tools/gen_texts.py desde las tablas t-eng/t-fre/t-ger/t-ita/t-ptr/t-spa del PvZ J2ME */\n')
    f.write('#ifndef TEXTS_H\n#define TEXTS_H\n')
    n = None
    for code, name in langs:
        t = read(os.path.join(src, 't-' + name))
        n = len(t)
        f.write('static const char *const TXT_%s[] = {\n' % code)
        for s in t: f.write('    %s,\n' % c(s))
        f.write('};\n')
    f.write('#define TXT_COUNT %d\n' % n)
    f.write('enum { LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT, LANG_ES, LANG_COUNT };\n')
    f.write('static const char *const *const TXT_LANGS[LANG_COUNT] = { TXT_EN, TXT_FR, TXT_DE, TXT_IT, TXT_PT, TXT_ES };\n')
    f.write('extern const char *const *TXT;   /* tabla del idioma actual (ui.c) */\n#endif\n')
print('ok', n)
