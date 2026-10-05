#!/usr/bin/env python3
"""
jvm2cpp - ahead-of-time translator from JVM bytecode (CLDC / MIDP class files) to C++.

Every Java method becomes a plain C++ function. The JVM operand stack and local variables
become typed C++ locals (one variable per stack depth / local slot and per value kind), so the
generated code is ordinary C++ that GCC optimises well on the PSP's MIPS CPU.

Usage:
    jvm2cpp.py --out gen/ --main Game [--override overrides.txt] dir_or_jar [dir_or_jar ...]

Inputs are directories or .jar files containing .class files. The game classes and the
runtime library classes (runtime/java, compiled with javac) are translated together.
"""

import argparse
import io
import os
import re
import struct
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from classfile import (ClassFile, parse_method_desc, decode_mutf8, ACC_STATIC, ACC_FINAL,
                       ACC_PRIVATE, ACC_INTERFACE, ACC_ABSTRACT, ACC_NATIVE)

# --------------------------------------------------------------------------------------------
# Naming helpers
# --------------------------------------------------------------------------------------------


def mangle(s):
    out = []
    for ch in s:
        if ch.isalnum():
            out.append(ch)
        elif ch in '/.':
            out.append('_')
        elif ch == ';':
            out.append('_')
        elif ch == '[':
            out.append('A')
        elif ch == '$':
            out.append('S')
        elif ch == '<':
            out.append('_')
        elif ch == '>':
            out.append('_')
        elif ch in '()':
            out.append('')
        elif ch == '_':
            out.append('_1')
        else:
            out.append('_%04x' % ord(ch))
    return ''.join(out)


def cls_struct(name):
    return 'J_' + mangle(name)


def cls_meta(name):
    if name.startswith('['):
        return 'AC_' + mangle(name)
    return 'C_' + mangle(name)


def desc_mangle(desc):
    args, ret = parse_method_desc(desc)
    return mangle(''.join(args)) + '_' + mangle(ret)


def method_fn(cls, name, desc):
    return 'M_%s__%s__%s' % (mangle(cls), mangle(name), desc_mangle(desc))


def field_member(name, desc):
    return 'f_%s_%s' % (mangle(name), mangle(desc))


def static_var(cls, name, desc):
    return 'S_%s__%s_%s' % (mangle(cls), mangle(name), mangle(desc))


def kind_of(desc):
    """Stack kind of a field / arg descriptor: i j f d a."""
    c = desc[0]
    if c in 'IZBCS':
        return 'i'
    if c == 'J':
        return 'j'
    if c == 'F':
        return 'f'
    if c == 'D':
        return 'd'
    return 'a'


CTYPE_OF_KIND = {'i': 'int32_t', 'j': 'int64_t', 'f': 'float', 'd': 'double', 'a': 'JObject*'}
ZERO_OF_KIND = {'i': '0', 'j': '0', 'f': '0.0f', 'd': '0.0', 'a': 'nullptr'}


def field_ctype(desc):
    return {'I': 'int32_t', 'Z': 'int8_t', 'B': 'int8_t', 'C': 'uint16_t', 'S': 'int16_t',
            'J': 'int64_t', 'F': 'float', 'D': 'double'}.get(desc[0], 'JObject*')


def ret_ctype(desc):
    if desc == 'V':
        return 'void'
    return CTYPE_OF_KIND[kind_of(desc)]


# --------------------------------------------------------------------------------------------
# Opcode table: name, operand-length (-1 = special)
# --------------------------------------------------------------------------------------------

OPS = {}
_ops = """
0 nop 0;1 aconst_null 0;2 iconst_m1 0;3 iconst_0 0;4 iconst_1 0;5 iconst_2 0;6 iconst_3 0;
7 iconst_4 0;8 iconst_5 0;9 lconst_0 0;10 lconst_1 0;11 fconst_0 0;12 fconst_1 0;13 fconst_2 0;
14 dconst_0 0;15 dconst_1 0;16 bipush 1;17 sipush 2;18 ldc 1;19 ldc_w 2;20 ldc2_w 2;
21 iload 1;22 lload 1;23 fload 1;24 dload 1;25 aload 1;
26 iload_0 0;27 iload_1 0;28 iload_2 0;29 iload_3 0;30 lload_0 0;31 lload_1 0;32 lload_2 0;
33 lload_3 0;34 fload_0 0;35 fload_1 0;36 fload_2 0;37 fload_3 0;38 dload_0 0;39 dload_1 0;
40 dload_2 0;41 dload_3 0;42 aload_0 0;43 aload_1 0;44 aload_2 0;45 aload_3 0;
46 iaload 0;47 laload 0;48 faload 0;49 daload 0;50 aaload 0;51 baload 0;52 caload 0;53 saload 0;
54 istore 1;55 lstore 1;56 fstore 1;57 dstore 1;58 astore 1;
59 istore_0 0;60 istore_1 0;61 istore_2 0;62 istore_3 0;63 lstore_0 0;64 lstore_1 0;
65 lstore_2 0;66 lstore_3 0;67 fstore_0 0;68 fstore_1 0;69 fstore_2 0;70 fstore_3 0;
71 dstore_0 0;72 dstore_1 0;73 dstore_2 0;74 dstore_3 0;75 astore_0 0;76 astore_1 0;
77 astore_2 0;78 astore_3 0;
79 iastore 0;80 lastore 0;81 fastore 0;82 dastore 0;83 aastore 0;84 bastore 0;85 castore 0;
86 sastore 0;87 pop 0;88 pop2 0;89 dup 0;90 dup_x1 0;91 dup_x2 0;92 dup2 0;93 dup2_x1 0;
94 dup2_x2 0;95 swap 0;96 iadd 0;97 ladd 0;98 fadd 0;99 dadd 0;100 isub 0;101 lsub 0;
102 fsub 0;103 dsub 0;104 imul 0;105 lmul 0;106 fmul 0;107 dmul 0;108 idiv 0;109 ldiv 0;
110 fdiv 0;111 ddiv 0;112 irem 0;113 lrem 0;114 frem 0;115 drem 0;116 ineg 0;117 lneg 0;
118 fneg 0;119 dneg 0;120 ishl 0;121 lshl 0;122 ishr 0;123 lshr 0;124 iushr 0;125 lushr 0;
126 iand 0;127 land 0;128 ior 0;129 lor 0;130 ixor 0;131 lxor 0;132 iinc 2;
133 i2l 0;134 i2f 0;135 i2d 0;136 l2i 0;137 l2f 0;138 l2d 0;139 f2i 0;140 f2l 0;141 f2d 0;
142 d2i 0;143 d2l 0;144 d2f 0;145 i2b 0;146 i2c 0;147 i2s 0;148 lcmp 0;149 fcmpl 0;
150 fcmpg 0;151 dcmpl 0;152 dcmpg 0;153 ifeq 2;154 ifne 2;155 iflt 2;156 ifge 2;157 ifgt 2;
158 ifle 2;159 if_icmpeq 2;160 if_icmpne 2;161 if_icmplt 2;162 if_icmpge 2;163 if_icmpgt 2;
164 if_icmple 2;165 if_acmpeq 2;166 if_acmpne 2;167 goto 2;168 jsr 2;169 ret 1;
170 tableswitch -1;171 lookupswitch -1;172 ireturn 0;173 lreturn 0;174 freturn 0;
175 dreturn 0;176 areturn 0;177 return 0;178 getstatic 2;179 putstatic 2;180 getfield 2;
181 putfield 2;182 invokevirtual 2;183 invokespecial 2;184 invokestatic 2;
185 invokeinterface 4;186 invokedynamic 4;187 new 2;188 newarray 1;189 anewarray 2;
190 arraylength 0;191 athrow 0;192 checkcast 2;193 instanceof 2;194 monitorenter 0;
195 monitorexit 0;196 wide -1;197 multianewarray 3;198 ifnull 2;199 ifnonnull 2;
200 goto_w 4;201 jsr_w 4
"""
for _e in _ops.replace('\n', '').split(';'):
    _e = _e.strip()
    if _e:
        _c, _n, _l = _e.split()
        OPS[int(_c)] = (_n, int(_l))

