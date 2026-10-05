// Object allocation, garbage collection, type checks and exceptions for the translated code.
#include "classes.h"
#include "platform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// ------------------------------------------------------------------------------------------
// Heap
// ------------------------------------------------------------------------------------------

static JObject* g_all_objects = nullptr;
static size_t g_heap_bytes = 0;
static size_t g_bytes_since_gc = 0;
static bool g_gc_requested = false;
static std::vector<JObject**> g_extra_roots;

// Collect at the next safe point once this many bytes were allocated.
static const size_t GC_THRESHOLD = 3 * 1024 * 1024;

static size_t object_size(JObject* o) {
    JClass* c = o->cls;
    if (c->flags & JCLS_ARRAY) {
        size_t es;
        switch (c->elem_kind) {
            case 'J': case 'D': es = 8; break;
            case 'I': case 'F': es = 4; break;
            case 'C': case 'S': es = 2; break;
            case 'B': case 'Z': es = 1; break;
            default: es = sizeof(JObject*); break;
        }
        return JARRAY_DATA_OFFSET + es * (size_t)((JArray*)o)->length;
    }
    return c->instance_size;
}

static JObject* raw_alloc(JClass* cls, size_t size) {
    JObject* o = (JObject*)calloc(1, size);
    if (o == nullptr) {
        fprintf(stderr, "out of memory allocating %u bytes (heap %u)\n", (unsigned)size, (unsigned)g_heap_bytes);
        JObject* oom = (JObject*)calloc(1, C_java_lang_OutOfMemoryError.instance_size);
        if (oom) {
            oom->cls = &C_java_lang_OutOfMemoryError;
            oom->gc_next = g_all_objects;
            g_all_objects = oom;
            throw oom;
        }
        platform_fatal("out of memory");
    }
    o->cls = cls;
    o->gc_next = g_all_objects;
    g_all_objects = o;
    g_heap_bytes += size;
    g_bytes_since_gc += size;
    return o;
}

JObject* jnew(JClass* cls) {
    return raw_alloc(cls, cls->instance_size);
}

static int g_trace_exc = -1;

static void trace_throw(JObject* e) {
    if (g_trace_exc < 0) g_trace_exc = getenv("PVZ_TRACE_EXC") != nullptr;
    if (g_trace_exc) fprintf(stderr, "throw %s\n", e->cls->name);
}

[[noreturn]] static void throw_new(JClass* cls, void (*ctor)(JObject*)) {
    JObject* e = jnew(cls);
    trace_throw(e);
    ctor(e);
    throw e;
}

JObject* jnewarray(JClass* arrcls, int32_t n) {
    if (n < 0) throw_new(&C_java_lang_NegativeArraySizeException, M_java_lang_NegativeArraySizeException___init____V);
    size_t es;
    switch (arrcls->elem_kind) {
        case 'J': case 'D': es = 8; break;
        case 'I': case 'F': es = 4; break;
        case 'C': case 'S': es = 2; break;
        case 'B': case 'Z': es = 1; break;
        default: es = sizeof(JObject*); break;
    }
    JObject* a = raw_alloc(arrcls, JARRAY_DATA_OFFSET + es * (size_t)n);
    ((JArray*)a)->length = n;
    return a;
}

JObject* jmultianewarray(JClass* arrcls, int ndims, const int32_t* dims) {
    for (int i = 0; i < ndims; i++)
        if (dims[i] < 0) throw_new(&C_java_lang_NegativeArraySizeException, M_java_lang_NegativeArraySizeException___init____V);
    JObject* a = jnewarray(arrcls, dims[0]);
    if (ndims > 1) {
        for (int i = 0; i < dims[0]; i++) {
            JObject* sub = jmultianewarray(arrcls->component, ndims - 1, dims + 1);
            jadata<JObject*>(a)[i] = sub;
        }
    }
    return a;
}

// ------------------------------------------------------------------------------------------
// Garbage collector: precise mark & sweep, run only at safe points between frames when no
// translated Java code is on the native stack. Roots are the static fields, interned string
// literals, class objects and runtime-registered slots.
// ------------------------------------------------------------------------------------------

