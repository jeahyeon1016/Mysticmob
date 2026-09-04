import java.nio.file.*;
import java.util.*;
import java.util.regex.*;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.constructor.SafeConstructor;

// Static YAML/reference validation; does not start or modify a Minecraft server.
class ValidateOrcKing {
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
    System.out.println("PASS: unique IDs, YAML syntax, skill/random/callback/summon references.");
    System.out.println("Not validated: plugin runtime parsing, targeting, damage events, visual behavior.");
  }
}
