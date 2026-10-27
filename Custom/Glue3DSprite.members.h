// Hand-written members of the generated Glue3DSprite class (the converter includes this file inside the
// class, like a C# partial class; it never overwrites it). The definitions are in:
//   CameraControls.cpp  - Unity-style camera (right-mouse look, wheel zoom, middle-drag pan, Q/E)
//   SceneGizmo.cpp      - Select / Move / Rotate / Scale toolbar and the 3D gizmos
//   MainMenu.cpp        - start menu (create / load project), saving, Ctrl+S
//   BuildGame.cpp       - File > build.. (turns the project into a game .exe)
//   ScriptBlocks.cpp    - GluePP's extra script blocks (else, repeat, for, and many more per category)
// In the game player (Player/PlayerStubs.cpp), the editor-only ones are empty.
// (No #pragma once: this is pasted into the class body.)

public:
    // ================= CameraControls.cpp =================

    /// Degrees the camera turns per stage unit of mouse movement while right-dragging (the stage is 480 units wide).
    static constexpr double MouseLookSensitivity = 0.5;
    /// Set to true to make dragging up look down.
    static constexpr bool InvertMouseLookY = false;
    /// How far the camera can look up/down. 90 = straight up/down, like Unity (the original was 45).
    static constexpr double MaxCameraPitch = 90;
    /// How far one mouse-wheel notch moves the camera forward/back.
    static constexpr double WheelZoomStep = 0.5;
    /// How far the camera slides per stage unit of middle-button drag.
    static constexpr double PanSpeed = 0.02;

    /// Horizontal / vertical camera turn this frame from right-mouse dragging.
    double MouseLookX();
    double MouseLookY();
    /// Called at the end of the camera block every frame: wheel zoom, middle-drag pan, Q/E up/down.
    void UnityStyleCameraExtras(Value speed);

    // ---- game controls: game.movement's 3rd and 4th values (CameraControls.cpp) ----
    //   game.movement(val speed, val turn_speed, str controls, str view)
    //   controls: what turns the view   "arrows" (default), "mouse" or "both"
    //   view:     which way it turns    "left/right" (default), "up/down" or "both"
    // Arrow keys that don't turn the view move instead: Up/Down walk forward/back, Left/Right step sideways.
    // With mouse controls, click the game to lock the mouse to it; Escape frees it.

    /// Degrees the view turns per stage unit of mouse movement (times the block's turn speed).
    static constexpr double GameMouseSensitivity = 0.5;
    /// How far a player can look up/down (degrees).
    static constexpr double GameMaxPitch = 60;
    /// The controls and view of the game.movement block running right now.
    std::string GameControls = "arrows", GameView = "leftright";
    void SetGameControls(const Value& controls, const Value& view);
    bool GameUsesMouse();
    bool GameUsesArrows();
    bool GameViewLeftRight();
    bool GameViewUpDown();
    double MoveForwardAxis();
    double MoveStrafeAxis();
    double TurnAxis();
    double MouseTurn(Value turnSpeed);
    double SteerAxis();
    /// Looking up/down (arrows and/or mouse); turns the game camera. Called every frame by game.movement.
    void GameLook(Value turnSpeed);
    bool InGameView(double x, double y);
    /// Hooked to Engine::BeforeScripts / Engine::Overlay (editor and game player).
    void GameControlsUpdate();
    void GameControlsDraw(Rect stageRect);
    int gameMouseFrames_ = 0;    // > 0 while a game.movement block with mouse controls is running
    int gameMovementFrames_ = 0; // > 0 while any game.movement block is running
    double gamePitch_ = 0;       // up/down look angle (degrees, + = up)
    bool gamePrevMouseDown_ = false;

    /// True while Glue3D draws the item being dragged (it then gets no hover glow, which used to flicker).
    bool drawingDragged_ = false;

    // ================= Scrolling.cpp: the mouse wheel scrolls Glue3D's scrollbars =================

    /// How close (stage units) the mouse must be to a scrollbar, on the side of the content it scrolls.
    static constexpr double ScrollReachVertical = 120, ScrollReachHorizontal = 70;
    /// With Shift held, a horizontal bar reacts this far above it (anywhere over what it scrolls).
    static constexpr double ScrollReachHorizontalShift = 220;
    /// Called by every scrollbar as it is drawn (ScrollerElement): remembers the one nearest the mouse.
    void ScrollbarSeen(const Value& axis, const Value& pos, const Value& from, const Value& to, const Value& address, const Value& max);
    /// Hooked to Engine::BeforeScripts: the mouse wheel moves that scrollbar.
    void ScrollWheelUpdate();
    struct ScrollTarget
    {
        double Distance = 1e300;
        int Address = 0;  // -1: a bar the script editor scrolls itself
        double Max = 0;
        bool Near = false; // horizontal: within the normal reach (not just the Shift reach)
    };
    ScrollTarget scrollVertical_, scrollHorizontal_; // nearest bars of each kind, found while drawing
    void ScrollBy(const ScrollTarget& t, bool horizontal, double wheel);

    // ================= MainMenu.cpp =================

    /// True while the start menu is showing (the editor is hidden). Always false in a built game.
    bool MainMenuActive = BuildFlavor::IsEditor;
    /// Folder and name of the open project ("" until one is created or loaded).
    std::string ProjectFolder, ProjectName;

    void MainMenuFrame();
    void LoadProject(const std::string& folder);
    void SaveProject();
    /// Hooked to Engine::BeforeScripts: Ctrl+S saves while editing.
    void SaveShortcutUpdate();
    /// Glue3D's message popup. The short click delay stops the click that caused it from closing it.
    void Notify(const Value& message, const Value& icon, const Value& sound, const Value& dismissable);

