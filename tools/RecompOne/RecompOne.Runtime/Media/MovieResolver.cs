namespace RecompOne.Runtime.Media;

/// <summary>Exact native STR-to-installed-movie mapping; no speculative logo aliases.</summary>
public static class MovieResolver
{
    static readonly HashSet<string> Allowed = new(StringComparer.OrdinalIgnoreCase)
    {
        "ATVILOGO", "TTSLOGO", "L1M1","L1M2","L2M1","L2M2","L2M3","L3M1","L4M1","L4M2",
        "L5M1","L5M2","L5M3","L5M4","L6M1","L7M1","L7M2","L7M3",
        "L8M1","L8M2","L8M3","L8M4","L8M5"
    };
    public static string? Resolve(string ps1Name,string directory)
    {
        // Native paths are Windows/ISO9660 strings even on Linux. Do not use the
        // host Path.GetFileName before normalizing them. No guessed LOGO alias.
        string name=ps1Name.Replace('\\','/').Split('/')[^1];
        int version=name.IndexOf(';'); if(version>=0) name=name[..version];
        if(!name.EndsWith(".STR",StringComparison.OrdinalIgnoreCase)) return null;
        string stem=name[..^4]; if(!Allowed.Contains(stem)) return null;
        if(!Directory.Exists(directory)) return null;
        string wanted=stem+".SFD";
        string[] matches=Directory.EnumerateFiles(directory,"*",SearchOption.TopDirectoryOnly)
            .Where(p=>Path.GetFileName(p).Equals(wanted,StringComparison.OrdinalIgnoreCase)).Take(2).ToArray();
        return matches.Length==1 ? matches[0] : null;
    }
}
