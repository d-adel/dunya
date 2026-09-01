using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Dock.Avalonia.Controls;
using Dock.Model.Avalonia;
using Dunya.Editor.Diagnostics;
using Dunya.Editor.Panels;
using Dunya.Editor.Shell;

namespace Dunya.Editor;

public partial class MainWindow : Window
{
    private readonly string m_logPath =
        Path.Combine(AppContext.BaseDirectory, "spike.log");

    private readonly ViewportHost m_viewport = new()
    {
        ProjectRoot = Program.ProjectRoot,
        World = Program.World
    };
    private readonly EntitiesPanel m_entities = new();
    private readonly InspectorPanel m_inspector = new();
    private readonly ConsolePanel m_console = new();
    private readonly ProjectPanel m_project = new();

    private readonly Factory m_factory = new();

    private EditorLayout m_layout = null!;

    public MainWindow()
    {
        InitializeComponent();

        File.WriteAllText(m_logPath, string.Empty);

        EditorLogSink.Reported = Append;

        m_viewport.Reported += Append;
        m_viewport.WorldOpened += () => Dispatcher.UIThread.Post(() =>
        {
            ShowContents();

            Authoring? ready = Author();

            if (ready != null)
            {
                ShowMaterials(ready);

                ShowProject();

                if (Program.AuthorPath != null)
                {
                    RunAuthorScript(ready, Program.AuthorPath);
                    ShowContents();
                    ShowMaterials(ready);
                }
            }

            if (Program.AutoPlay)
            {
                Play();
            }

        });

        m_entities.Picked += m_inspector.Show;
        m_inspector.Reader = m_viewport.Component;

        BuildLayout();

        this.FindControl<MenuItem>("ResetLayoutItem")!.Click += (_, _) => BuildLayout();

        m_inspector.Focused += id => m_selected = id;

        m_viewport.Picked += id => Dispatcher.UIThread.Post(() => m_entities.Select(id));

        m_viewport.PlayToggleRequested += () => Dispatcher.UIThread.Post(
            () =>
            {
                if (Author()?.Playing() == true)
                {
                    Stop();
                }
                else
                {
                    Play();
                }
            }
        );

        m_viewport.FocusRequested += () => Dispatcher.UIThread.Post(
            () => Author()?.FocusCamera(m_selected)
        );

        m_entities.AddRequested += async () =>
        {
            string? kind = await CreateDialog.Ask(this);

            if (kind != null)
            {
                CreateEntity(kind);
            }
        };

        Wire("AddBoxItem", () => AddSolid(Authoring.Box));
        Wire("AddSphereItem", () => AddSolid(Authoring.Sphere));
        Wire("SubtractSphereItem", SubtractFromSelected);
        Wire("MakeStaticItem", () => OnSelected(a => a.SetStatic(m_selected!.Value)));
        Wire("MakeDeformableItem", () => OnSelected(a => a.SetDeformable(m_selected!.Value)));
        m_project.Refresh += ShowProject;

        m_project.OpenWorld += name =>
        {
            Authoring? authoring = Author();

            if (authoring == null)
            {
                return;
            }

            Append(authoring.OpenWorld(name)
                ? "opened world " + name
                : "open FAILED: " + DunyaNative.LastError());

            ShowContents();
            ShowProject();
        };

        m_project.NewWorld += () => AskNewWorld();
        m_project.Import += () => AskImport();

        Wire("NewWorldItem", () => AskNewWorld());
        Wire("SaveWorldAsItem", () => AskSaveAs());
        Wire("ImportAssetItem", () => AskImport());
        Wire("OpenProjectItem", () => AskOpenProject());
        Wire("ExitItem", Close);
        Wire("FrameAllItem", () => Author()?.FocusCamera(null));
        Wire("AlignToCameraItem", () => Author()?.AlignToSceneCamera());
        Wire("Quality1Item", () => SetQuality(1.0f));
        Wire("Quality15Item", () => SetQuality(1.5f));
        Wire("Quality2Item", () => SetQuality(2.0f));

        this.FindControl<MenuItem>("QualityMenuItem")!.SubmenuOpened +=
            (_, _) => ShowQuality();

        MenuItem grid = this.FindControl<MenuItem>("ShowGridItem")!;

        grid.Click += (_, _) =>
        {
            m_gridVisible = !m_gridVisible;
            grid.Header = m_gridVisible ? "Hide _Grid" : "Show _Grid";
            Author()?.ShowGrid(m_gridVisible);
        };

        Wire("SaveWorldItem", () => Report(Author()?.Save() == true ? "world saved" : "world NOT saved"));
        Wire("BuildItem", () => Build());
        Wire("PlayItem", () => Play());
        Wire("StopItem", () => Stop());

        Closing += (_, _) => m_viewport.Shutdown();

        Opened += (_, _) =>
        {
            Append($"window opened   top level hwnd=0x{TryGetPlatformHandle()?.Handle ?? IntPtr.Zero:X}");

            int seconds = AutoCloseSeconds();

            if (seconds <= 0)
            {
                return;
            }

            DispatcherTimer.RunOnce(
                () =>
                {
                    Append("spike over");
                    Close();
                },
                TimeSpan.FromSeconds(seconds)
            );
        };
    }