private:
    enum class MenuDialog { None, Create, Load };
    MenuDialog menuDialog_ = MenuDialog::None;
    // "create a new project": which engine parts are switched on.
    bool engineTile_ = true, engineSegment_ = true, engineMode7_ = true;
    // "load a project": the folder being browsed ("" = list of drives) and what's in it.
    struct BrowseEntry
    {
        std::string Path, Label, Detail;
        bool IsProject, IsUp;
    };
    std::string browseFolder_;
    std::vector<BrowseEntry> browseEntries_;
    int browseScroll_ = 0;
    bool clickTaken_ = false; // only one menu control reacts to a click

    bool MenuInputFree();
    bool Hovering(double x1, double y1, double x2, double y2);
    bool TakeClick();
    void Label(const std::string& text, double x, double y, double scale, bool center = false, double ghost = 50,
               double brightness = -100, int limit = 0);
    void InfoText(const std::string& text, double x, double y, double scale = 0.45);
    void DrawMenuBackground();
    void DrawMenuButtons();
    void MenuButton(double x1, double y1, double x2, double y2, const char* icon, const char* title, const char* subtitle, int action);
    void DrawBannerCarousel();
    void DrawWelcomePopup();
    std::string EngineString() const;
    void DrawCreateDialog();
    void EngineToggle(const char* costume, const char* label, double x, bool& on);
    void TryCreateProject();
    void OpenBrowser(const std::string& folder);
    void DrawLoadDialog();
    void EnterEditor();

public:
    // ================= SceneGizmo.cpp =================

    enum class SceneTool { Select, Move, Rotate, Scale };
    /// Tool selected when the app starts.
    SceneTool CurrentSceneTool = SceneTool::Move;
    /// Show gizmo handles on the selected object (the toolbar's last button).
    bool ShowGizmo = true;

    /// True while the editable 3D scene view is on screen and nothing (dialog, menu, running game) is over it.
    bool SceneViewActive();
    bool InSceneViewport(double x, double y);
    /// Hooked to Engine::BeforeScripts / Engine::Overlay.
    void SceneGizmoUpdate();
    void SceneGizmoDraw(Rect stageRect);
    // Used by automated tests (and handy from your own code).
    std::string DescribeObject(const std::string& name);
    void SetCamera(double x, double y, double z, double yaw, double pitch);
    void AddTestCamera(const std::string& name, double x, double y, double z);
    void AddTestObject(const std::string& name, double x, double y, double z);

