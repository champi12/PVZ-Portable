package javax.microedition.rms;

/**
 * Record stores are kept in memory and written to the memory stick as one file per store
 * ("<name>.rms") whenever they change.
 */
public class RecordStore {
    private static RecordStore[] open = new RecordStore[4];
    private static int openCount;

    private String name;
    private byte[][] records = new byte[4][];   // index = recordId - 1; null = deleted
    private int nextId = 1;
    private int opens;

    private RecordStore(String name) { this.name = name; }

    private static native byte[] readFile(String name);
    private static native void writeFile(String name, byte[] data);
    private static native void deleteFile(String name);

    public static RecordStore openRecordStore(String name, boolean create) throws RecordStoreException {
        for (int i = 0; i < openCount; i++) {
            if (open[i].name.equals(name)) { open[i].opens++; return open[i]; }
        }
        RecordStore rs = new RecordStore(name);
        byte[] data = readFile(name);
        if (data == null) {
            if (!create) throw new RecordStoreNotFoundException(name);
            rs.save();
        } else {
            rs.load(data);
        }
        if (openCount == open.length) {
            RecordStore[] n = new RecordStore[openCount * 2];
            System.arraycopy(open, 0, n, 0, openCount);
            open = n;
        }
        open[openCount++] = rs;
        rs.opens = 1;
        return rs;
    }

    public static void deleteRecordStore(String name) throws RecordStoreException {
        for (int i = 0; i < openCount; i++) {
            if (open[i].name.equals(name)) throw new RecordStoreException("open");
        }
        if (readFile(name) == null) throw new RecordStoreNotFoundException(name);
        deleteFile(name);
    }

    public static String[] listRecordStores() { return null; }

    public void closeRecordStore() throws RecordStoreException {
        if (opens <= 0) throw new RecordStoreNotOpenException();
        if (--opens == 0) {
            for (int i = 0; i < openCount; i++) {
                if (open[i] == this) {
                    open[i] = open[--openCount];
                    open[openCount] = null;
                    break;
                }
            }
        }
    }

    private void load(byte[] d) {
        int p = 0;
        nextId = ((d[p] & 0xFF) << 24) | ((d[p + 1] & 0xFF) << 16) | ((d[p + 2] & 0xFF) << 8) | (d[p + 3] & 0xFF);
        p += 4;
        records = new byte[nextId > 4 ? nextId : 4][];
        while (p + 8 <= d.length) {
            int id = ((d[p] & 0xFF) << 24) | ((d[p + 1] & 0xFF) << 16) | ((d[p + 2] & 0xFF) << 8) | (d[p + 3] & 0xFF);
            int len = ((d[p + 4] & 0xFF) << 24) | ((d[p + 5] & 0xFF) << 16) | ((d[p + 6] & 0xFF) << 8) | (d[p + 7] & 0xFF);
            p += 8;
            byte[] r = new byte[len];
            System.arraycopy(d, p, r, 0, len);
            p += len;
            if (id >= 1 && id <= records.length) records[id - 1] = r;
        }
    }

    private static int put(byte[] d, int p, int v) {
        d[p] = (byte)(v >> 24); d[p + 1] = (byte)(v >> 16); d[p + 2] = (byte)(v >> 8); d[p + 3] = (byte)v;
        return p + 4;
    }

    private void save() {
        int size = 4;
        for (int i = 0; i < records.length; i++) if (records[i] != null) size += 8 + records[i].length;
        byte[] d = new byte[size];
        int p = put(d, 0, nextId);
        for (int i = 0; i < records.length; i++) {
            if (records[i] == null) continue;
            p = put(d, p, i + 1);
            p = put(d, p, records[i].length);
            System.arraycopy(records[i], 0, d, p, records[i].length);
            p += records[i].length;
        }
        writeFile(name, d);
    }

    private void check() throws RecordStoreNotOpenException {
        if (opens <= 0) throw new RecordStoreNotOpenException();
    }

    public int addRecord(byte[] data, int off, int len) throws RecordStoreException {
        check();
        int id = nextId++;
        if (id > records.length) {
            byte[][] n = new byte[records.length * 2][];
            System.arraycopy(records, 0, n, 0, records.length);
            records = n;
        }
        byte[] r = new byte[len];
        if (len > 0) System.arraycopy(data, off, r, 0, len);
        records[id - 1] = r;
        save();
        return id;
    }

    public void setRecord(int id, byte[] data, int off, int len) throws RecordStoreException {
        check();
        if (id < 1 || id >= nextId || records[id - 1] == null) throw new InvalidRecordIDException();
        byte[] r = new byte[len];
        if (len > 0) System.arraycopy(data, off, r, 0, len);
        records[id - 1] = r;
        save();
    }

    public void deleteRecord(int id) throws RecordStoreException {
        check();
        if (id < 1 || id >= nextId || records[id - 1] == null) throw new InvalidRecordIDException();
        records[id - 1] = null;
        save();
    }

    public byte[] getRecord(int id) throws RecordStoreException {
        check();
        if (id < 1 || id >= nextId || records[id - 1] == null) throw new InvalidRecordIDException();
        byte[] r = records[id - 1];
        if (r.length == 0) return null;
        byte[] c = new byte[r.length];
        System.arraycopy(r, 0, c, 0, r.length);
        return c;
    }

    public int getRecord(int id, byte[] buf, int off) throws RecordStoreException {
        byte[] r = getRecord(id);
        if (r == null) return 0;
        System.arraycopy(r, 0, buf, off, r.length);
        return r.length;
    }

    public int getRecordSize(int id) throws RecordStoreException {
        check();
        if (id < 1 || id >= nextId || records[id - 1] == null) throw new InvalidRecordIDException();
        return records[id - 1].length;
    }

    public int getNumRecords() throws RecordStoreNotOpenException {
        check();
        int n = 0;
        for (int i = 0; i < records.length; i++) if (records[i] != null) n++;
        return n;
    }

    public int getNextRecordID() throws RecordStoreNotOpenException { check(); return nextId; }
    public String getName() { return name; }
    public int getSize() { return 0; }
    public int getSizeAvailable() { return 1 << 20; }
    public int getVersion() { return 0; }
    public long getLastModified() { return 0; }
}