    private void BuildLayout()
    {
        m_layout = EditorLayout.Build(
            m_factory,
            m_entities,
            m_viewport,
            m_inspector,
            m_console,
            m_project
        );

        DockControl dock = this.FindControl<DockControl>("Dock")!;

        dock.Factory = m_factory;
        dock.InitializeFactory = true;
        dock.InitializeLayout = true;
        dock.Layout = m_layout.Root;

    }

    private static ulong MaterialAt(Authoring authoring, uint index)
    {
        uint count = authoring.MaterialCount();

        return count == 0 ? 0ul : authoring.MaterialAt(Math.Min(index, count - 1));
    }

    private void ShowMaterials(Authoring authoring)
    {
        uint count = authoring.MaterialCount();

        var names = new List<string>();

        for (uint index = 0; index < count; ++index)
        {
            names.Add("material " + index);
        }

    }

    private List<uint> Stamp(Authoring authoring, ShapeRequest request, ulong material)
    {
        var made = new List<uint>();

        for (uint k = 0; k < request.ChunksZ; ++k)
        {
            for (uint j = 0; j < request.ChunksY; ++j)
            {
                for (uint i = 0; i < request.ChunksX; ++i)
                {
                    float hx = request.SizeX / request.ChunksX;
                    float hy = request.SizeY / request.ChunksY;
                    float hz = request.SizeZ / request.ChunksZ;

                    float cx = request.X + 2.0f * hx * (i - (request.ChunksX - 1) * 0.5f);
                    float cy = request.Y + 2.0f * hy * (j - (request.ChunksY - 1) * 0.5f);
                    float cz = request.Z + 2.0f * hz * (k - (request.ChunksZ - 1) * 0.5f);

                    uint entity = authoring.CreateSdf(
                        cx, cy, cz, request.Resolution, request.Margin
                    );

                    if (entity == uint.MaxValue)
                    {
                        Append("the entity was refused");

                        return made;
                    }

                    if (!authoring.AddPrimitive(
                            entity, request.Kind, Authoring.Union, material,
                            cx, cy, cz, hx, hy, hz))
                    {
                        Append("the shape was refused: " + DunyaNative.LastError());

                        return made;
                    }

                    if (request.Static && !authoring.SetStatic(entity))
                    {
                        Append("the static tag was refused");
                    }

                    if (request.Deformable && !authoring.SetDeformable(entity))
                    {
                        Append("the deformable tag was refused");
                    }

                    made.Add(entity);
                }
            }
        }

        return made;
    }

    private void RunAuthorScript(Authoring authoring, string path)
    {
        AuthorScript? script = AuthorScript.Read(path);

        if (script == null)
        {
            Append("no author script at " + path);

            return;
        }

        var materials = new List<ulong>();

        for (uint index = 0; index < authoring.MaterialCount(); ++index)
        {
            materials.Add(authoring.MaterialAt(index));
        }

        foreach (AuthorMaterial wanted in script.Materials)
        {
            ulong minted = authoring.AddMaterial(
                wanted.R, wanted.G, wanted.B, wanted.Metallic, wanted.Roughness
            );

            if (minted == 0)
            {
                Append("a material was refused: " + DunyaNative.LastError());

                return;
            }

            materials.Add(minted);
        }

        Append($"materials: {materials.Count}");

        foreach (AuthorStep step in script.Steps)
        {
            ulong material = materials[(int)Math.Min(step.Material, materials.Count - 1)];

            if (step.Op == "camera")
            {
                float[] look = step.Target ?? new[] { 0.0f, 0.0f, 0.0f };

                uint eye = authoring.CreateCamera(
                    step.X, step.Y, step.Z, look[0], look[1], look[2], step.Fov
                );

                Append(
                    eye == uint.MaxValue
                        ? "the camera was refused"
                        : "placed camera " + eye
                );

                continue;
            }

            if (step.Op == "create")
            {
                ShapeRequest request = AuthorScript.RequestOf(step);
                request.Static = step.Static;
                request.Deformable = step.Deformable;

                List<uint> made = Stamp(authoring, request, material);

                Append($"stamped {made.Count} entit{(made.Count == 1 ? "y" : "ies")}");

                continue;
            }

            if (step.At == null || step.At.Length != 3)
            {
                Append("an add step named no target");

                return;
            }

            uint target = Nearest(step.At);

            if (target == uint.MaxValue)
            {
                Append("no entity near " + AuthorScript.Describe(step.At));

                return;
            }

            bool done = authoring.AddPrimitive(
                target,
                AuthorScript.KindOf(step.Kind),
                step.Subtract ? Authoring.Subtract : Authoring.Union,
                material,
                step.X, step.Y, step.Z,
                step.Sx, step.Sy, step.Sz
            );

            Append(
                done
                    ? $"added {step.Kind} to {target}"
                    : "the shape was refused: " + DunyaNative.LastError()
            );

            if (!done)
            {
                return;
            }
        }

        Append(authoring.Save() ? "world saved" : "world NOT saved");
    }