private:
    std::string selected_;            // name of the selected object ("" = none)
    int hotHandle_ = -1;              // handle under the mouse (for highlighting)
    int dragHandle_ = -1;             // handle being dragged, or DirectDrag
    double dragStartMouseX_ = 0, dragStartMouseY_ = 0;
    double dragStart_[3] = {0, 0, 0}; // the 3 values being edited, when the drag began
    Vec2 dragAxisScreen_;             // move/scale: axis direction on screen
    double dragArm_ = 0;              // world length of the gizmo arm when the drag began
    Vec2 ringTangent_;
    double ringRadius_ = 0;
    double dragPlaneStartX_ = 0, dragPlaneStartZ_ = 0;
    bool consumingClick_ = false, prevMouseDown_ = false, sceneViewActive_ = false;
    int hoverButton_ = -1;

    struct CameraInfo { double camX, camY, camZ, sin, cos, fov, pitch, kx, ky; };
    struct SceneObject { std::string Name, Type; double X, Y, Z; };
    double ViewLeft();
    double ViewTop();
    double ViewRight();
    double ViewBottom();
    CameraInfo Camera();
    bool Project(double x, double y, double z, Vec2& screen, double& depth);
    bool ProjectV(Vec3 p, Vec2& s);
    bool UnprojectToPlane(double sx, double sy, double planeY, double& x, double& z);
    double ArmLength(double depth);
    std::vector<SceneObject> SceneObjects();
    double ReadObject(const std::string& name, const char* key);
    void WriteObject(const std::string& name, const char* key, double value);
    const char* const* EditedKeys();
    void ButtonPos(int i, double& x, double& y);
    int ToolbarButtonAt(double mx, double my);
    std::string PickObject(double mx, double my);
    bool SelectedPivot(Vec3& pivot, Vec2& screen, double& arm);
    int HandleAt(double mx, double my);
    void BeginDrag(int handle, double mx, double my);
    void UpdateDrag(double mx, double my);
    double AlongAxis(Vec2 mouseDelta);
    void DrawGizmo(Vec3 pivot, double arm, float k, Rect stageRect);
    void DrawLabel(const std::string& text, Vec2 at, float k);
    void DrawReadout(Vec2 at, float k);
    void DrawToolbar(float k, Rect stageRect);

public:
    // ================= BuildGame.cpp =================

    /// File > build..
    void OpenBuildDialog();
    /// Called every editor frame from Glue3DAll (after the editor is drawn, before the pointer).
    void BuildDialogFrame();

private:
    bool buildDialogOpen_ = false;
    int buildStep_ = 0; // 0 = idle, 1 = show "building..." this frame, 2 = build now
    bool buildFullscreen_ = false;
    bool buildIntro_ = true;
    std::string buildLoadingImage_, buildLoadingImageName_; // custom loading screen: file in the project folder
    void PickLoadingImage();
    void ClearLoadingImage();
    void CloseBuildDialog();
    void BuildToggle(const char* label, const char* icon, double y, bool& on, const char* hint, double interactive);
    void RunBuild();
    void CopyLevels(const std::filesystem::path& data);
    void LoadBuildSettings();
    void SaveBuildSettings();

public:
    // ================= ScriptBlocks.cpp: GluePP's extra script blocks (see README.md) =================

    /// Called for every script line before Glue3D's own blocks; true = it was one of GluePP's blocks.
    bool CustomCommand();
    /// Extra operators for if / while / wait_until (!=, >=, <=, contains); called first by Glue3D's Operation.
    bool CustomOperation(const Value& op);
    /// game.message's text over the game view.
    void ScriptBlocksDraw(Rect stageRect);
    /// Tests: run script lines on one object (automation "runscript"), one step per frame.
    void TestRunScript(const std::string& object, const std::vector<std::string>& lines);
    void TestStep();
    /// Script editor: lines starting with these words indent the lines after them / step back themselves.
    bool BlockIndents(const Value& firstWord);
    bool BlockDedents(const Value& firstWord);
    /// Script editor: blocks that answer in data_return get "var variable = data_return" added when dropped.
    bool BlockReturnsValue(const Value& category, const Value& command);
    /// Script editor: how far the block palette moves per scrollbar step (long categories move further).
    double PaletteScrollStep();
    /// Script editor: a palette block's label size (long names get smaller instead of sticking out).
    double PaletteLabelScale(const Value& label);
    double PaletteOverflow();
    bool PaletteShown();
    double PaletteWheelStep();
    /// Script editor: where the left sidebar's scrollbar ends (as long as the block palette has to scroll).
    double SidebarScrollEnd(double from, double to);

private:
    bool FlowBlocks();
    bool ObjectBlocks();
    bool PhysicsBlocks();
    bool GameBlocks();
    bool MathBlocks();
    bool VariableBlocks();
    bool TerminalBlocks();
    bool InterfaceBlocks();
    bool TimeBlocks();
    bool ListBlocks();
    bool SoundBlocks();
    bool ModelBlocks();
    String Cmd();
    Value Arg(int n);
    Value ArgType(int n);
    Value ReadData(const Value& object, const Value& key);
    void WriteData(const Value& object, const Value& key, Value v); // by value: v may be stage.dataReturn
    void SetReturn(Value v);
    bool ObjectExists(const Value& name);
    bool HasPhysics();
    void JumpTo(int index);
    int FindBlockEnd(int from, const char* open, const char* close);
    int FindBlockStart(int from, const char* open, const char* close);
    std::string LoopKey(int line);
    bool Condition();
    bool ListRead(const Value& name, std::vector<std::string>& items);
    void ListWrite(const Value& name, const std::vector<std::string>& items);
    struct LoopState
    {
        double Count = 0, To = 0, Step = 0;
    };
    std::map<std::string, LoopState> loopCounters_; // running repeat / for loops, by object, script and line
    std::string gameMessage_;
    double gameMessageUntil_ = 0;
    std::vector<std::string> testObjects_;
    bool TestStepObject(const std::string& name);

    // ================= ScriptSystems.cpp: events, functions, collision, key presses, tags =================
