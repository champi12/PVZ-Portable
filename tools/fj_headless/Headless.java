import org.recompile.mobile.*;
import java.io.*;
import java.util.*;
import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
/* Ejecuta un jar J2ME sin pantalla y guarda capturas.
   Uso: java Headless juego.jar ancho alto script.txt outdir
   script: lineas "ms accion args": key <code> (pulsa+suelta), down <code>, up <code>,
           tap x y, shot nombre, burst nombre n intervaloMs */
public class Headless {
    public static void main(String[] a) throws Exception {
        int w=Integer.parseInt(a[1]), h=Integer.parseInt(a[2]);
        Mobile.setPlatform(new MobilePlatform(w,h));
        Mobile.getPlatform().setPainter(new Runnable(){ public void run(){} });
        String file="file://"+new File(a[0]).getAbsolutePath();
        if(!Mobile.getPlatform().loadJar(file)){System.out.println("no jar");System.exit(1);}
        final List<String> lines=new ArrayList<>();
        BufferedReader br=new BufferedReader(new FileReader(a[3])); String l; while((l=br.readLine())!=null) if(l.trim().length()>0&&!l.startsWith("#")) lines.add(l.trim());
        final String out=a[4]; new File(out).mkdirs();
        Thread t=new Thread(){ public void run(){
            long t0=System.currentTimeMillis();
            try{
            for(String ln:lines){
                String[] p=ln.split("\\s+"); long at=Long.parseLong(p[0]);
                while(System.currentTimeMillis()-t0<at) Thread.sleep(5);
                MobilePlatform mp=Mobile.getPlatform();
                switch(p[1]){
                    case "key": mp.keyPressed(Integer.parseInt(p[2])); Thread.sleep(60); mp.keyReleased(Integer.parseInt(p[2])); break;
                    case "down": mp.keyPressed(Integer.parseInt(p[2])); break;
                    case "up": mp.keyReleased(Integer.parseInt(p[2])); break;
                    case "tap": mp.pointerPressed(Integer.parseInt(p[2]),Integer.parseInt(p[3])); Thread.sleep(60); mp.pointerReleased(Integer.parseInt(p[2]),Integer.parseInt(p[3])); break;
                    case "shot": save(mp.getLCD(), out+"/"+p[2]+".png"); break;
                    case "burst": { int n=Integer.parseInt(p[3]); int iv=Integer.parseInt(p[4]);
                        for(int i=0;i<n;i++){ save(mp.getLCD(), String.format("%s/%s_%03d.png",out,p[2],i)); Thread.sleep(iv);} break; }
                    case "quit": System.exit(0);
                }
            }}catch(Exception e){e.printStackTrace();}
            System.exit(0);
        }};
        t.setDaemon(true); t.start();
        Mobile.getPlatform().runJar();
        t.join();
    }
    static synchronized void save(BufferedImage img,String f) throws Exception {
        BufferedImage c=new BufferedImage(img.getWidth(),img.getHeight(),BufferedImage.TYPE_INT_RGB);
        c.getGraphics().drawImage(img,0,0,null); ImageIO.write(c,"png",new File(f));
    }
}
