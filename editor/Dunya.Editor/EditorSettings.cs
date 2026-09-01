using System;
using System.IO;
using System.Text.Json;

namespace Dunya.Editor;

public sealed class EditorSettings
{
    public string? ProjectRoot { get; set; }

    public string? World { get; set; }

    private static string Path =>
        System.IO.Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "dunya",
            "editor.json"
        );

    public static EditorSettings Load()
    {
        try
        {
            return File.Exists(Path)
                ? JsonSerializer.Deserialize<EditorSettings>(File.ReadAllText(Path))
                  ?? new EditorSettings()
                : new EditorSettings();
        }
        catch (Exception)
        {
            return new EditorSettings();
        }
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);

            File.WriteAllText(Path, JsonSerializer.Serialize(this));
        }
        catch (Exception)
        {
        }
    }
}