    private uint Nearest(float[] at)
    {
        uint best = uint.MaxValue;
        float closest = 0.25f;

        foreach (WorldEntity entity in m_viewport.Contents())
        {
            if (!AuthorScript.PositionOf(
                    m_viewport.Component(entity.Id, "Pose"), out float[] position))
            {
                continue;
            }

            float dx = position[0] - at[0];
            float dy = position[1] - at[1];
            float dz = position[2] - at[2];

            float distance = dx * dx + dy * dy + dz * dz;

            if (distance < closest)
            {
                closest = distance;
                best = entity.Id;
            }
        }

        return best;
    }

    private bool m_gridVisible = true;

    private void ShowProject()
    {
        Authoring? authoring = Author();

        if (authoring == null)
        {
            return;
        }

        m_project.Show(Program.ProjectRoot, authoring.CurrentWorld());
    }

    private async void AskNewWorld()
    {
        string? name = await Prompts.Text(this, "New world", "level2");

        if (name == null || Author() is not Authoring authoring)
        {
            return;
        }

        Append(authoring.NewWorld(name)
            ? "created world " + name
            : "new world FAILED: " + DunyaNative.LastError());

        ShowContents();
        ShowProject();
    }

    private async void AskSaveAs()
    {
        Authoring? authoring = Author();

        if (authoring == null)
        {
            return;
        }

        string? name = await Prompts.Text(this, "Save world as", authoring.CurrentWorld());

        if (name == null)
        {
            return;
        }

        Append(authoring.SaveAs(name)
            ? "saved as " + name
            : "save as FAILED: " + DunyaNative.LastError());

        ShowProject();
    }

    private async void AskImport()
    {
        string? file = await Prompts.File(
            this, "Import asset", "*.mat.json", "*.obj", "*.gltf", "*.glb", "*.png", "*.jpg"
        );

        if (file == null || Author() is not Authoring authoring)
        {
            return;
        }

        string type = file.EndsWith(".mat.json", StringComparison.OrdinalIgnoreCase)
            ? "material"
            : file.EndsWith(".png", StringComparison.OrdinalIgnoreCase)
              || file.EndsWith(".jpg", StringComparison.OrdinalIgnoreCase)
                ? "texture"
                : "mesh";

        ulong minted = authoring.ImportAsset(file, type);

        Append(minted == 0
            ? "import FAILED: " + DunyaNative.LastError()
            : $"imported {type} {minted}");

        ShowProject();
        ShowMaterials(authoring);
    }

    private async void AskOpenProject()
    {
        string? folder = await Prompts.Folder(this, "Open project");

        if (folder == null)
        {
            return;
        }

        Append("reopening at " + folder);

        m_viewport.Reopen(folder, "main");
    }

    private void ShowContents()
    {
        IReadOnlyList<WorldEntity> contents = m_viewport.Contents();

        m_entities.Show(contents);

        Append($"world listed       {contents.Count} entities");
    }

    private static int AutoCloseSeconds()
    {
        string? given = Environment.GetEnvironmentVariable("DUNYA_SPIKE_SECONDS");

        return int.TryParse(given, out int seconds) ? seconds : 0;
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);


    private uint? m_selected;


    private void Wire(string name, Action action)
    {
        this.FindControl<MenuItem>(name)!.Click += (_, _) => action();
    }

    private void SetQuality(float scale)
    {
        Author()?.SetSupersample(scale);

        ShowQuality();
    }

    private void ShowQuality()
    {
        IntPtr session = m_viewport.SessionHandle;

        float scale = session == IntPtr.Zero
            ? 0.0f
            : new Authoring(session).Supersample();

        this.FindControl<MenuItem>("Quality1Item")!.IsChecked = scale == 1.0f;
        this.FindControl<MenuItem>("Quality15Item")!.IsChecked = scale == 1.5f;
        this.FindControl<MenuItem>("Quality2Item")!.IsChecked = scale == 2.0f;
    }

