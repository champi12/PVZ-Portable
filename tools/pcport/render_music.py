#!/usr/bin/env python3
"""Pre-renderiza la musica de PvZ (sounds/mainmusic.mo3) para la PSP.

La PSP no tiene memoria para el modulo .mo3 decodificado (~26 MB), asi que cada melodia se graba
aparte, separada en pistas (melodia, tambores, platillos) para que el juego pueda subir y bajar los
tambores como en PC. Formato: archivos .pvzm (IMA ADPCM mono a 22050 Hz) en la carpeta `music/`
que va junto al EBOOT.

Necesita Python 3 y libopenmpt (Linux: paquete libopenmpt0; Windows: libopenmpt.dll junto a este
script, de https://lib.openmpt.org).

Uso: render_music.py mainmusic.mo3 CARPETA_SALIDA
"""
import ctypes, ctypes.util, os, struct, sys

RATE = 22050
BLOCK = 1024
CHANNELS = 30

# orden de inicio -> grupos de canales (copiado de Music::SetupVolumeForTune)
def tune_groups(main_end, drums=None, hihats=()):
    """Devuelve las pistas: canales que siempre suenan juntos con el mismo volumen."""
    roles = {}
    for ch in range(CHANNELS):
        if ch <= main_end:
            r = 'm'
        else:
            d = drums is not None and drums[0] <= ch <= drums[1]
            h = any(a <= ch <= b for a, b in hihats)
            r = ('d' if d else '') + ('h' if h else '')
            if not r:
                continue  # siempre en silencio
        roles.setdefault(r, []).append(ch)
    return list(roles.values())

FULL = [list(range(CHANNELS))]
STREAMS = {
    0x00: tune_groups(23, (24, 26), [(27, 27)]),               # dia
    0x5E: tune_groups(17, (18, 28), [(18, 24), (29, 29)]),     # piscina
    0x7D: tune_groups(15, (16, 22), [(23, 23)]),               # niebla
    0xB8: tune_groups(17, (18, 20), [(21, 21)]),               # tejado
    0x30: FULL,  # noche
    0x4C: FULL,  # noche: tambores (Music::UpdateMusicBurst salta a 76 o 77)
    0x4D: FULL,
    0x7A: FULL,  # elige tus semillas
    0x98: FULL,  # titulo
    0xDD: FULL,  # jardin zen
    0xB1: FULL,  # puzle
    0xA6: FULL,  # minijuegos
    0xD4: FULL,  # cinta transportadora
    0x9E: FULL,  # jefe final
}

def load_lib():
    names = [os.path.join(os.path.dirname(os.path.abspath(__file__)), 'libopenmpt.dll'),
             'libopenmpt.so.0', ctypes.util.find_library('openmpt') or 'libopenmpt']
    for n in names:
        try:
            return ctypes.CDLL(n)
        except OSError:
            pass
    sys.exit('No encuentro libopenmpt')

class Interactive(ctypes.Structure):
    _fields_ = [(n, ctypes.c_void_p) for n in (
        'set_current_speed', 'set_current_tempo', 'set_tempo_factor', 'get_tempo_factor',
        'set_pitch_factor', 'get_pitch_factor', 'set_global_volume', 'get_global_volume',
        'set_channel_volume', 'get_channel_volume', 'set_channel_mute_status', 'get_channel_mute_status',
        'set_instrument_mute_status', 'get_instrument_mute_status', 'play_note', 'stop_note')]

L = load_lib()
L.openmpt_module_ext_create_from_memory.restype = ctypes.c_void_p
L.openmpt_module_ext_create_from_memory.argtypes = [ctypes.c_char_p, ctypes.c_size_t] + [ctypes.c_void_p] * 7
L.openmpt_module_ext_get_module.restype = ctypes.c_void_p
L.openmpt_module_ext_get_module.argtypes = [ctypes.c_void_p]
L.openmpt_module_ext_get_interface.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
L.openmpt_module_ext_destroy.argtypes = [ctypes.c_void_p]
L.openmpt_module_set_repeat_count.argtypes = [ctypes.c_void_p, ctypes.c_int32]
L.openmpt_module_set_position_order_row.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32]
L.openmpt_module_set_position_order_row.restype = ctypes.c_double
L.openmpt_module_read_mono.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_size_t, ctypes.c_void_p]
L.openmpt_module_read_mono.restype = ctypes.c_size_t
L.openmpt_module_get_current_order.argtypes = [ctypes.c_void_p]
L.openmpt_module_get_num_channels.argtypes = [ctypes.c_void_p]
L.openmpt_module_ctl_set_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
MUTE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int32, ctypes.c_int)