NAME2OP = {v[0]: k for k, v in OPS.items()}

THROWING = set("""iaload laload faload daload aaload baload caload saload iastore lastore fastore
dastore aastore bastore castore sastore idiv ldiv irem lrem getstatic putstatic getfield
putfield invokevirtual invokespecial invokestatic invokeinterface new newarray anewarray
arraylength athrow checkcast multianewarray monitorenter""".split())

NEWARRAY_TYPES = {4: '[Z', 5: '[C', 6: '[F', 7: '[D', 8: '[B', 9: '[S', 10: '[I', 11: '[J'}

ARRAY_ELEM = {  # opcode prefix -> (C element type, stack kind)
    'i': ('int32_t', 'i'), 'l': ('int64_t', 'j'), 'f': ('float', 'f'), 'd': ('double', 'd'),
    'a': ('JObject*', 'a'), 'b': ('int8_t', 'i'), 'c': ('uint16_t', 'i'), 's': ('int16_t', 'i'),
}


class Insn:
    __slots__ = ('pc', 'op', 'name', 'args', 'length')

    def __init__(self, pc, op, name, args, length):
        self.pc, self.op, self.name, self.args, self.length = pc, op, name, args, length


def decode(code):
    insns = []
    pc = 0
    n = len(code)
    while pc < n:
        op = code[pc]
        name, ln = OPS[op]
        args = ()
        if name == 'wide':
            op2 = code[pc + 1]
            name2 = OPS[op2][0]
            idx = struct.unpack_from('>H', code, pc + 2)[0]
            if name2 == 'iinc':
                c = struct.unpack_from('>h', code, pc + 4)[0]
                insns.append(Insn(pc, op2, 'iinc', (idx, c), 6))
                pc += 6
            else:
                insns.append(Insn(pc, op2, name2, (idx,), 4))
                pc += 4
            continue
        if name == 'tableswitch':
            p = (pc + 4) & ~3
            default, low, high = struct.unpack_from('>iii', code, p)
            offs = struct.unpack_from('>%di' % (high - low + 1), code, p + 12)
            ln_total = p + 12 + 4 * (high - low + 1) - pc
            insns.append(Insn(pc, op, name, (pc + default, low,
                                             [pc + o for o in offs]), ln_total))
            pc += ln_total
            continue
        if name == 'lookupswitch':
            p = (pc + 4) & ~3
            default, npairs = struct.unpack_from('>ii', code, p)
            pairs = []
            for k in range(npairs):
                key, off = struct.unpack_from('>ii', code, p + 8 + 8 * k)
                pairs.append((key, pc + off))
            ln_total = p + 8 + 8 * npairs - pc
            insns.append(Insn(pc, op, name, (pc + default, pairs), ln_total))
            pc += ln_total
            continue
        b = code[pc + 1:pc + 1 + ln]
        if name in ('bipush',):
            args = (struct.unpack('>b', b)[0],)
        elif name == 'sipush':
            args = (struct.unpack('>h', b)[0],)
        elif name == 'ldc' or ln == 1:
            args = (b[0],)
        elif name == 'iinc':
            args = (b[0], struct.unpack('>b', b[1:2])[0])
        elif name in ('goto_w', 'jsr_w'):
            args = (pc + struct.unpack('>i', b)[0],)
        elif name.startswith('if') or name in ('goto', 'jsr'):
            args = (pc + struct.unpack('>h', b)[0],)
        elif name == 'invokeinterface':
            args = (struct.unpack('>H', b[:2])[0], b[2])
        elif name == 'multianewarray':
            args = (struct.unpack('>H', b[:2])[0], b[2])
        elif ln == 2:
            args = (struct.unpack('>H', b)[0],)
        insns.append(Insn(pc, op, name, args, 1 + ln))
        pc += 1 + ln
    return insns


# --------------------------------------------------------------------------------------------
# Class hierarchy model
# --------------------------------------------------------------------------------------------


class World:
    def __init__(self):
        self.classes = {}
        self.strings = {}      # tuple(units) -> index
        self.string_list = []
        self.array_classes = set()
        self.ikeys = {}        # (name, desc) -> key

    def add(self, cf):
        self.classes[cf.name] = cf

    def get(self, name):
        c = self.classes.get(name)
        if c is None:
            raise KeyError('missing class ' + name)
        return c

    def strlit(self, units):
        t = tuple(units)
        if t not in self.strings:
            self.strings[t] = len(self.string_list)
            self.string_list.append(t)
        return self.strings[t]

    def array_class(self, desc):
        # register desc and all component array descs
        d = desc
        while d.startswith('['):
            self.array_classes.add(d)
            d = d[1:]
        return cls_meta(desc)

    def ikey(self, name, desc):
        k = (name, desc)
        if k not in self.ikeys:
            self.ikeys[k] = len(self.ikeys) + 1
        return self.ikeys[k]

    def ancestors(self, name):
        """name, super, super-super ..."""
        out = []
        while name:
            out.append(name)
            name = self.get(name).super
        return out

    def all_interfaces(self, name):
        seen = []

        def visit_iface(i):
            if i in seen:
                return
            seen.append(i)
            for s in self.get(i).interfaces:
                visit_iface(s)

        for a in self.ancestors(name):
            for i in self.get(a).interfaces:
                visit_iface(i)
        return seen

    def is_subclass(self, sub, sup):
        return sup in self.ancestors(sub)

    def find_method(self, cls, name, desc):
        """Resolve a method like the JVM: superclasses, then superinterfaces."""
        for a in self.ancestors(cls):
            for m in self.get(a).methods:
                if m.name == name and m.desc == desc:
                    return m
        for i in self.all_interfaces(cls):
            for m in self.get(i).methods:
                if m.name == name and m.desc == desc:
                    return m
        return None

    def all_interfaces_of_iface(self, name):
        out = []

        def visit(i):
            for s in self.get(i).interfaces:
                if s not in out:
                    out.append(s)
                    visit(s)

        visit(name)
        return out

    def find_field(self, cls, name, desc):
        """Returns (declaring class, field)."""
        c = cls
        while c:
            cf = self.get(c)
            for f in cf.fields:
                if f.name == name and f.desc == desc:
                    return c, f
            for i in self._ifaces_closure(cf.interfaces):
                for f in self.get(i).fields:
                    if f.name == name and f.desc == desc:
                        return i, f
            c = cf.super
        raise KeyError('field %s.%s:%s' % (cls, name, desc))

    def _ifaces_closure(self, ifs):
        out = []

        def visit(i):
            if i in out:
                return
            out.append(i)
            for s in self.get(i).interfaces:
                visit(s)

        for i in ifs:
            visit(i)
        return out

    # ---- vtables ----

    def compute_layouts(self):
        self.vtables = {}
        self.vslot = {}  # cls -> {(name,desc): slot}
        order = self.topo_order()
        for name in order:
            cf = self.get(name)
            if cf.is_interface:
                continue
            if cf.super:
                vt = list(self.vtables[cf.super])
                slots = dict(self.vslot[cf.super])
            else:
                vt, slots = [], {}
            for m in cf.methods:
                if m.is_static or m.is_private or m.name in ('<init>', '<clinit>'):
                    continue
                key = (m.name, m.desc)
                if key in slots:
                    vt[slots[key]] = m
                else:
                    slots[key] = len(vt)
                    vt.append(m)
            # Miranda methods: interface methods not implemented by any class method get a slot
            for i in self.all_interfaces(name):
                for m in self.get(i).methods:
                    if m.is_static or m.name == '<clinit>':
                        continue
                    key = (m.name, m.desc)
                    if key not in slots:
                        slots[key] = len(vt)
                        vt.append(m)  # abstract
            self.vtables[name] = vt
            self.vslot[name] = slots

    def topo_order(self):
        order = []
        seen = set()

        def visit(n):
            if n in seen:
                return
            seen.add(n)
            cf = self.get(n)
            if cf.super:
                visit(cf.super)
            for i in cf.interfaces:
                visit(i)
            order.append(n)

        for n in sorted(self.classes):
            visit(n)
        return order


