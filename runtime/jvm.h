// Core object model shared by the translated code (gen/) and the hand-written runtime.
#pragma once

#include <cstdint>
#include <cstring>
#include <cstddef>

struct JClass;

struct JObject {
    JClass* cls;
    JObject* gc_next;   // intrusive list of all live objects
    uint32_t gc_mark;
};

// Arrays: header, then the length, then element data aligned to 8 bytes.
struct JArray : JObject {
    int32_t length;
};
static const size_t JARRAY_DATA_OFFSET = (sizeof(JArray) + 7) & ~(size_t)7;

struct JIEntry {
    int key;
    void* fn;
};

struct JStrLitEntry {
    int offset;
    int length;
};

enum {
    JCLS_INTERFACE = 1,
    JCLS_ABSTRACT = 2,
    JCLS_ARRAY = 4,
};

struct JClass {
    const char* name;
    JClass* super;
    JClass** interfaces;
    int n_interfaces;
    uint32_t instance_size;
    void** vtable;
    int vtable_len;
    const JIEntry* itable;
    int itable_len;
    const uint16_t* ref_offsets;
    int n_ref_offsets;
    int flags;
    char elem_kind;          // arrays: 'I','J','B','Z','C','S','F','D','L'
    JClass* component;       // arrays of references: component class (may be null)
    JObject* class_object;   // lazily created java.lang.Class instance
};

// ---- provided by generated meta.cpp ----
extern JObject** const jstatic_roots[];
extern const int jstatic_roots_count;
extern const uint16_t jstrlit_data[];
extern const JStrLitEntry jstrlit_table[];
extern const int jstrlit_count;
extern JObject* jstrlit_objs[];
extern JClass* const jall_classes[];
extern const int jall_classes_count;
JObject* jcreate_main();

// ---- runtime API used by generated code ----
JObject* jnew(JClass* cls);
JObject* jnewarray(JClass* arrcls, int32_t n);
JObject* jmultianewarray(JClass* arrcls, int ndims, const int32_t* dims);
bool jinstanceof_slow(JObject* o, JClass* c);
[[noreturn]] void jthrow(JObject* ex);
[[noreturn]] void jthrow_npe();
[[noreturn]] void jthrow_aioobe(int32_t idx);
[[noreturn]] void jthrow_arith();
[[noreturn]] void jthrow_cce(JObject* o, JClass* c);
[[noreturn]] void jthrow_internal(const char* msg);
[[noreturn]] void jabstract_method();
[[noreturn]] void jfell_off();
void* jimethod(JObject* o, int key);
JObject* jstrlit_create(int idx);

#define JCLINIT(m) do { if (__builtin_expect(!IS_##m, 0)) CI_##m(); } while (0)

static inline JObject* JNN(JObject* o) {
    if (__builtin_expect(o == nullptr, 0)) jthrow_npe();
    return o;
}

static inline int32_t jinstanceof(JObject* o, JClass* c) {
    if (o == nullptr) return 0;
    if (o->cls == c) return 1;
    return jinstanceof_slow(o, c) ? 1 : 0;
}

static inline void jcheckcast(JObject* o, JClass* c) {
    if (o != nullptr && o->cls != c && !jinstanceof_slow(o, c)) jthrow_cce(o, c);
}

static inline JObject* jstrlit(int idx) {
    JObject* s = jstrlit_objs[idx];
    if (__builtin_expect(s == nullptr, 0)) s = jstrlit_create(idx);
    return s;
}

template <typename T>
static inline T* jadata(JObject* a) {
    return (T*)((char*)a + JARRAY_DATA_OFFSET);
}

static inline int32_t jalen(JObject* a) {
    return ((JArray*)JNN(a))->length;
}

template <typename T>
static inline T jaget(JObject* a, int32_t i) {
    JArray* arr = (JArray*)JNN(a);
    if (__builtin_expect((uint32_t)i >= (uint32_t)arr->length, 0)) jthrow_aioobe(i);
    return jadata<T>(a)[i];
}

template <typename T>
static inline void jaset(JObject* a, int32_t i, T v) {
    JArray* arr = (JArray*)JNN(a);
    if (__builtin_expect((uint32_t)i >= (uint32_t)arr->length, 0)) jthrow_aioobe(i);
    jadata<T>(a)[i] = v;
}

static inline int32_t jidiv(int32_t a, int32_t b) {
    if (__builtin_expect(b == 0, 0)) jthrow_arith();
    if (b == -1) return (int32_t)(0u - (uint32_t)a);
    return a / b;
}
static inline int32_t jirem(int32_t a, int32_t b) {
    if (__builtin_expect(b == 0, 0)) jthrow_arith();
    if (b == -1) return 0;
    return a % b;
}
static inline int64_t jjdiv(int64_t a, int64_t b) {
    if (__builtin_expect(b == 0, 0)) jthrow_arith();
    if (b == -1) return (int64_t)(0ull - (uint64_t)a);
    return a / b;
}
static inline int64_t jjrem(int64_t a, int64_t b) {
    if (__builtin_expect(b == 0, 0)) jthrow_arith();
    if (b == -1) return 0;
    return a % b;
}
float jfrem(float a, float b);
double jfrem(double a, double b);
int32_t jf2i(float f);
int64_t jf2j(float f);
int32_t jd2i(double d);
int64_t jd2j(double d);
static inline float jbits2f(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static inline double jbits2d(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }

// ---- helpers for hand-written natives ----
JObject* jstring_from_utf8(const char* s);
JObject* jstring_from_units(const uint16_t* u, int len);
// Writes a UTF-8 copy of a java.lang.String into buf (always NUL terminated).
void jstring_to_utf8(JObject* s, char* buf, size_t size);
JObject* jclass_object(JClass* c);
extern JClass AC_AB;   // byte[]
extern JClass AC_AC;   // char[]
extern JClass AC_AI;   // int[]

// Garbage collector
void gc_collect();
void gc_maybe_collect();
void gc_request();
size_t gc_heap_bytes();
void gc_add_root(JObject** slot);