public:
    /// Every frame before scripts (called from GameControlsUpdate): counts key presses for game.key_down/up.
    void ScriptBlocksUpdate();

private:
    struct ObjectRecord
    {
        std::string Name;
        int Index = 0, Length = 0; // where its record is in stage.scene
    };
    bool SystemsCommand();
    bool EventBlocks();
    bool FunctionBlocks();
    bool CollisionBlocks();
    bool KeyEventBlocks();
    bool TagBlocks();
    std::vector<ObjectRecord> ObjectRecords();
    Value RecordValue(const ObjectRecord& o, const char16_t* key, Value fallback = Value());
    bool IsSolid(const ObjectRecord& o);
    bool Matches(const ObjectRecord& o, const std::string& nameOrTag);
    void Box(const ObjectRecord& o, double centre[3], double size[3]);
    bool BoxesTouch(const ObjectRecord& a, const ObjectRecord& b);
    const ObjectRecord* FindObject(const std::vector<ObjectRecord>& all, const std::string& name);
    void ResetIfNewRun();
    bool KeyEdge(const std::string& key, bool release);
    void SendEvent(const std::string& object, const std::string& event);
    bool TakeEvent(const std::string& event);
    int FindFunction(const std::string& name);
    std::vector<Value> LineArgs(const std::string& line);
    struct KeyState
    {
        bool Down = false;
        long Presses = 0, Releases = 0;
    };
    std::map<std::string, KeyState> keys_;                       // keys scripts ask about
    std::map<std::string, long> keySeen_;                        // per object + key: presses already reported
    std::map<std::string, std::set<std::string>> events_;        // per object: events not taken yet
    std::map<std::string, std::vector<int>> callStacks_;         // per object + script: where each call returns to
    std::map<std::string, double> cooldowns_;                    // per object + name: when it last fired
    double lastRunClock_ = 0;

    // saved progress, levels, pause, time scale, clones
public:
    /// time.scale while the game runs (Glue3D's delta time is multiplied by it).
    double TimeScale();
    /// game.pause: is the object Glue3D is about to run paused?
    bool ScriptPaused();
    /// True while the editor plays another level (project) with game.load_scene; saving is blocked then.
    bool LevelActive() const;

private:
    struct StageSnapshot;
    bool SaveBlocks();
    bool TimeScaleBlocks();
    bool LevelBlocks();
    bool CloneBlocks();
    std::string SaveFilePath();
    void WriteSaveFile();
    std::string LevelFolder(const std::string& name);
    void LevelUpdate();
    void LoadLevel(const std::string& name);
    StageSnapshot TakeSnapshot();
    void RestoreSnapshot(const StageSnapshot& snap);
    std::shared_ptr<StageSnapshot> levelBackup_; // the editor's project while another level plays
    std::string pendingLevel_, currentLevel_;
    bool pendingRestart_ = false;
    bool paused_ = false;
    std::set<std::string> pauseExempt_;
    double timeScale_ = 1;
    std::map<std::string, std::string> lastClone_; // per object: the clone it made last

    // inline answers, compound conditions, text helpers
    void RunBlockLine(const std::string& text);
    bool InlineAnswer();
    Value ConditionValue(const std::string& type, const std::string& value);
    bool CompoundCondition(bool& result);
    bool EvalCondition(const std::vector<std::string>& words, bool& result);
    bool TextBlocks();

    // polish: camera shake, gamepads, 3D sound, UI styles and bars, animation, debug drawing
public:
    /// game.camera_shake's offset for the camera this frame (axis 0 = x, 1 = y, 2 = z).
    double CameraShake(int axis);
    /// A UI element's style while Glue3D draws it (interface.color / font_size / text_brightness).
    double UiStyle(const char16_t* key, double fallback);