def render(data, start, channels, repeat, limit=None):
    """Mezcla en mono desde el orden `start` con solo `channels` sonando. Devuelve (muestras, ordenes)."""
    ext = L.openmpt_module_ext_create_from_memory(data, len(data), None, None, None, None, None, None, None)
    mod = L.openmpt_module_ext_get_module(ext)
    it = Interactive()
    L.openmpt_module_ext_get_interface(ext, b'interactive', ctypes.byref(it), ctypes.sizeof(it))
    mute = MUTE(it.set_channel_mute_status)
    for ch in range(L.openmpt_module_get_num_channels(mod)):
        mute(ext, ch, 0 if ch in channels else 1)
    L.openmpt_module_set_repeat_count(mod, repeat)
    L.openmpt_module_set_position_order_row(mod, start, 0)
    out = bytearray()
    orders = []
    buf = (ctypes.c_int16 * 256)()
    while limit is None or len(out) // 2 < limit:
        o = L.openmpt_module_get_current_order(mod)
        if not orders or orders[-1][1] != o:
            orders.append((len(out) // 2, o))
        n = L.openmpt_module_read_mono(mod, RATE, 256, buf)
        if n == 0:
            break
        out += bytes(buf)[:n * 2]
    last = L.openmpt_module_get_current_order(mod)
    L.openmpt_module_ext_destroy(ext)
    return out, orders, last

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
        73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
        449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
        9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8]

def adpcm_block(samples, pred, idx):
    """Un bloque IMA ADPCM: cabecera (prediccion, indice) y BLOCK muestras de 4 bits."""
    head = struct.pack('<hBB', pred, idx, 0)
    nib = []
    for s in samples:
        step = STEP[idx]
        diff = s - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        delta = step >> 3
        if diff >= step:
            code |= 4; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 2; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 1; delta += step
        pred = pred - delta if code & 8 else pred + delta
        pred = -32768 if pred < -32768 else 32767 if pred > 32767 else pred
        idx += IDX[code & 7]
        idx = 0 if idx < 0 else 88 if idx > 88 else idx
        nib.append(code)
    body = bytes(nib[i] | (nib[i + 1] << 4) for i in range(0, BLOCK, 2))
    return head + body, pred, idx

def main():
    src, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    data = open(src, 'rb').read()
    for start, groups in STREAMS.items():
        allch = sorted(c for g in groups for c in g)
        # una pasada completa (hasta que vuelve a algo ya tocado) para la longitud y los ordenes
        full, orders, _ = render(data, start, set(allch), 0)
        n = len(full) // 2
        # a donde salta al acabar: se sigue tocando un poco con repeticion infinita
        _, _, target = render(data, start, set(allch), -1, n + RATE // 10)
        loop = next((p for p, o in orders if o == target), 0)
        nblocks = (n + BLOCK - 1) // BLOCK
        stems = []
        for g in groups:
            pcm, _, _ = render(data, start, set(g), 0, n) if len(groups) > 1 else (full, None, None)
            pcm = struct.unpack('<%dh' % (len(pcm) // 2), pcm)
            pcm = list(pcm[:n])
            stems.append(pcm + [0] * (nblocks * BLOCK - len(pcm)))
        head = struct.pack('<4sHHIIII', b'PVZM', 1, len(groups), RATE, n, loop, len(orders))
        head += b''.join(struct.pack('<I', sum(1 << c for c in g)) for g in groups)
        head += b''.join(struct.pack('<IHH', p, o, 0) for p, o in orders)
        state = [(0, 0)] * len(groups)
        body = bytearray()
        for b in range(nblocks):
            for i, pcm in enumerate(stems):
                blk, pr, ix = adpcm_block(pcm[b * BLOCK:(b + 1) * BLOCK], state[i][0] if b else pcm[0], state[i][1])
                state[i] = (pr, ix)
                body += blk
        name = os.path.join(outdir, 'tune_%02X.pvzm' % start)
        with open(name, 'wb') as f:
            f.write(head + body)
        print('%s: %.1f s, %d pistas, bucle en %.1f s (orden %d)' % (name, n / RATE, len(groups), loop / RATE, target))

if __name__ == '__main__':
    main()
