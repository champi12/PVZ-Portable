"""Minimal Java class file parser (enough for CLDC 1.0 / Java 8 class files)."""

import struct

ACC_PUBLIC = 0x0001
ACC_PRIVATE = 0x0002
ACC_PROTECTED = 0x0004
ACC_STATIC = 0x0008
ACC_FINAL = 0x0010
ACC_SYNCHRONIZED = 0x0020
ACC_NATIVE = 0x0100
ACC_INTERFACE = 0x0200
ACC_ABSTRACT = 0x0400


def decode_mutf8(b):
    """Decode modified UTF-8 into a list of UTF-16 code units."""
    out = []
    i = 0
    n = len(b)
    while i < n:
        c = b[i]
        if c < 0x80:
            out.append(c)
            i += 1
        elif (c & 0xE0) == 0xC0:
            out.append(((c & 0x1F) << 6) | (b[i + 1] & 0x3F))
            i += 2
        else:
            out.append(((c & 0x0F) << 12) | ((b[i + 1] & 0x3F) << 6) | (b[i + 2] & 0x3F))
            i += 3
    return out


def units_to_str(units):
    return ''.join(chr(u) for u in units)


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def u1(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def u2(self):
        v = struct.unpack_from('>H', self.d, self.p)[0]
        self.p += 2
        return v

    def u4(self):
        v = struct.unpack_from('>I', self.d, self.p)[0]
        self.p += 4
        return v

    def bytes(self, n):
        v = self.d[self.p:self.p + n]
        self.p += n
        return v


class Field:
    def __init__(self, access, name, desc, const_value):
        self.access = access
        self.name = name
        self.desc = desc
        self.const_value = const_value  # ('I', 5) / ('J', 5) / ('S', units) / None

    @property
    def is_static(self):
        return bool(self.access & ACC_STATIC)


class Method:
    def __init__(self, access, name, desc):
        self.access = access
        self.name = name
        self.desc = desc
        self.code = None
        self.max_stack = 0
        self.max_locals = 0
        self.exc_table = []  # (start, end, handler, catch_type_name or None)
        self.cls = None

    @property
    def is_static(self):
        return bool(self.access & ACC_STATIC)

    @property
    def is_abstract(self):
        return bool(self.access & ACC_ABSTRACT)

    @property
    def is_native(self):
        return bool(self.access & ACC_NATIVE)

    @property
    def is_private(self):
        return bool(self.access & ACC_PRIVATE)


class ClassFile:
    def __init__(self, data):
        r = Reader(data)
        if r.u4() != 0xCAFEBABE:
            raise ValueError('bad magic')
        self.minor = r.u2()
        self.major = r.u2()
        n = r.u2()
        cp = [None] * n
        i = 1
        while i < n:
            tag = r.u1()
            if tag == 1:
                ln = r.u2()
                cp[i] = ('Utf8', r.bytes(ln))
            elif tag == 3:
                cp[i] = ('Integer', struct.unpack('>i', r.bytes(4))[0])
            elif tag == 4:
                cp[i] = ('Float', struct.unpack('>f', r.bytes(4))[0], r.d[r.p - 4:r.p])
            elif tag == 5:
                cp[i] = ('Long', struct.unpack('>q', r.bytes(8))[0])
                i += 1
            elif tag == 6:
                cp[i] = ('Double', struct.unpack('>d', r.bytes(8))[0], r.d[r.p - 8:r.p])
                i += 1
            elif tag == 7:
                cp[i] = ('Class', r.u2())
            elif tag == 8:
                cp[i] = ('String', r.u2())
            elif tag in (9, 10, 11):
                kind = {9: 'Fieldref', 10: 'Methodref', 11: 'InterfaceMethodref'}[tag]
                cp[i] = (kind, r.u2(), r.u2())
            elif tag == 12:
                cp[i] = ('NameAndType', r.u2(), r.u2())
            elif tag == 15:
                cp[i] = ('MethodHandle', r.u1(), r.u2())
            elif tag == 16:
                cp[i] = ('MethodType', r.u2())
            elif tag == 18:
                cp[i] = ('InvokeDynamic', r.u2(), r.u2())
            else:
                raise ValueError('bad cp tag %d' % tag)
            i += 1
        self.cp = cp
        self.access = r.u2()
        self.name = self.class_name(r.u2())
        sup = r.u2()
        self.super = self.class_name(sup) if sup else None
        self.interfaces = [self.class_name(r.u2()) for _ in range(r.u2())]
        self.fields = []
        for _ in range(r.u2()):
            acc = r.u2()
            name = self.utf8(r.u2())
            desc = self.utf8(r.u2())
            cv = None
            for _ in range(r.u2()):
                an = self.utf8(r.u2())
                ln = r.u4()
                body = r.bytes(ln)
                if an == 'ConstantValue':
                    idx = struct.unpack('>H', body)[0]
                    e = cp[idx]
                    if e[0] == 'String':
                        cv = ('S', decode_mutf8(cp[e[1]][1]))
                    elif e[0] in ('Integer', 'Long'):
                        cv = (e[0][0], e[1])
                    elif e[0] == 'Float':
                        cv = ('F', e[1])
                    elif e[0] == 'Double':
                        cv = ('D', e[1])
            self.fields.append(Field(acc, name, desc, cv))
        self.methods = []
        for _ in range(r.u2()):
            acc = r.u2()
            m = Method(acc, self.utf8(r.u2()), self.utf8(r.u2()))
            m.cls = self
            for _ in range(r.u2()):
                an = self.utf8(r.u2())
                ln = r.u4()
                body = r.bytes(ln)
                if an == 'Code':
                    cr = Reader(body)
                    m.max_stack = cr.u2()
                    m.max_locals = cr.u2()
                    cl = cr.u4()
                    m.code = cr.bytes(cl)
                    for _ in range(cr.u2()):
                        s, e, h, t = cr.u2(), cr.u2(), cr.u2(), cr.u2()
                        m.exc_table.append((s, e, h, self.class_name(t) if t else None))
            self.methods.append(m)

    def utf8(self, i):
        return units_to_str(decode_mutf8(self.cp[i][1]))

    def class_name(self, i):
        return self.utf8(self.cp[i][1])

    def name_and_type(self, i):
        e = self.cp[i]
        return self.utf8(e[1]), self.utf8(e[2])

    def member_ref(self, i):
        e = self.cp[i]
        cls = self.class_name(e[1])
        name, desc = self.name_and_type(e[2])
        return cls, name, desc

    @property
    def is_interface(self):
        return bool(self.access & ACC_INTERFACE)


def parse_method_desc(desc):
    """'(I[JLjava/lang/String;)V' -> (['I', '[J', 'Ljava/lang/String;'], 'V')"""
    assert desc[0] == '('
    i = 1
    args = []
    while desc[i] != ')':
        j = i
        while desc[j] == '[':
            j += 1
        if desc[j] == 'L':
            j = desc.index(';', j)
        args.append(desc[i:j + 1])
        i = j + 1
    return args, desc[i + 1:]