static std::vector<JObject*> g_mark_stack;

static inline void mark(JObject* o) {
    if (o != nullptr && !o->gc_mark) {
        o->gc_mark = 1;
        g_mark_stack.push_back(o);
    }
}

void gc_add_root(JObject** slot) { g_extra_roots.push_back(slot); }

void gc_collect() {
    for (int i = 0; i < jstatic_roots_count; i++) mark(*jstatic_roots[i]);
    for (int i = 0; i < jstrlit_count; i++) mark(jstrlit_objs[i]);
    for (JObject** r : g_extra_roots) mark(*r);
    while (!g_mark_stack.empty()) {
        JObject* o = g_mark_stack.back();
        g_mark_stack.pop_back();
        JClass* c = o->cls;
        if (c->flags & JCLS_ARRAY) {
            if (c->elem_kind == 'L') {
                JObject** d = jadata<JObject*>(o);
                int32_t n = ((JArray*)o)->length;
                for (int32_t i = 0; i < n; i++) mark(d[i]);
            }
        } else {
            for (int i = 0; i < c->n_ref_offsets; i++) mark(*(JObject**)((char*)o + c->ref_offsets[i]));
        }
    }
    JObject** link = &g_all_objects;
    size_t freed = 0;
    while (*link) {
        JObject* o = *link;
        if (o->gc_mark) {
            o->gc_mark = 0;
            link = &o->gc_next;
        } else {
            *link = o->gc_next;
            freed += object_size(o);
            free(o);
        }
    }
    g_heap_bytes -= freed;
    g_bytes_since_gc = 0;
    g_gc_requested = false;
}

void gc_maybe_collect() {
    if (g_gc_requested || g_bytes_since_gc > GC_THRESHOLD) gc_collect();
}

void gc_request() { g_gc_requested = true; }
size_t gc_heap_bytes() { return g_heap_bytes; }

// ------------------------------------------------------------------------------------------
// Type checks
// ------------------------------------------------------------------------------------------

static bool iface_extends(JClass* i, JClass* target) {
    if (i == target) return true;
    for (int k = 0; k < i->n_interfaces; k++)
        if (iface_extends(i->interfaces[k], target)) return true;
    return false;
}

static bool class_assignable(JClass* from, JClass* to) {
    if (from == to || to == &C_java_lang_Object) return true;
    if (to->flags & JCLS_INTERFACE) {
        for (JClass* k = from; k; k = k->super)
            for (int i = 0; i < k->n_interfaces; i++)
                if (iface_extends(k->interfaces[i], to)) return true;
        return false;
    }
    if (to->flags & JCLS_ARRAY) {
        if (!(from->flags & JCLS_ARRAY)) return false;
        if (from->elem_kind != 'L' || to->elem_kind != 'L') return false;
        if (to->component == nullptr || from->component == nullptr) return false;
        return class_assignable(from->component, to->component);
    }
    for (JClass* k = from; k; k = k->super)
        if (k == to) return true;
    return false;
}

bool jinstanceof_slow(JObject* o, JClass* c) { return class_assignable(o->cls, c); }

void* jimethod(JObject* o, int key) {
    JClass* c = JNN(o)->cls;
    for (JClass* k = c; k; k = k->super) {
        for (int i = 0; i < k->itable_len; i++)
            if (k->itable[i].key == key) return k->itable[i].fn;
    }
    jthrow_internal("interface method not found");
}

// ------------------------------------------------------------------------------------------
// Exceptions
// ------------------------------------------------------------------------------------------

void jthrow(JObject* ex) {
    if (ex == nullptr) jthrow_npe();
    trace_throw(ex);
    throw ex;
}

