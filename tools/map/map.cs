#:property JsonSerializerIsReflectionEnabledByDefault=true
#:property NoWarn=IL2026;IL3050

using System.Text.Json;
using System.Text.RegularExpressions;

var here = new DirectoryInfo(Directory.GetCurrentDirectory());
DirectoryInfo? repo = here;
while (repo is not null && !Directory.Exists(Path.Combine(repo.FullName, "engine")))
    repo = repo.Parent;

if (repo is null)
{
    Console.Error.WriteLine("run this from inside the dunya repo");
    return 1;
}

var root = repo.FullName;
var engine = Path.Combine(root, "engine");
var viewerPath = Path.Combine(root, "tools", "map", "template.html");
var outPath = Path.Combine(root, "tools", "map", "index.html");

var classRe = new Regex(@"^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^{]*)?\{?\s*$");
var skipRe = new Regex(@"^\s*(using|typedef|friend|static_assert|template|return|public:|private:|protected:)");

var types = new Dictionary<string, TypeInfo>();

foreach (var full in Directory.EnumerateFiles(engine, "*.h", SearchOption.AllDirectories))
{
    var rel = Path.GetRelativePath(root, full).Replace('\\', '/');
    if (rel.Contains("/_deps/")) continue;

    var lines = File.ReadAllLines(full);

    TypeInfo? current = null;
    var depth = 0;

    foreach (var raw in lines)
    {
        var line = raw.TrimEnd();

        if (current is null)
        {
            var m = classRe.Match(line);
            if (m.Success && !line.TrimEnd().EndsWith(";"))
            {
                current = new TypeInfo { name = m.Groups[1].Value, file = rel, lib = LibraryOf(rel) };
                depth = line.Contains('{') ? 1 : 0;
                if (depth == 0) continue;
            }
            continue;
        }

        var opens = line.Count(c => c == '{');
        var closes = line.Count(c => c == '}');

        if (depth == 0)
        {
            depth += opens - closes;
            continue;
        }

        if (depth == 1 && !skipRe.IsMatch(line) && (opens == closes || opens - closes == 1))
        {
            var member = ParseMember(line);
            if (member is not null) current.members.Add(member);
        }

        depth += opens - closes;

        if (depth <= 0)
        {
            if (current.members.Count > 0 && !types.ContainsKey(current.name))
                types[current.name] = current;
            current = null;
        }
    }
}

// ---------- resolve member types onto known classes ----------

foreach (var t in types.Values)
    foreach (var m in t.members)
        m.target = types.ContainsKey(m.bare) ? m.bare : null;

var owned = new HashSet<string>();
foreach (var t in types.Values)
    foreach (var m in t.members)
        if (m.target is not null && m.relation is "owns" or "owns-many" or "owns-maybe")
            owned.Add(m.target);

var roots = types.Keys.Where(k => !owned.Contains(k)).OrderBy(k => k).ToList();

var targets = ReadBuildGraph(root);

var payload = new Dictionary<string, object>
{
    ["targets"] = targets.ToDictionary(kv => kv.Key, kv => (object)new
    {
        kv.Value.name,
        kv.Value.tier,
        kv.Value.kind,
        kv.Value.lib,
        deps = kv.Value.deps.ToList(),
        runs = kv.Value.runs.ToList(),
        kv.Value.tested
    }),
    ["types"] = types.ToDictionary(kv => kv.Key, kv => (object)new
    {
        kv.Value.name,
        kv.Value.file,
        kv.Value.lib,
        members = kv.Value.members.Select(m => new
        {
            m.name, m.type, m.relation, m.bare, m.target
        }).ToList()
    }),
    ["roots"] = roots,
    ["byLib"] = types.Values
        .GroupBy(t => t.lib)
        .ToDictionary(g => g.Key, g => (object)g.Select(t => t.name).OrderBy(n => n).ToList())
};

var json = JsonSerializer.Serialize(payload);
File.WriteAllText(outPath, File.ReadAllText(viewerPath).Replace("/*DATA*/null", json));

Console.WriteLine($"{types.Count} types with members, {roots.Count} roots");
Console.WriteLine($"wrote {outPath}");
return 0;

// ---------- the build graph ----------

static Dictionary<string, BuildTarget> ReadBuildGraph(string root)
{
    var libRe = new Regex(@"add_library\(\s*([A-Za-z_]\w*)\s+(?!ALIAS)", RegexOptions.Multiline);
    var exeRe = new Regex(@"add_executable\(\s*([A-Za-z_]\w*)", RegexOptions.Multiline);
    var moduleRe = new Regex(@"dunya_module\(\s*([A-Za-z_]\w*)\s+([A-Za-z]+)\s*\)");
    var linkRe = new Regex(@"target_link_libraries\(\s*([A-Za-z_]\w*)([^)]*)\)", RegexOptions.Singleline);
    var aliasRe = new Regex(@"dunya::([a-z]\w*)");
    var testRe = new Regex(
        "add_test\\(\\s*NAME\\s+\"?[^\"\\n]+\"?\\s+COMMAND\\s+([^\\s)]+)([^)]*)\\)",
        RegexOptions.Singleline
    );
    var targetFileRe = new Regex(@"TARGET_FILE:([A-Za-z_]\w*)");

    var found = new Dictionary<string, BuildTarget>();

    foreach (var file in Directory.EnumerateFiles(root, "CMakeLists.txt", SearchOption.AllDirectories))
    {
        var rel = Path.GetRelativePath(root, file).Replace('\\', '/');
        if (rel.Contains("_deps/") || rel.StartsWith("build")) continue;

        var text = File.ReadAllText(file);
        var dir = Path.GetDirectoryName(rel)!.Replace('\\', '/');

        foreach (Match m in libRe.Matches(text))
            Ensure(found, m.Groups[1].Value, "library", dir);

        foreach (Match m in exeRe.Matches(text))
            Ensure(found, m.Groups[1].Value, "executable", dir);

        foreach (Match m in moduleRe.Matches(text))
        {
            var t = Ensure(found, m.Groups[1].Value, "library", dir);
            t.tier = m.Groups[2].Value;
        }

        foreach (Match m in linkRe.Matches(text))
        {
            var t = Ensure(found, m.Groups[1].Value, "library", dir);

            foreach (Match a in aliasRe.Matches(m.Groups[2].Value))
                t.deps.Add("dunya_" + a.Groups[1].Value);
        }

        foreach (Match m in testRe.Matches(text))
        {
            var runner = m.Groups[1].Value;
            var body = m.Groups[2].Value;

            if (found.TryGetValue(runner, out var invoked)) invoked.tested = true;

            foreach (Match a in targetFileRe.Matches(body))
            {
                if (found.ContainsKey(runner) && found.ContainsKey(a.Groups[1].Value))
                {
                    found[runner].runs.Add(a.Groups[1].Value);
                }
            }
        }
    }

    foreach (var t in found.Values)
        t.deps.RemoveWhere(d => !found.ContainsKey(d) || d == t.name);

    return found;
}

static BuildTarget Ensure(Dictionary<string, BuildTarget> found, string name, string kind, string dir)
{
    if (!found.TryGetValue(name, out var target))
    {
        found[name] = target = new BuildTarget { name = name, kind = kind };
    }

    if (target.lib.Length == 0 && dir.StartsWith("engine/dunya/"))
    {
        target.lib = dir["engine/dunya/".Length..];
    }

    return target;
}

// ---------- member parsing ----------

static Member? ParseMember(string line)
{
    var text = line.Trim();

    if (text.StartsWith("//")) return null;

    if (text.EndsWith(";")) text = text[..^1].Trim();
    else if (text.EndsWith("{")) text = text[..^1].Trim();
    else return null;

    var assign = text.IndexOf('=');
    var paren = TopLevelParen(text);
    if (assign >= 0 && (paren < 0 || assign < paren)) text = text[..assign].Trim();

    var brace = text.IndexOf('{');
    if (brace >= 0) text = text[..brace].Trim();

    if (text.Length == 0 || TopLevelParen(text) >= 0) return null;

    if (Regex.IsMatch(text, @"^(struct|class|enum|union|namespace)\b")) return null;

    var lastSpace = LastTopLevelSpace(text);
    if (lastSpace <= 0) return null;

    var name = text[(lastSpace + 1)..].Trim();
    var type = text[..lastSpace].Trim();

    if (name.Length == 0 || type.Length == 0) return null;
    if (!Regex.IsMatch(name, @"^[a-zA-Z_]\w*$")) return null;
    if (type is "return" or "case" or "else") return null;

    while (name.StartsWith("*") || name.StartsWith("&"))
    {
        type += name[0];
        name = name[1..];
    }

    var (relation, bare) = Classify(type);

    return new Member { name = name, type = type, relation = relation, bare = bare };
}