private:
    bool PolishGameBlocks();
    bool GamepadBlocks();
    bool SoundAtBlocks();
    bool UiBlocks();
    bool AnimationBlocks();
    bool DebugBlocks();
    bool IsInputDown(const std::string& key);
    void ShakeUpdate();
    void AnimationUpdate();
    bool WorldToStage(double x, double y, double z, Vec2& out);
    void DrawSystemsOverlay(Rect stageRect);
    double shakeStrength_ = 0, shakeLength_ = 1, shakeStart_ = 0, shakeOffset_[3] = {0, 0, 0};
    long soundCounter_ = 0;
    struct Key
    {
        double Time = 0;
        std::array<double, 3> Rot{};
    };
    std::map<std::string, std::map<std::string, std::vector<Key>>> animations_; // name -> bone -> keyframes
    struct Playing
    {
        std::string Name;
        double Start = 0, Speed = 1;
        bool Loop = true, Running = false;
        std::map<std::string, std::array<double, 3>> Applied; // bone -> rotation given so far
    };
    std::map<std::string, Playing> playing_; // per object
    bool showHitboxes_ = false;
    struct DebugLine
    {
        double P[6] = {0, 0, 0, 0, 0, 0};
        double Until = 0;
    };
    std::vector<DebugLine> debugLines_;

    // ================= ScriptTerminal.cpp: terminal tools =================
public:
    /// Every terminal line goes through this first (Glue3D's TerminalPrint calls it); true = handled.
    bool TerminalHook(const Value& message, const Value& color);
    /// The dev console: a line typed into it (also used by tests).
    void SubmitConsole(const std::string& text);

private:
    bool TerminalBlocksMore();
    void ResetTerminalTools();
    void TerminalUpdate();
    void DrawTerminalOverlay(Rect stageRect);
    void PrintLevel(int level, const std::string& text);
    void SetWatch(const std::string& label, const std::string& value);
    void DumpObject(const std::string& name);
    void Breakpoint(const std::string& why);
    bool ObjectIs(const std::string& object, const std::string& nameOrTag);
    int ScriptLineNumber();
    std::string SourceText();
    bool GameRunning();
    std::vector<std::pair<std::string, std::string>> watches_; // label, value (in order)
    std::set<std::string> printedOnce_;
    std::map<std::string, double> printEvery_;
    std::map<std::string, long> counts_;
    std::map<std::string, double> timeStarts_;
    bool showSource_ = false, timestamps_ = false, fpsWatch_ = false, overlay_ = false, logToFile_ = false;
    int filterLevel_ = 0, printLevel_ = -1;
    double maxLines_ = 1000;
    std::set<std::string> muted_, only_;
    bool breakpoint_ = false;
    bool consoleOpen_ = false;
    std::string consoleLine_, inputAsker_, inputPrompt_;
    std::map<std::string, std::string> inputAnswers_;
    std::vector<std::string> lastCommand_;

    // ================= ScriptVariables.cpp: variables =================
public:
    /// The object whose variables the editor's panel shows while the game runs (the selected one).
    std::string InspectedObject();
    /// After Glue3D reads a block's values: "global x", "local x" and "var slot[2]" give their values.
    void ApplyValueTypes();
    void SelectForTest(const std::string& name); // automation "select" (editor only)

private:
    struct Token
    {
        enum KindT { Word, Value, Op, Call } Kind;
        std::string Text;
    };
    struct Parser;
    friend struct Parser;
    std::vector<Token> Tokenize(const std::string& text);
    scratch::Value RunAnswerBlock(const std::string& text, bool& waiting);
    bool EvalExpression(const std::string& text, scratch::Value& out, bool& waiting);
    bool EvalConditionText(const std::string& text, bool& out);
    bool LineCondition(bool& result);
    bool FindVar(const std::string& name, scratch::Value& out);
    std::string ArrayName(const std::string& name);
    scratch::Value ReadVar(const std::string& name);
    scratch::Value ReadGlobal(const std::string& name);
    void DeleteVar(const scratch::Value& object, const std::string& name);
    std::string FrameKey();
    void MakeLocal(const std::string& name);
    void PushFrame();
    void PopFrame();
    bool AssignCommand();
    bool VarBlocks();
    void VariablesUpdate();
    void ResetVariableTools();
    void DrawVariablesPanel(Rect stageRect, Rect view);
    bool SaveGet(const std::string& key, scratch::Value& out);
    void SaveSet(const std::string& key, const scratch::Value& v);
    std::map<std::string, scratch::Value> globals_;
    std::set<std::string> constants_;               // "object#name" (or "global#name")
    struct Change
    {
        bool Seen = false;
        scratch::Value Last, Previous;
    };
    std::map<std::string, Change> changes_;         // var.changed / previous, per object#name
    std::map<std::string, std::string> savedVars_;  // saved var: object#name -> value last saved
    std::map<std::string, std::vector<std::map<std::string, std::optional<scratch::Value>>>> callFrames_;
    bool strictVars_ = false;
    std::set<std::string> strictWarned_;

public:
