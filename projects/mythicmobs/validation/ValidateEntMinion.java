import java.nio.file.*;
import java.util.*;
import java.util.regex.*;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.constructor.SafeConstructor;

// Static YAML/reference validation; does not start or modify a Minecraft server.
class ValidateEntMinion {
  public static void main(String[] args) throws Exception {
    var options = new LoaderOptions();
    options.setAllowDuplicateKeys(false);
    var yaml = new Yaml(new SafeConstructor(options));
    var skills = new HashSet<String>();
    var mobs = new HashSet<String>();
    var texts = new LinkedHashMap<Path,String>();
    for (String arg : args) {
      Path path = Path.of(arg);
      String text = Files.readString(path);
      Object parsed = yaml.load(text);
      if (!(parsed instanceof Map<?,?> map)) throw new Exception("Not a YAML map: " + path);
      var ids = text.contains("  Type:") ? mobs : skills;
      for (Object key : map.keySet()) {
        if (!ids.add(key.toString())) throw new Exception("Duplicate ID: " + key);
      }
      texts.put(path,text);
      System.out.println("YAML OK: " + path + " (" + map.size() + " definitions)");
    }
    Pattern refs = Pattern.compile("(?:skill|sudoskill)\\{s=([A-Za-z0-9_]+)|(?:onStart|onTick|onHit|onEnd|onSummon)=([A-Za-z0-9_]+)");
    for (var entry : texts.entrySet()) {
      Matcher matcher = refs.matcher(entry.getValue());
      while (matcher.find()) {
        String id = matcher.group(1) != null ? matcher.group(1) : matcher.group(2);
        if (!skills.contains(id)) throw new Exception("Missing skill " + id + " in " + entry.getKey());
      }
      matcher = Pattern.compile("randomskill\\{skills=([^}]+)").matcher(entry.getValue());
      while (matcher.find()) for (String weighted : matcher.group(1).split(",")) {
        String id = weighted.trim().split(" ")[0];
        if (!skills.contains(id)) throw new Exception("Missing random skill " + id);
      }
      matcher = Pattern.compile("summon\\{type=([A-Za-z0-9_]+)").matcher(entry.getValue());
      while (matcher.find()) if (!mobs.contains(matcher.group(1))) throw new Exception("Missing mob " + matcher.group(1));
    }
    String source = texts.values().stream().filter(s -> s.contains("EM_Box:")).findFirst().orElseThrow();
    var expressions = new ArrayList<String>();
    Matcher box = Pattern.compile("var=target.em_(?:forward|side|height);[^\\n]*value=\"([^\"]+)\"").matcher(source);
    while (box.find()) expressions.add(box.group(1));
    if (expressions.size()!=3) throw new Exception("Missing rectangle expressions");
    for (int yaw=0;yaw<360;yaw+=45) {
      double rad=Math.toRadians(yaw);
      for (double[] point : new double[][]{{1,0,0,1},{1,.49,0,1},{1,.51,0,0},{2.01,0,0,0},{-.01,0,0,0},{1,0,1.51,0}}) {
        double x=-point[0]*Math.sin(rad)+point[1]*Math.cos(rad);
        double z=point[0]*Math.cos(rad)+point[1]*Math.sin(rad);
        double[] result=new double[3];
        for(int i=0;i<3;i++) {
          String expr=expressions.get(i).replace("<target.l.x>","("+x+")").replace("<target.l.z>","("+z+")")
            .replace("<target.l.y>","("+point[2]+")").replace("<caster.l.yaw>",""+yaw)
            .replace("<caster.l.x>","0").replace("<caster.l.y>","0").replace("<caster.l.z>","0");
          result[i]=new net.objecthunter.exp4j.ExpressionBuilder(expr).build().evaluate();
        }
        boolean hit=result[0]>=0 && result[0]<=2 && Math.abs(result[1])<=.5 && Math.abs(result[2])<=1.5;
        if(hit!=(point[3]==1)) throw new Exception("Rectangle failed at yaw="+yaw);
      }
    }
    System.out.println("PASS: YAML, unique IDs, references, actual rectangle expressions (48 cases).");
    System.out.println("Not validated: plugin runtime parsing, targeting, damage events, visual behavior.");
  }
}


