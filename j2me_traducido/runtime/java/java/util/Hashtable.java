package java.util;

public class Hashtable {
    private Object[] keys = new Object[8];
    private Object[] values = new Object[8];
    private int count;

    public Hashtable() {}
    public Hashtable(int cap) {}
    private int find(Object k) {
        for (int i = 0; i < count; i++) if (keys[i].equals(k)) return i;
        return -1;
    }
    public synchronized Object get(Object k) { int i = find(k); return i < 0 ? null : values[i]; }
    public synchronized boolean containsKey(Object k) { return find(k) >= 0; }
    public synchronized Object put(Object k, Object v) {
        if (k == null || v == null) throw new NullPointerException();
        int i = find(k);
        if (i >= 0) { Object o = values[i]; values[i] = v; return o; }
        if (count == keys.length) {
            Object[] nk = new Object[count * 2], nv = new Object[count * 2];
            System.arraycopy(keys, 0, nk, 0, count);
            System.arraycopy(values, 0, nv, 0, count);
            keys = nk; values = nv;
        }
        keys[count] = k; values[count] = v; count++;
        return null;
    }
    public synchronized Object remove(Object k) {
        int i = find(k);
        if (i < 0) return null;
        Object o = values[i];
        count--;
        keys[i] = keys[count]; values[i] = values[count];
        keys[count] = null; values[count] = null;
        return o;
    }
    public synchronized void clear() {
        for (int i = 0; i < count; i++) { keys[i] = null; values[i] = null; }
        count = 0;
    }
    public int size() { return count; }
    public boolean isEmpty() { return count == 0; }
}
