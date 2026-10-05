package java.lang;

public class Throwable {
    private String detailMessage;
    public Throwable() {}
    public Throwable(String message) { detailMessage = message; }
    public String getMessage() { return detailMessage; }
    public String toString() {
        String n = getClass().getName();
        return detailMessage == null ? n : n + ": " + detailMessage;
    }
    public void printStackTrace() { System.out.println(toString()); }
}