# --------------------------------------------------------------------------------------------
# Method translation
# --------------------------------------------------------------------------------------------


class TranslateError(Exception):
    pass


def fn_signature(world, m, fname=None, with_names=False):
    args, ret = parse_method_desc(m.desc)
    params = []
    if not m.is_static:
        params.append('JObject* p_this' if with_names else 'JObject*')
    for i, a in enumerate(args):
        t = CTYPE_OF_KIND[kind_of(a)]
        params.append('%s p%d' % (t, i) if with_names else t)
    return '%s %s(%s)' % (ret_ctype(ret), fname or method_fn(m.cls.name, m.name, m.desc),
                          ', '.join(params))


def fn_ptr_type(m_desc, is_static):
    args, ret = parse_method_desc(m_desc)
    params = [] if is_static else ['JObject*']
    params += [CTYPE_OF_KIND[kind_of(a)] for a in args]
    return '%s(*)(%s)' % (ret_ctype(ret), ', '.join(params))


class MethodTranslator:
    def __init__(self, world, cf, m, overridden=False):
        self.world = world
        self.cf = cf
        self.m = m
        self.insns = decode(m.code)
        self.by_pc = {ins.pc: ins for ins in self.insns}
        self.used_vars = set()
        self.overridden = overridden

    def cp(self, i):
        return self.cf.cp[i]

    # ---- data flow: stack kinds at each pc ----
    def flow(self):
        states = {}
        work = [(0, ())]
        for (s, e, h, t) in self.m.exc_table:
            work.append((h, ('a',)))
        succ_cache = {}
        while work:
            pc, st = work.pop()
            if pc in states:
                if states[pc] != st:
                    if len(states[pc]) != len(st):
                        raise TranslateError('stack depth mismatch at %d in %s.%s' % (pc, self.cf.name, self.m.name))
                continue
            states[pc] = st
            ins = self.by_pc[pc]
            new_st = self.stack_effect(ins, list(st))
            for t in self.successors(ins):
                work.append((t, tuple(new_st)))
        self.states = states

    def successors(self, ins):
        n = ins.name
        nxt = ins.pc + ins.length
        if n in ('goto', 'goto_w'):
            return [ins.args[0]]
        if n in ('ireturn', 'lreturn', 'freturn', 'dreturn', 'areturn', 'return', 'athrow'):
            return []
        if n == 'tableswitch':
            return [ins.args[0]] + list(ins.args[2])
        if n == 'lookupswitch':
            return [ins.args[0]] + [p[1] for p in ins.args[1]]
        if n in ('jsr', 'jsr_w', 'ret'):
            raise TranslateError('jsr/ret not supported')
        if n.startswith('if'):
            return [ins.args[0], nxt]
        return [nxt]

    def stack_effect(self, ins, st):
        """Returns the stack after executing ins (list of kinds)."""
        n = ins.name
        T = {'i': 'i', 'l': 'j', 'f': 'f', 'd': 'd', 'a': 'a'}
        if n == 'nop':
            return st
        if n == 'aconst_null':
            return st + ['a']
        if n.startswith('iconst') or n in ('bipush', 'sipush'):
            return st + ['i']
        if n.startswith('lconst'):
            return st + ['j']
        if n.startswith('fconst'):
            return st + ['f']
        if n.startswith('dconst'):
            return st + ['d']
        if n in ('ldc', 'ldc_w', 'ldc2_w'):
            e = self.cp(ins.args[0])
            return st + [{'Integer': 'i', 'Float': 'f', 'String': 'a', 'Long': 'j', 'Double': 'd', 'Class': 'a'}[e[0]]]
        if re.match(r'^[ilfda]load(_\d)?$', n):
            return st + [T[n[0]]]
        if re.match(r'^[ilfda]store(_\d)?$', n):
            return st[:-1]
        if re.match(r'^[ilfdabcs]aload$', n):
            return st[:-2] + [ARRAY_ELEM[n[0]][1]]
        if re.match(r'^[ilfdabcs]astore$', n):
            return st[:-3]
        if n == 'pop':
            return st[:-1]
        if n == 'pop2':
            return st[:-1] if st[-1] in 'jd' else st[:-2]
        if n in ('dup', 'dup_x1', 'dup_x2', 'dup2', 'dup2_x1', 'dup2_x2', 'swap'):
            k, m = self.dup_shape(n, st)
            if n == 'swap':
                return st[:-2] + [st[-1], st[-2]]
            top = st[len(st) - k:]
            below = st[len(st) - k - m:len(st) - k]
            return st[:len(st) - k - m] + top + below + top
        if re.match(r'^[ilfd](add|sub|mul|div|rem|and|or|xor)$', n):
            return st[:-1]
        if re.match(r'^[ilfd]neg$', n):
            return st
        if re.match(r'^[il](shl|shr|ushr)$', n):
            return st[:-1]
        if n == 'iinc':
            return st
        m_ = re.match(r'^([ilfd])2([ilfdbcs])$', n)
        if m_:
            return st[:-1] + [T.get(m_.group(2), 'i')]
        if n in ('lcmp', 'fcmpl', 'fcmpg', 'dcmpl', 'dcmpg'):
            return st[:-2] + ['i']
        if n in ('ifeq', 'ifne', 'iflt', 'ifge', 'ifgt', 'ifle', 'ifnull', 'ifnonnull'):
            return st[:-1]
        if n.startswith('if_'):
            return st[:-2]
        if n in ('goto', 'goto_w'):
            return st
        if n in ('tableswitch', 'lookupswitch'):
            return st[:-1]
        if n.endswith('return'):
            return []
        if n == 'getstatic':
            _, _, desc = self.cf.member_ref(ins.args[0])
            return st + [kind_of(desc)]
        if n == 'putstatic':
            return st[:-1]
        if n == 'getfield':
            _, _, desc = self.cf.member_ref(ins.args[0])
            return st[:-1] + [kind_of(desc)]
        if n == 'putfield':
            return st[:-2]
        if n.startswith('invoke'):
            _, name, desc = self.cf.member_ref(ins.args[0])
            args, ret = parse_method_desc(desc)
            k = len(args) + (0 if n == 'invokestatic' else 1)
            st = st[:len(st) - k]
            if ret != 'V':
                st = st + [kind_of(ret)]
            return st
        if n == 'new':
            return st + ['a']
        if n in ('newarray', 'anewarray'):
            return st[:-1] + ['a']
        if n == 'arraylength':
            return st[:-1] + ['i']
        if n == 'athrow':
            return []
        if n == 'checkcast':
            return st
        if n == 'instanceof':
            return st[:-1] + ['i']
        if n in ('monitorenter', 'monitorexit'):
            return st[:-1]
        if n == 'multianewarray':
            return st[:-ins.args[1]] + ['a']
        raise TranslateError('unhandled opcode ' + n)

    @staticmethod
    def dup_shape(n, st):
        """Returns (k, m): duplicate top k entries and insert them below m entries."""
        cat2 = lambda k: k in 'jd'
        if n == 'dup':
            return 1, 0
        if n == 'dup_x1':
            return 1, 1
        if n == 'dup_x2':
            return (1, 1) if cat2(st[-2]) else (1, 2)
        if n == 'dup2':
            return (1, 0) if cat2(st[-1]) else (2, 0)
        if n == 'dup2_x1':
            return (1, 1) if cat2(st[-1]) else (2, 1)
        if n == 'dup2_x2':
            if cat2(st[-1]):
                return (1, 1) if cat2(st[-2]) else (1, 2)
            return (2, 1) if cat2(st[-3]) else (2, 2)
        if n == 'swap':
            return 1, 1
        raise ValueError(n)

    # ---- code generation ----
    def sv(self, depth, kind):
        v = 's%d%s' % (depth, kind)
        self.used_vars.add(v)
        return v

    def lv(self, idx, kind):
        v = 'l%d%s' % (idx, kind)
        self.used_vars.add(v)
        return v

    def clinit_guard(self, target):
        """Emit a class-initialisation check when accessing statics of another class."""
        if target == self.cf.name:
            return ''
        if not self.world.get(target).is_interface and self.world.is_subclass(self.cf.name, target):
            return ''
        if not self.world.has_clinit(target):
            return ''
        return 'JCLINIT(%s); ' % mangle(target)

    def translate(self):
        self.flow()
        m = self.m
        world = self.world
        lines = []
        targets = set()
        for ins in self.insns:
            if ins.pc not in self.states:
                continue
            n = ins.name
            if n.startswith('if') or n in ('goto', 'goto_w'):
                targets.add(ins.args[0])
            elif n == 'tableswitch':
                targets.add(ins.args[0])
                targets.update(ins.args[2])
            elif n == 'lookupswitch':
                targets.add(ins.args[0])
                targets.update(p[1] for p in ins.args[1])
        handlers = sorted(set(h for (s, e, h, t) in m.exc_table))
        targets.update(handlers)
        has_exc = bool(m.exc_table)
        protected = set()
        for (s, e, h, t) in m.exc_table:
            for ins in self.insns:
                if s <= ins.pc < e:
                    protected.add(ins.pc)

        skip_until = -1
        for ins in self.insns:
            if ins.pc < skip_until:
                continue
            if ins.pc not in self.states:
                continue  # dead code
            st = list(self.states[ins.pc])
            if ins.pc in targets:
                lines.append('L%d:;' % ins.pc)
            if has_exc and ins.pc in protected and ins.name in THROWING:
                lines.append('  jpc = %d;' % ins.pc)
            if ins.name == 'newarray':
                res = self.try_array_init(ins, st, targets)
                if res:
                    code, end_pc = res
                    lines.extend(code)
                    skip_until = end_pc
                    continue
            code = self.gen(ins, st)
            if code:
                lines.append('  ' + code)

        # assemble
        if has_exc:
            self.used_vars.add('s0a')
        args, ret = parse_method_desc(m.desc)
        out = []
        out.append(fn_signature(world, m, self.fname, with_names=True) + ' {')
        decls = []
        for v in sorted(self.used_vars):
            k = v[-1]
            decls.append('%s %s = %s;' % (CTYPE_OF_KIND[k], v, ZERO_OF_KIND[k]))
        if decls:
            # group to reduce line count
            for i in range(0, len(decls), 6):
                out.append('  ' + ' '.join(decls[i:i + 6]))
        # params into locals
        slot = 0
        if not m.is_static:
            if 'l0a' in self.used_vars:
                out.append('  l0a = p_this;')
            slot = 1
        for i, a in enumerate(args):
            k = kind_of(a)
            v = 'l%d%s' % (slot, k)
            if v in self.used_vars:
                out.append('  %s = p%d;' % (v, i))
            slot += 2 if k in 'jd' else 1
        if has_exc:
            out.append('  int jpc = 0; int jresume = -1;')
            out.append('  for (;;) {')
            out.append('  try {')
            out.append('  switch (jresume) {')
            for h in handlers:
                out.append('    case %d: goto L%d;' % (h, h))
            out.append('    default: break;')
            out.append('  }')
            out.extend(lines)
            out.append('  } catch (JObject* jex) {')
            for (s, e, h, t) in m.exc_table:
                cond = 'jpc >= %d && jpc < %d' % (s, e)
                if t and t != 'java/lang/Throwable':
                    cond += ' && jinstanceof(jex, &%s)' % cls_meta(t)
                out.append('    if (%s) { s0a = jex; jresume = %d; continue; }' % (cond, h))
                self.used_vars.add('s0a')
            out.append('    throw;')
            out.append('  }')
            out.append('  }')
        else:
            out.extend(lines)
        if not lines or not re.search(r'(return|jthrow|throw|goto)', lines[-1]):
            out.append('  jfell_off();')
            if ret != 'V':
                out.append('  return 0;')
        out.append('}')
        return out

    def try_array_init(self, ins, st, targets):
        """Detect `newarray; (dup; idx; const; Xastore)*` and emit a table copy."""
        if not st:
            return None
        prev = None
        # length must be a constant pushed by the previous instruction
        idx = self.insns.index(ins)
        if idx == 0:
            return None
        p = self.insns[idx - 1]
        length = self.const_int(p)
        if length is None or p.pc in targets or ins.pc in targets:
            return None
        atype = NEWARRAY_TYPES[ins.args[0]]
        store = {'[Z': 'bastore', '[B': 'bastore', '[C': 'castore', '[S': 'sastore',
                 '[I': 'iastore', '[J': 'lastore', '[F': 'fastore', '[D': 'dastore'}[atype]
        if atype in ('[F', '[D'):
            return None
        items = []
        j = idx + 1
        while j + 3 < len(self.insns):
            a, b, c, d = self.insns[j:j + 4]
            if a.name != 'dup' or d.name != store:
                break
            if any(x.pc in targets for x in (a, b, c, d)):
                break
            iv = self.const_int(b)
            vv = self.const_long(c) if atype == '[J' else self.const_int(c)
            if iv is None or vv is None or not (0 <= iv < length):
                break
            items.append((iv, vv))
            j += 4
        if len(items) < 8:
            return None
        end_pc = self.insns[j].pc if j < len(self.insns) else None
        if end_pc is None:
            return None
        depth = len(st) - 1
        ctype = {'[Z': 'int8_t', '[B': 'int8_t', '[C': 'uint16_t', '[S': 'int16_t',
                 '[I': 'int32_t', '[J': 'int64_t'}[atype]
        sequential = all(items[k][0] == k for k in range(len(items)))
        code = []
        arr = self.sv(depth, 'a')
        code.append('  %s = jnewarray(&%s, %d);' % (arr, self.world.array_class(atype), length))
        vals = ', '.join(self.c_lit(v, atype) for _, v in items)
        if sequential:
            code.append('  { static const %s d[] = {%s}; memcpy(jadata<%s>(%s), d, sizeof(d)); }' % (ctype, vals, ctype, arr))
        else:
            idxs = ', '.join(str(i) for i, _ in items)
            code.append('  { static const %s d[] = {%s}; static const int32_t x[] = {%s}; %s* a = jadata<%s>(%s); for (unsigned k = 0; k < %d; k++) a[x[k]] = d[k]; }'
                        % (ctype, vals, idxs, ctype, ctype, arr, len(items)))
        return code, end_pc

    @staticmethod
    def c_lit(v, atype):
        if atype == '[J':
            if v == -(1 << 63):
                return '(-0x7fffffffffffffffLL - 1)'
            return '%dLL' % v
        if v == -(1 << 31):
            return '(-0x7fffffff - 1)'
        return str(v)

    def const_int(self, ins):
        n = ins.name
        if n.startswith('iconst_'):
            return -1 if n == 'iconst_m1' else int(n[-1])
        if n in ('bipush', 'sipush'):
            return ins.args[0]
        if n in ('ldc', 'ldc_w'):
            e = self.cp(ins.args[0])
            if e[0] == 'Integer':
                return e[1]
        return None

    def const_long(self, ins):
        n = ins.name
        if n in ('lconst_0', 'lconst_1'):
            return int(n[-1])
        if n == 'ldc2_w':
            e = self.cp(ins.args[0])
            if e[0] == 'Long':
                return e[1]
        return None

    def gen(self, ins, st):
        n = ins.name
        d = len(st)
        key = (self.cf.name, self.m.name + self.m.desc, ins.pc)
        if key in self.world.const_patches:
            if self.stack_effect(ins, list(st)) != st + ['i']:
                raise TranslateError('constant patch at %s is not an int push' % (key,))
            self.world.const_patches_used.add(key)
            return '%s = %d; /* patched */' % (self.sv(d, 'i'), self.world.const_patches[key])
        sv = self.sv
        T = {'i': 'i', 'l': 'j', 'f': 'f', 'd': 'd', 'a': 'a'}
        world = self.world

        if n == 'nop':
            return ''
        if n == 'aconst_null':
            return '%s = nullptr;' % sv(d, 'a')
        if n.startswith('iconst_'):
            return '%s = %d;' % (sv(d, 'i'), -1 if n == 'iconst_m1' else int(n[-1]))
        if n in ('bipush', 'sipush'):
            return '%s = %d;' % (sv(d, 'i'), ins.args[0])
        if n.startswith('lconst_'):
            return '%s = %d;' % (sv(d, 'j'), int(n[-1]))
        if n.startswith('fconst_'):
            return '%s = %d.0f;' % (sv(d, 'f'), int(n[-1]))
        if n.startswith('dconst_'):
            return '%s = %d.0;' % (sv(d, 'd'), int(n[-1]))
        if n in ('ldc', 'ldc_w', 'ldc2_w'):
            e = self.cp(ins.args[0])
            if e[0] == 'Integer':
                return '%s = %s;' % (sv(d, 'i'), self.c_lit(e[1], '[I'))
            if e[0] == 'Long':
                return '%s = %s;' % (sv(d, 'j'), self.c_lit(e[1], '[J'))
            if e[0] == 'Float':
                bits = struct.unpack('>I', e[2])[0]
                return '%s = jbits2f(0x%08xu);' % (sv(d, 'f'), bits)
            if e[0] == 'Double':
                bits = struct.unpack('>Q', e[2])[0]
                return '%s = jbits2d(0x%016xULL);' % (sv(d, 'd'), bits)
            if e[0] == 'String':
                units = decode_mutf8(self.cf.cp[e[1]][1])
                return '%s = jstrlit(%d);' % (sv(d, 'a'), world.strlit(units))
            raise TranslateError('ldc class constant not supported')
        m_ = re.match(r'^([ilfda])load(?:_(\d))?$', n)
        if m_:
            idx = int(m_.group(2)) if m_.group(2) is not None else ins.args[0]
            k = T[m_.group(1)]
            return '%s = %s;' % (sv(d, k), self.lv(idx, k))
        m_ = re.match(r'^([ilfda])store(?:_(\d))?$', n)
        if m_:
            idx = int(m_.group(2)) if m_.group(2) is not None else ins.args[0]
            k = T[m_.group(1)]
            return '%s = %s;' % (self.lv(idx, k), sv(d - 1, k))
        m_ = re.match(r'^([ilfdabcs])aload$', n)
        if m_:
            ct, k = ARRAY_ELEM[m_.group(1)]
            return '%s = jaget<%s>(%s, %s);' % (sv(d - 2, k), ct, sv(d - 2, 'a'), sv(d - 1, 'i'))
        m_ = re.match(r'^([ilfdabcs])astore$', n)
        if m_:
            ct, k = ARRAY_ELEM[m_.group(1)]
            val = sv(d - 1, k)
            if ct != CTYPE_OF_KIND[k]:
                val = '(%s)%s' % (ct, val)
            return 'jaset<%s>(%s, %s, %s);' % (ct, sv(d - 3, 'a'), sv(d - 2, 'i'), val)
        if n == 'pop' or n == 'pop2':
            return ''
        if n in ('dup', 'dup_x1', 'dup_x2', 'dup2', 'dup2_x1', 'dup2_x2', 'swap'):
            k, mm = self.dup_shape(n, st)
            if n == 'swap':
                a, b = st[-2], st[-1]
                if a == b:
                    return '{ %s t = %s; %s = %s; %s = t; }' % (CTYPE_OF_KIND[a], sv(d - 2, a), sv(d - 2, a), sv(d - 1, a), sv(d - 1, a))
                return '{ %s t0 = %s; %s t1 = %s; %s = t1; %s = t0; }' % (
                    CTYPE_OF_KIND[a], sv(d - 2, a), CTYPE_OF_KIND[b], sv(d - 1, b), sv(d - 2, b), sv(d - 1, a))
            base = d - k - mm
            old = st[base:]
            new = st[d - k:] + st[base:d - k] + st[d - k:]
            src_index = list(range(mm, mm + k)) + list(range(0, mm)) + list(range(mm, mm + k))
            parts = []
            temps = []
            for i, kd in enumerate(old):
                temps.append('%s t%d = %s;' % (CTYPE_OF_KIND[kd], i, sv(base + i, kd)))
            for i, kd in enumerate(new):
                parts.append('%s = t%d;' % (sv(base + i, kd), src_index[i]))
            if mm == 0:
                # plain dup: no shuffling needed
                return ' '.join('%s = %s;' % (sv(d + i, st[d - k + i]), sv(d - k + i, st[d - k + i])) for i in range(k))
            return '{ ' + ' '.join(temps + parts) + ' }'
        m_ = re.match(r'^([ilfd])(add|sub|mul|div|rem|and|or|xor)$', n)
        if m_:
            k = T[m_.group(1)]
            a, b = sv(d - 2, k), sv(d - 1, k)
            op = m_.group(2)
            if op in ('div', 'rem') and k in 'ij':
                return '%s = j%s%s(%s, %s);' % (a, k, op, a, b)
            if op == 'rem':
                return '%s = jfrem(%s, %s);' % (a, a, b)
            sym = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/', 'and': '&', 'or': '|', 'xor': '^'}[op]
            if k in 'ij' and op in ('add', 'sub', 'mul'):
                ut = 'uint32_t' if k == 'i' else 'uint64_t'
                st_ = CTYPE_OF_KIND[k]
                return '%s = (%s)((%s)%s %s (%s)%s);' % (a, st_, ut, a, sym, ut, b)
            return '%s = %s %s %s;' % (a, a, sym, b)
        m_ = re.match(r'^([ilfd])neg$', n)
        if m_:
            k = T[m_.group(1)]
            a = sv(d - 1, k)
            if k == 'i':
                return '%s = (int32_t)(0u - (uint32_t)%s);' % (a, a)
            if k == 'j':
                return '%s = (int64_t)(0ull - (uint64_t)%s);' % (a, a)
            return '%s = -%s;' % (a, a)
        m_ = re.match(r'^([il])(shl|shr|ushr)$', n)
        if m_:
            k = T[m_.group(1)]
            a, b = sv(d - 2, k), sv(d - 1, 'i')
            mask = 31 if k == 'i' else 63
            ut = 'uint32_t' if k == 'i' else 'uint64_t'
            stt = CTYPE_OF_KIND[k]
            if m_.group(2) == 'shl':
                return '%s = (%s)((%s)%s << (%s & %d));' % (a, stt, ut, a, b, mask)
            if m_.group(2) == 'shr':
                return '%s = %s >> (%s & %d);' % (a, a, b, mask)
            return '%s = (%s)((%s)%s >> (%s & %d));' % (a, stt, ut, a, b, mask)
        if n == 'iinc':
            v = self.lv(ins.args[0], 'i')
            return '%s = (int32_t)((uint32_t)%s + (uint32_t)(%d));' % (v, v, ins.args[1])
        m_ = re.match(r'^([ilfd])2([ilfdbcs])$', n)
        if m_:
            fk, tk = T[m_.group(1)], m_.group(2)
            a = sv(d - 1, fk)
            if tk in 'bcs':
                ct = {'b': 'int8_t', 'c': 'uint16_t', 's': 'int16_t'}[tk]
                return '%s = (int32_t)(%s)%s;' % (sv(d - 1, 'i'), ct, a)
            tk = T[tk]
            if fk in 'fd' and tk in 'ij':
                return '%s = j%s2%s(%s);' % (sv(d - 1, tk), fk, tk, a)
            return '%s = (%s)%s;' % (sv(d - 1, tk), CTYPE_OF_KIND[tk], a)
        if n == 'lcmp':
            a, b = sv(d - 2, 'j'), sv(d - 1, 'j')
            return '%s = (%s > %s) - (%s < %s);' % (sv(d - 2, 'i'), a, b, a, b)
        if n in ('fcmpl', 'fcmpg', 'dcmpl', 'dcmpg'):
            k = n[0]
            a, b = sv(d - 2, k), sv(d - 1, k)
            nan = '-1' if n.endswith('l') else '1'
            return '%s = (%s > %s) ? 1 : (%s < %s) ? -1 : (%s == %s) ? 0 : %s;' % (sv(d - 2, 'i'), a, b, a, b, a, b, nan)
        cmp = {'eq': '==', 'ne': '!=', 'lt': '<', 'ge': '>=', 'gt': '>', 'le': '<='}
        if n in ('ifeq', 'ifne', 'iflt', 'ifge', 'ifgt', 'ifle'):
            return 'if (%s %s 0) goto L%d;' % (sv(d - 1, 'i'), cmp[n[2:]], ins.args[0])
        if n.startswith('if_icmp'):
            return 'if (%s %s %s) goto L%d;' % (sv(d - 2, 'i'), cmp[n[7:]], sv(d - 1, 'i'), ins.args[0])
        if n.startswith('if_acmp'):
            return 'if (%s %s %s) goto L%d;' % (sv(d - 2, 'a'), cmp[n[7:]], sv(d - 1, 'a'), ins.args[0])
        if n == 'ifnull':
            return 'if (%s == nullptr) goto L%d;' % (sv(d - 1, 'a'), ins.args[0])
        if n == 'ifnonnull':
            return 'if (%s != nullptr) goto L%d;' % (sv(d - 1, 'a'), ins.args[0])
        if n in ('goto', 'goto_w'):
            return 'goto L%d;' % ins.args[0]
        if n == 'tableswitch':
            default, low, tgts = ins.args
            cases = ' '.join('case %d: goto L%d;' % (low + i, t) for i, t in enumerate(tgts))
            return 'switch (%s) { %s default: goto L%d; }' % (sv(d - 1, 'i'), cases, default)
        if n == 'lookupswitch':
            default, pairs = ins.args
            cases = ' '.join('case %s: goto L%d;' % (self.c_lit(k, '[I'), t) for k, t in pairs)
            return 'switch (%s) { %s default: goto L%d; }' % (sv(d - 1, 'i'), cases, default)
        if n == 'return':
            return 'return;'
        if n.endswith('return'):
            k = T[n[0]]
            return 'return %s;' % sv(d - 1, k)
        if n in ('getstatic', 'putstatic'):
            cls, name, desc = self.cf.member_ref(ins.args[0])
            dcls, f = world.find_field(cls, name, desc)
            var = static_var(dcls, name, desc)
            guard = self.clinit_guard(dcls)
            k = kind_of(desc)
            if n == 'getstatic':
                return '%s%s = %s;' % (guard, sv(d, k), var)
            val = sv(d - 1, k)
            ft = field_ctype(desc)
            if ft != CTYPE_OF_KIND[k]:
                val = '(%s)%s' % (ft, val)
            return '%s%s = %s;' % (guard, var, val)
        if n in ('getfield', 'putfield'):
            cls, name, desc = self.cf.member_ref(ins.args[0])
            dcls, f = world.find_field(cls, name, desc)
            k = kind_of(desc)
            mem = field_member(name, desc)
            if n == 'getfield':
                return '%s = ((%s*)JNN(%s))->%s;' % (sv(d - 1, k), cls_struct(dcls), sv(d - 1, 'a'), mem)
            val = sv(d - 1, k)
            ft = field_ctype(desc)
            if ft != CTYPE_OF_KIND[k]:
                val = '(%s)%s' % (ft, val)
            return '((%s*)JNN(%s))->%s = %s;' % (cls_struct(dcls), sv(d - 2, 'a'), mem, val)
        if n.startswith('invoke'):
            return self.gen_invoke(ins, st)
        if n == 'new':
            cls = self.cf.class_name(ins.args[0])
            guard = self.clinit_guard(cls)
            return '%s%s = jnew(&%s);' % (guard, sv(d, 'a'), cls_meta(cls))
        if n == 'newarray':
            atype = NEWARRAY_TYPES[ins.args[0]]
            return '%s = jnewarray(&%s, %s);' % (sv(d - 1, 'a'), world.array_class(atype), sv(d - 1, 'i'))
        if n == 'anewarray':
            cname = self.cf.class_name(ins.args[0])
            adesc = '[' + (cname if cname.startswith('[') else 'L' + cname + ';')
            return '%s = jnewarray(&%s, %s);' % (sv(d - 1, 'a'), world.array_class(adesc), sv(d - 1, 'i'))
        if n == 'multianewarray':
            adesc = self.cf.class_name(ins.args[0])
            dims = ins.args[1]
            dvals = ', '.join(sv(d - dims + i, 'i') for i in range(dims))
            return '{ int32_t dims[] = {%s}; %s = jmultianewarray(&%s, %d, dims); }' % (
                dvals, sv(d - dims, 'a'), world.array_class(adesc), dims)
        if n == 'arraylength':
            return '%s = jalen(%s);' % (sv(d - 1, 'i'), sv(d - 1, 'a'))
        if n == 'athrow':
            return 'jthrow(%s);' % sv(d - 1, 'a')
        if n in ('checkcast', 'instanceof'):
            cname = self.cf.class_name(ins.args[0])
            meta = world.array_class(cname) if cname.startswith('[') else cls_meta(cname)
            if n == 'checkcast':
                if cname == 'java/lang/Object':
                    return ''
                return 'jcheckcast(%s, &%s);' % (sv(d - 1, 'a'), meta)
            return '%s = jinstanceof(%s, &%s);' % (sv(d - 1, 'i'), sv(d - 1, 'a'), meta)
        if n in ('monitorenter', 'monitorexit'):
            return 'JNN(%s);' % sv(d - 1, 'a')
        raise TranslateError('unhandled opcode ' + n)

    def gen_invoke(self, ins, st):
        n = ins.name
        d = len(st)
        world = self.world
        cls, name, desc = self.cf.member_ref(ins.args[0])
        args, ret = parse_method_desc(desc)
        nargs = len(args) + (0 if n == 'invokestatic' else 1)
        base = d - nargs
        argv = []
        i = base
        if n != 'invokestatic':
            argv.append(self.sv(i, 'a'))
            i += 1
        for a in args:
            argv.append(self.sv(i, kind_of(a)))
            i += 1
        result = ''
        if ret != 'V':
            result = '%s = ' % self.sv(base, kind_of(ret))
        if cls.startswith('['):
            # method on an array (clone / getClass etc.): dispatch through Object
            cls = 'java/lang/Object'
        if n == 'invokestatic':
            m = world.find_method(cls, name, desc)
            if m is None:
                raise TranslateError('unresolved static %s.%s%s' % (cls, name, desc))
            guard = self.clinit_guard(m.cls.name)
            return '%s%s%s(%s);' % (guard, result, world.fn_name(m), ', '.join(argv))
        if n == 'invokespecial':
            if name == '<init>':
                m = world.find_method(cls, name, desc)
            else:
                # super call or private method
                m = world.find_method(cls, name, desc)
            if m is None:
                raise TranslateError('unresolved special %s.%s%s' % (cls, name, desc))
            argv[0] = 'JNN(%s)' % argv[0] if name != '<init>' else argv[0]
            return '%s%s(%s);' % (result, world.fn_name(m), ', '.join(argv))
        if n == 'invokevirtual' and not world.get(cls).is_interface:
            m = world.find_method(cls, name, desc)
            if m is None:
                raise TranslateError('unresolved virtual %s.%s%s' % (cls, name, desc))
            final = (m.is_private or (m.access & ACC_FINAL) or (m.cls.access & ACC_FINAL)) and not m.is_abstract
            if final and not world.get(m.cls.name).is_interface:
                argv[0] = 'JNN(%s)' % argv[0]
                return '%s%s(%s);' % (result, world.fn_name(m), ', '.join(argv))
            slot = world.vslot[cls].get((name, desc))
            if slot is None:
                raise TranslateError('no vslot %s.%s%s' % (cls, name, desc))
            obj = argv[0]
            return '%s((%s)JNN(%s)->cls->vtable[%d])(%s);' % (
                result, fn_ptr_type(desc, False), obj, slot, ', '.join(argv))
        # interface call
        key = world.ikey(name, desc)
        obj = argv[0]
        return '%s((%s)jimethod(%s, %d))(%s);' % (result, fn_ptr_type(desc, False), obj, key, ', '.join(argv))