    private Authoring? Author()
    {
        IntPtr session = m_viewport.SessionHandle;

        if (session == IntPtr.Zero)
        {
            Append("no session yet");

            return null;
        }

        return new Authoring(session);
    }

    private void OnSelected(Func<Authoring, bool> action)
    {
        Authoring? authoring = Author();

        if (authoring == null)
        {
            return;
        }

        if (m_selected == null)
        {
            Append("nothing selected");

            return;
        }

        Append(action(authoring) ? "done" : "refused: " + DunyaNative.LastError());

        ShowContents();
    }

    private void AddSolid(uint kind)
    {
        Authoring? authoring = Author();

        if (authoring == null)
        {
            return;
        }

        uint entity = authoring.CreateSdf(0.0f, 0.0f, 0.0f, 33u, 0.5f);

        if (entity == uint.MaxValue)
        {
            Append("the entity was refused");

            return;
        }

        bool ok = authoring.AddPrimitive(
            entity, kind, Authoring.Union, authoring.DefaultMaterial(),
            0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f
        );

        Append(ok ? $"created entity {entity}" : "the primitive was refused");

        ShowContents();
    }

    private void SubtractFromSelected()
    {
        OnSelected(authoring => authoring.AddPrimitive(
            m_selected!.Value,
            Authoring.Sphere,
            Authoring.Subtract,
            authoring.DefaultMaterial(),
            0.0f,
            0.0f,
            0.0f,
            0.3f,
            0.3f,
            0.3f
        ));
    }

    private string? Build()
    {
        Authoring? author = Author();

        if (author == null)
        {
            return null;
        }

        string output = Path.Combine(
            Directory.GetCurrentDirectory(),
            "builds",
            Path.GetFileName(Path.TrimEndingDirectorySeparator(Program.ProjectRoot)));

        try
        {
            string game = author.Package(output, author.Worlds());

            Report($"built {game}");

            return game;
        }
        catch (Exception error)
        {
            Report($"build FAILED: {error.Message}");

            return null;
        }
    }

    private void Play()
    {
        Authoring? author = Author();

        if (author == null)
        {
            return;
        }

        m_console.ClearGame();

        if (!author.Play())
        {
            Report($"play FAILED: {DunyaNative.LastError()}");

            return;
        }

        EnterPlayMode(true);

        Report("playing");
    }

    private void Stop()
    {
        Authoring? author = Author();

        if (author == null)
        {
            return;
        }

        if (!author.Stop())
        {
            Report($"stop FAILED: {DunyaNative.LastError()}");

            return;
        }

        EnterPlayMode(false);

        Report("stopped");
    }

    private void EnterPlayMode(bool playing)
    {
        m_console.ShowGame(playing);

        m_viewport.Playing = playing;

        if (playing)
        {
            m_viewport.TakeFocus();
        }

        m_entities.IsEnabled = !playing;
        m_inspector.IsEnabled = !playing;

        foreach (string name in new[] { "FileMenu", "EntityMenu" })
        {
            MenuItem? menu = this.FindControl<MenuItem>(name);

            if (menu != null)
            {
                menu.IsEnabled = !playing;
            }
        }
    }

    private void CreateEntity(string kind)
    {
        Authoring? authoring = Author();

        if (authoring == null)
        {
            return;
        }

        uint made = kind switch
        {
            "Camera" => authoring.CreateCamera(0, 2, 6, 0, 0, 0, 70.0f),
            "Light" => authoring.CreateLight(),
            "Environment" => authoring.CreateEnvironment(),
            _ => CreateSolid(authoring, kind)
        };

        if (made == uint.MaxValue)
        {
            Report($"{kind} was refused: {DunyaNative.LastError()}");

            return;
        }

        Report($"created {kind} as entity {made}");

        ShowContents();
        ShowProject();
    }

    private uint CreateSolid(Authoring authoring, string kind)
    {
        uint shape = kind switch
        {
            "Sphere" => Authoring.Sphere,
            "Cylinder" => Authoring.Cylinder,
            _ => Authoring.Box
        };

        uint entity = authoring.CreateSdf(0.0f, 0.0f, 0.0f, 65u, 0.5f);

        if (entity == uint.MaxValue)
        {
            return entity;
        }

        if (!authoring.AddPrimitive(
                entity,
                shape,
                Authoring.Union,
                authoring.DefaultMaterial(),
                0.0f,
                0.0f,
                0.0f,
                0.5f,
                0.5f,
                0.5f))
        {
            return uint.MaxValue;
        }

        authoring.SetDeformable(entity);

        return entity;
    }

    private void Report(string line) => Append(line);

    private void Append(string line)
    {
        File.AppendAllText(m_logPath, line + Environment.NewLine);

        Dispatcher.UIThread.Post(() => m_console.Append(line));
    }
}