void jthrow_npe() { throw_new(&C_java_lang_NullPointerException, M_java_lang_NullPointerException___init____V); }
void jthrow_aioobe(int32_t) {
    throw_new(&C_java_lang_ArrayIndexOutOfBoundsException, M_java_lang_ArrayIndexOutOfBoundsException___init____V);
}
void jthrow_arith() { throw_new(&C_java_lang_ArithmeticException, M_java_lang_ArithmeticException___init____V); }
void jthrow_cce(JObject* o, JClass* c) {
    fprintf(stderr, "ClassCastException: %s -> %s\n", o->cls->name, c->name);
    throw_new(&C_java_lang_ClassCastException, M_java_lang_ClassCastException___init____V);
}
void jthrow_internal(const char* msg) {
    fprintf(stderr, "internal error: %s\n", msg);
    JObject* e = jnew(&C_java_lang_Error);
    M_java_lang_Error___init___Ljava_lang_String__V(e, jstring_from_utf8(msg));
    throw e;
}
void jabstract_method() {
    throw_new(&C_java_lang_AbstractMethodError, M_java_lang_AbstractMethodError___init____V);
}
void jfell_off() { jthrow_internal("execution fell off the end of a method"); }

// ------------------------------------------------------------------------------------------
// Numbers
// ------------------------------------------------------------------------------------------

float jfrem(float a, float b) { return std::fmod(a, b); }
double jfrem(double a, double b) { return std::fmod(a, b); }
int32_t jf2i(float f) { return jd2i(f); }
int64_t jf2j(float f) { return jd2j(f); }
int32_t jd2i(double d) {
    if (d != d) return 0;
    if (d >= 2147483647.0) return 2147483647;
    if (d <= -2147483648.0) return (-2147483647 - 1);
    return (int32_t)d;
}
int64_t jd2j(double d) {
    if (d != d) return 0;
    if (d >= 9223372036854775807.0) return 9223372036854775807LL;
    if (d <= -9223372036854775808.0) return (-9223372036854775807LL - 1);
    return (int64_t)d;
}

// ------------------------------------------------------------------------------------------
// Strings and classes
// ------------------------------------------------------------------------------------------

JObject* jstring_from_units(const uint16_t* u, int len) {
    JObject* chars = jnewarray(&AC_AC, len);
    if (len) memcpy(jadata<uint16_t>(chars), u, len * 2);
    J_java_lang_String* s = (J_java_lang_String*)jnew(&C_java_lang_String);
    s->f_value_AC = chars;
    s->f_offset_I = 0;
    s->f_count_I = len;
    return s;
}

JObject* jstring_from_utf8(const char* str) {
    std::vector<uint16_t> u;
    const unsigned char* p = (const unsigned char*)str;
    while (*p) {
        unsigned c = *p++;
        if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
        else if (c >= 0xC0 && p[0]) { c = ((c & 0x1F) << 6) | (p[0] & 0x3F); p += 1; }
        u.push_back((uint16_t)c);
    }
    return jstring_from_units(u.data(), (int)u.size());
}

void jstring_to_utf8(JObject* so, char* buf, size_t size) {
    size_t n = 0;
    if (so != nullptr && size > 0) {
        J_java_lang_String* s = (J_java_lang_String*)so;
        const uint16_t* d = jadata<uint16_t>(s->f_value_AC) + s->f_offset_I;
        for (int i = 0; i < s->f_count_I; i++) {
            unsigned c = d[i];
            if (c < 0x80) {
                if (n + 1 >= size) break;
                buf[n++] = (char)c;
            } else if (c < 0x800) {
                if (n + 2 >= size) break;
                buf[n++] = (char)(0xC0 | (c >> 6));
                buf[n++] = (char)(0x80 | (c & 0x3F));
            } else {
                if (n + 3 >= size) break;
                buf[n++] = (char)(0xE0 | (c >> 12));
                buf[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
                buf[n++] = (char)(0x80 | (c & 0x3F));
            }
        }
    }
    if (size > 0) buf[n] = 0;
}

JObject* jstrlit_create(int idx) {
    const JStrLitEntry& e = jstrlit_table[idx];
    JObject* s = jstring_from_units(jstrlit_data + e.offset, e.length);
    jstrlit_objs[idx] = s;
    return s;
}

JObject* jclass_object(JClass* c) {
    if (c->class_object == nullptr) {
        J_java_lang_Class* o = (J_java_lang_Class*)jnew(&C_java_lang_Class);
        o->f_vmClass_J = (int64_t)(intptr_t)c;
        c->class_object = o;
        gc_add_root(&c->class_object);
    }
    return c->class_object;
}