# --------------------------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------------------------


def load_inputs(paths, world):
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                for fn in files:
                    if fn.endswith('.class'):
                        with open(os.path.join(root, fn), 'rb') as f:
                            world.add(ClassFile(f.read()))
        else:
            with zipfile.ZipFile(p) as z:
                for zn in z.namelist():
                    if zn.endswith('.class'):
                        world.add(ClassFile(z.read(zn)))


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--main', required=True, help='MIDlet class name')
    ap.add_argument('--override', help='file listing methods implemented by hand (cls.name(desc))')
    ap.add_argument('--patch', help='file of int constant patches: "cls method(desc) pc value"')
    ap.add_argument('inputs', nargs='+')
    a = ap.parse_args()

    world = World()
    load_inputs(a.inputs, world)
    for ad in ('[B', '[C', '[I', '[Ljava/lang/Object;', '[Ljava/lang/String;'):
        world.array_class(ad)  # always available to the runtime
    overrides = set()
    if a.override:
        for line in open(a.override):
            line = line.split('#')[0].strip()
            if line:
                overrides.add(line)

    world.const_patches = {}
    world.const_patches_used = set()
    if a.patch:
        for line in open(a.patch):
            line = line.split('#')[0].strip()
            if line:
                c, m, pc, v = line.split()
                world.const_patches[(c, m, int(pc))] = int(v)

    os.makedirs(a.out, exist_ok=True)
    world.compute_layouts()
    order = world.topo_order()

    clinit_classes = set(n for n in order if any(m.name == '<clinit>' for m in world.get(n).methods)
                         or any(f.is_static and f.const_value and f.const_value[0] == 'S' for f in world.get(n).fields))

    def has_clinit(name):
        # a class needs initialisation if it or any superclass has a static initialiser
        cf = world.get(name)
        if cf.is_interface:
            return name in clinit_classes
        return any(x in clinit_classes for x in world.ancestors(name))
    world.has_clinit = has_clinit

    def fn_name(m):
        return method_fn(m.cls.name, m.name, m.desc)
    world.fn_name = fn_name

    # ---------------- header ----------------
    H = []
    H.append('// Generated by jvm2cpp. Do not edit.')
    H.append('#pragma once')
    H.append('#include "jvm.h"')
    H.append('')
    for n in order:
        H.append('extern JClass %s;' % cls_meta(n))
    H.append('')
    for n in order:
        cf = world.get(n)
        sup = cls_struct(cf.super) if cf.super else 'JObject'
        if cf.is_interface:
            continue
        H.append('struct %s : %s {' % (cls_struct(n), sup))
        for f in cf.fields:
            if not f.is_static:
                H.append('  %s %s;' % (field_ctype(f.desc), field_member(f.name, f.desc)))
        H.append('};')
    H.append('')
    for n in order:
        cf = world.get(n)
        for f in cf.fields:
            if f.is_static:
                H.append('extern %s %s;' % (field_ctype(f.desc), static_var(n, f.name, f.desc)))
        if has_clinit(n):
            H.append('extern bool IS_%s; void CI_%s();' % (mangle(n), mangle(n)))
        for m in cf.methods:
            H.append(fn_signature(world, m) + ';')
            if '%s.%s%s' % (n, m.name, m.desc) in overrides and m.code is not None:
                H.append(fn_signature(world, m, fn_name(m) + '__orig') + ';')
    H.append('')

    # ---------------- per-class sources ----------------
    errors = []
    natives = []
    file_list = []
    for n in order:
        cf = world.get(n)
        S = ['// Generated by jvm2cpp from %s. Do not edit.' % n, '#include "classes.h"', '']
        for f in cf.fields:
            if f.is_static:
                init = ''
                cv = f.const_value
                if cv and cv[0] in 'IJ':
                    init = ' = %s' % MethodTranslator.c_lit(cv[1], '[I' if cv[0] == 'I' else '[J')
                elif cv and cv[0] in 'FD':
                    init = ' = %r' % cv[1]
                elif field_ctype(f.desc) == 'JObject*':
                    init = ' = nullptr'
                else:
                    init = ' = 0'
                S.append('%s %s%s;' % (field_ctype(f.desc), static_var(n, f.name, f.desc), init))
        if has_clinit(n):
            S.append('bool IS_%s = false;' % mangle(n))
            S.append('void CI_%s() {' % mangle(n))
            S.append('  if (IS_%s) return;' % mangle(n))
            S.append('  IS_%s = true;' % mangle(n))
            if cf.super and not cf.is_interface and has_clinit(cf.super):
                S.append('  CI_%s();' % mangle(cf.super))
            for f in cf.fields:
                if f.is_static and f.const_value and f.const_value[0] == 'S':
                    S.append('  %s = jstrlit(%d);' % (static_var(n, f.name, f.desc), world.strlit(f.const_value[1])))
            for m in cf.methods:
                if m.name == '<clinit>':
                    S.append('  %s();' % fn_name(m))
            S.append('}')
        S.append('')
        for m in cf.methods:
            key = '%s.%s%s' % (n, m.name, m.desc)
            if m.is_native:
                natives.append(fn_signature(world, m) + ';')
                continue
            if m.code is None:
                continue
            fname = fn_name(m) + ('__orig' if key in overrides else '')
            try:
                t = MethodTranslator(world, cf, m)
                t.fname = fname
                S.extend(t.translate())
            except TranslateError as e:
                errors.append('%s: %s' % (key, e))
                S.append(fn_signature(world, m, fname) + ' { jthrow_internal("untranslatable method %s"); }' % key.replace('"', ''))
            S.append('')
        fn = 'cls_%s.cpp' % mangle(n)
        file_list.append(fn)
        write_if_changed(os.path.join(a.out, fn), '\n'.join(S) + '\n')

    # ---------------- metadata ----------------
    M = ['// Generated by jvm2cpp. Do not edit.', '#include "classes.h"', '#include <cstddef>', '']
    M.append('#pragma GCC diagnostic ignored "-Winvalid-offsetof"')
    for n in order:
        cf = world.get(n)
        meta = cls_meta(n)
        mn = mangle(n)
        # interfaces
        if cf.interfaces:
            M.append('static JClass* IF_%s[] = {%s};' % (mn, ', '.join('&' + cls_meta(i) for i in cf.interfaces)))
        if cf.is_interface:
            M.append('JClass %s = {%s, nullptr, %s, %d, 0, nullptr, 0, nullptr, 0, nullptr, 0, JCLS_INTERFACE, 0, nullptr, nullptr};' % (
                meta, c_string(n), ('IF_' + mn) if cf.interfaces else 'nullptr', len(cf.interfaces)))
            continue
        vt = world.vtables[n]
        ents = []
        for m in vt:
            if m.is_abstract or m.code is None and not m.is_native:
                ents.append('(void*)&jabstract_method')
            else:
                ents.append('(void*)&' + fn_name(m))
        M.append('static void* VT_%s[] = {%s};' % (mn, ', '.join(ents) if ents else '0'))
        # itable
        it = []
        for i in world.all_interfaces(n):
            for m in world.get(i).methods:
                if m.is_static or m.name == '<clinit>':
                    continue
                impl = world.vtables[n][world.vslot[n][(m.name, m.desc)]]
                fnref = '(void*)&jabstract_method' if (impl.is_abstract or (impl.code is None and not impl.is_native)) else '(void*)&' + fn_name(impl)
                ent = '{%d, %s}' % (world.ikey(m.name, m.desc), fnref)
                if ent not in it:
                    it.append(ent)
        if it:
            M.append('static const JIEntry IT_%s[] = {%s};' % (mn, ', '.join(it)))
        # ref offsets (all instance ref fields incl. inherited)
        offs = []
        for anc in world.ancestors(n):
            for f in world.get(anc).fields:
                if not f.is_static and field_ctype(f.desc) == 'JObject*':
                    offs.append('(uint16_t)offsetof(%s, %s)' % (cls_struct(anc), field_member(f.name, f.desc)))
        if offs:
            M.append('static const uint16_t RO_%s[] = {%s};' % (mn, ', '.join(offs)))
        flags = []
        if cf.access & ACC_ABSTRACT:
            flags.append('JCLS_ABSTRACT')
        M.append('JClass %s = {%s, %s, %s, %d, sizeof(%s), VT_%s, %d, %s, %d, %s, %d, %s, 0, nullptr, nullptr};' % (
            meta, c_string(n), '&' + cls_meta(cf.super) if cf.super else 'nullptr',
            ('IF_' + mn) if cf.interfaces else 'nullptr', len(cf.interfaces),
            cls_struct(n), mn, len(vt), ('IT_' + mn) if it else 'nullptr', len(it),
            ('RO_' + mn) if offs else 'nullptr', len(offs), ' | '.join(flags) if flags else '0'))
    M.append('')
    # array classes
    obj_vt = 'VT_' + mangle('java/lang/Object')
    arr_list = sorted(world.array_classes, key=lambda s: (s.count('['), s))
    for ad in arr_list:
        M.append('extern JClass %s;' % cls_meta(ad))
    for ad in arr_list:
        comp = ad[1:]
        ek = comp[0]
        if ek in 'L[':
            ek = 'L'
            if comp.startswith('['):
                compref = '&' + cls_meta(comp)
            else:
                cn = comp[1:-1]
                compref = '&' + cls_meta(cn) if cn in world.classes else 'nullptr'
        else:
            compref = 'nullptr'
        M.append('JClass %s = {%s, &%s, nullptr, 0, 0, %s, %d, nullptr, 0, nullptr, 0, JCLS_ARRAY, \'%s\', %s, nullptr};' % (
            cls_meta(ad), c_string(ad), cls_meta('java/lang/Object'), obj_vt,
            len(world.vtables['java/lang/Object']), ek, compref))
    M.append('')
    # static roots
    roots = []
    for n in order:
        for f in world.get(n).fields:
            if f.is_static and field_ctype(f.desc) == 'JObject*':
                roots.append('&' + static_var(n, f.name, f.desc))
    M.append('JObject** const jstatic_roots[] = {%s};' % (', '.join(roots) if roots else 'nullptr'))
    M.append('const int jstatic_roots_count = %d;' % len(roots))
    # string literals
    data = []
    table = []
    off = 0
    for units in world.string_list:
        table.append('{%d, %d}' % (off, len(units)))
        data.extend(units)
        off += len(units)
    M.append('const uint16_t jstrlit_data[] = {%s};' % (', '.join(str(u) for u in data) if data else '0'))
    M.append('const JStrLitEntry jstrlit_table[] = {%s};' % (', '.join(table) if table else '{0, 0}'))
    M.append('const int jstrlit_count = %d;' % len(world.string_list))
    M.append('JObject* jstrlit_objs[%d];' % max(1, len(world.string_list)))
    # all classes list (for Class.forName / debugging)
    M.append('JClass* const jall_classes[] = {%s};' % ', '.join('&' + cls_meta(n) for n in order))
    M.append('const int jall_classes_count = %d;' % len(order))
    # main entry
    mainm = world.find_method(a.main, '<init>', '()V')
    M.append('JObject* jcreate_main() {')
    M.append('  JCLINIT(%s);' % mangle(a.main) if has_clinit(a.main) else '')
    M.append('  JObject* o = jnew(&%s);' % cls_meta(a.main))
    M.append('  %s(o);' % fn_name(mainm))
    M.append('  return o;')
    M.append('}')
    write_if_changed(os.path.join(a.out, 'meta.cpp'), '\n'.join(M) + '\n')
    H.extend('extern JClass %s;' % cls_meta(ad) for ad in arr_list)
    write_if_changed(os.path.join(a.out, 'classes.h'), '\n'.join(H) + '\n')
    write_if_changed(os.path.join(a.out, 'natives.txt'), '\n'.join(natives) + '\n')
    file_list.append('meta.cpp')
    write_if_changed(os.path.join(a.out, 'sources.mk'), 'GEN_SOURCES := ' + ' '.join(file_list) + '\n')
    for k in world.const_patches:
        if k not in world.const_patches_used:
            errors.append('constant patch not applied: %s %s %d' % k)
    if errors:
        print('Translation errors:', file=sys.stderr)
        for e in errors:
            print('  ' + e, file=sys.stderr)
        sys.exit(1)
    print('translated %d classes, %d strings, %d natives' % (len(order), len(world.string_list), len(natives)))


def write_if_changed(path, text):
    try:
        with open(path) as f:
            if f.read() == text:
                return
    except IOError:
        pass
    with open(path, 'w') as f:
        f.write(text)


if __name__ == '__main__':
    main()