static int TopLevelParen(string s)
{
    var angle = 0;
    for (var i = 0; i < s.Length; i++)
    {
        if (s[i] == '<') angle++;
        else if (s[i] == '>') angle--;
        else if (s[i] == '(' && angle == 0) return i;
    }
    return -1;
}

static int LastTopLevelSpace(string s)
{
    var angle = 0;
    for (var i = s.Length - 1; i >= 0; i--)
    {
        var c = s[i];
        if (c == '>') angle++;
        else if (c == '<') angle--;
        else if (c == ' ' && angle == 0) return i;
    }
    return -1;
}

static (string relation, string bare) Classify(string type)
{
    var t = type.Replace("const ", "").Replace("static ", "").Replace("mutable ", "").Trim();

    if (t.EndsWith("&")) return ("references", Bare(t.TrimEnd('&', ' ')));
    if (t.EndsWith("*")) return ("references", Bare(t.TrimEnd('*', ' ')));

    var inner = Inner(t, "std::unique_ptr");
    if (inner is not null) return ("owns", Bare(inner));

    inner = Inner(t, "std::shared_ptr");
    if (inner is not null) return ("shares", Bare(inner));

    inner = Inner(t, "std::weak_ptr");
    if (inner is not null) return ("references", Bare(inner));

    inner = Inner(t, "std::span");
    if (inner is not null) return ("borrows", Bare(inner));

    inner = Inner(t, "std::optional");
    if (inner is not null) return ("owns-maybe", Bare(inner));

    inner = Inner(t, "std::vector") ?? Inner(t, "std::array") ?? Inner(t, "std::deque");
    if (inner is not null) return ("owns-many", Bare(inner));

    if (t.StartsWith("std::function")) return ("callback", "");
    if (t.StartsWith("std::unordered_map") || t.StartsWith("std::map")) return ("owns-many", "");

    return ("owns", Bare(t));
}

static string? Inner(string t, string wrapper)
{
    if (!t.StartsWith(wrapper + "<")) return null;

    var start = wrapper.Length + 1;
    var angle = 1;

    for (var i = start; i < t.Length; i++)
    {
        if (t[i] == '<') angle++;
        else if (t[i] == '>' && --angle == 0)
        {
            var arg = t[start..i];
            var comma = TopLevelComma(arg);
            return (comma >= 0 ? arg[..comma] : arg).Trim();
        }
    }

    return null;
}

static int TopLevelComma(string s)
{
    var angle = 0;
    for (var i = 0; i < s.Length; i++)
    {
        if (s[i] == '<') angle++;
        else if (s[i] == '>') angle--;
        else if (s[i] == ',' && angle == 0) return i;
    }
    return -1;
}

static string Bare(string t)
{
    t = t.Replace("const ", "").Trim().TrimEnd('&', '*', ' ');
    var angle = t.IndexOf('<');
    if (angle > 0) t = t[..angle];
    var colon = t.LastIndexOf("::", StringComparison.Ordinal);
    return colon >= 0 ? t[(colon + 2)..] : t;
}

static string LibraryOf(string rel)
{
    var parts = rel.Split('/');
    if (parts.Length < 3) return "engine";
    if (parts[1] == "dunya") return parts.Length > 2 ? parts[2] : "engine";
    return parts[1];
}

class TypeInfo
{
    public string name = "", file = "", lib = "";
    public List<Member> members = new();
}

class Member
{
    public string name = "", type = "", relation = "", bare = "";
    public string? target;
}

class BuildTarget
{
    public string name = "", tier = "", kind = "", lib = "";
    public SortedSet<string> deps = new();
    public SortedSet<string> runs = new();
    public bool tested;
}
