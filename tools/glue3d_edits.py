#!/usr/bin/env python3
"""
Applies GluePP's hand edits to the generated Glue3D code (Generated/Glue3DSprite*.g.cpp).

sb3_to_cpp.py rewrites Generated/ from the .sb3, which would lose these changes; run this right after it:

    python3 tools/sb3_to_cpp.py sb3 Generated assets --namespace Glue3D --title Glue3D \
        --rename "`=Glue3D" --members "Glue3DSprite=../Custom/Glue3DSprite.members.h"
    python3 tools/glue3d_edits.py Generated

Every edited line is marked "// EDITED" in the code. Each edit names the method it belongs to and the exact
generated text it replaces, so it fails loudly instead of editing the wrong place.

Last, it marks the editor-only methods: every generated method that a built game can never reach (starting from
the event scripts and the hand-written code compiled into the player, skipping `BuildFlavor::IsEditor` branches)
is wrapped in `#ifndef GLUE3D_PLAYER`, so the game player doesn't contain the editor at all.
"""
import glob, os, re, sys

EDITS = [
    # ---- editor-only parts (compiled out of a built game; see Custom/BuildFlavor.h) ----
    ("Script Glue3DSprite::WhenKeyAnyPressed()",
     '    if (KeyPressed(u"/"_s + u""_s))',
     '    if (BuildFlavor::IsEditor && KeyPressed(u"/"_s + u""_s)) // EDITED: the "/rst" format shortcut is editor-only'),
    ("void Glue3DSprite::MainControl()",
     "    if (Eq(stage.allExportState, 0))",
     "    if (BuildFlavor::IsEditor && Eq(stage.allExportState, 0)) // EDITED: editor-only"),
    ("void Glue3DSprite::Glue3DRenderAll()",
     "    if (Eq(stage.engineFullscreen, 0))",
     "    // EDITED: a built game always shows the fullscreen game view; the editor screen isn't compiled into it.\n"
     "    if (BuildFlavor::IsEditor && Eq(stage.engineFullscreen, 0))"),
    ("void Glue3DSprite::MainKeyDetection()",
     '    else if (Eq(stage.editorTab, u"script"_s) && Eq(stage.textEditor, 1))',
     '    else if (BuildFlavor::IsEditor && Eq(stage.editorTab, u"script"_s) && Eq(stage.textEditor, 1)) // EDITED: editor-only'),
    ("void Glue3DSprite::MainKeyDetection()",
     "    if (Eq(stage.allExportState, 0))",
     "    if (BuildFlavor::IsEditor && Eq(stage.allExportState, 0)) // EDITED: editor-only shortcuts"),

    # ---- start menu, File > build.. ----
    ("void Glue3DSprite::Glue3DAll()",
     "        MainTime();\n"
     "        Glue3dExecute();\n"
     "        MainControl();\n"
     "        Glue3DRenderAll();\n"
     "        MainProccess();\n",
     "        MainTime();\n"
     "        // EDITED: the start menu (Custom/MainMenu.cpp) shows instead of the editor until a project is created or loaded.\n"
     "        if (MainMenuActive)\n"
     "        {\n"
     "            MainMenuFrame();\n"
     "            return;\n"
     "        }\n"
     "        Glue3dExecute();\n"
     "        MainControl();\n"
     "        Glue3DRenderAll();\n"
     "        if (BuildFlavor::IsEditor) MainProccess(); // EDITED: editor windows aren't part of a built game\n"
     "        if (BuildFlavor::IsEditor) BuildDialogFrame(); // EDITED: File > build.. dialog (Custom/BuildGame.cpp)\n"),
    ("void Glue3DSprite::ElementMainBar()",
     '        stage.elementData.Add(u"save project.."_s);\n'
     '        stage.elementData.Add(u"texture"_s);\n'
     '        stage.elementData.Add(u"import texture"_s);\n'
     '        stage.elementData.Add(u"filemove"_s);\n'
     '        stage.elementData.Add(u"import files"_s);\n',
     '        stage.elementData.Add(u"save project.."_s);\n'
     '        // EDITED: "import files" removed (projects are loaded from the start menu); "build.." added.\n'
     '        stage.elementData.Add(u"game"_s);\n'
     '        stage.elementData.Add(u"build.."_s);\n'
     '        stage.elementData.Add(u"texture"_s);\n'
     '        stage.elementData.Add(u"import texture"_s);\n'),
    ("void Glue3DSprite::MainbarFile()",
     '        Message(u"< press crtl + s >"_s, u"_icon.bsod"_s, u"Exclame_1"_s, u""_s);\n'
     '    }\n',
     '        // EDITED: really saves into the project\'s folder (was a "press ctrl + s" hint for Scratch). Ctrl+S works too.\n'
     '        SaveProject();\n'
     '    }\n'
     '    if (Eq(stage.menuSelection, u"build.."_s))\n'
     '    {\n'
     '        // EDITED: new - compiles the game into an .exe (Custom/BuildGame.cpp).\n'
     '        OpenBuildDialog();\n'
     '    }\n'),

    # ---- "export program" removed (File > build.. replaces it) ----
    ("void Glue3DSprite::ElementSideBarSelector()",
     re.compile(r'        Render2DOnDirSizeGbcInteractiveShadowHoverMessage\(u"_icon\.filemove"_s, stage\.guiStartx.*?u"export"_s\);\n'
                r'        if \(Eq\(stage\.guiClick, 1\)\)\n        \{\n.*?u"export program"_s\);\n.*?\n        \}\n', re.S),
     '        // EDITED: the "export" button was removed; games are made with File > build.. now.\n'),
    ("void Glue3DSprite::ElementConfigureTabs()",
     re.compile(r'    Render2DOnDirSizeGbcInteractiveShadowHoverMessage\(u"_gui\.balloon"_s, -90, -83, .*?'
                r'stage\.guiInfoName = u"export program"_s;\n.*?\n.*?\n    \}\n', re.S),
     '    // EDITED: the "export program" settings tab was removed; games are made with File > build.. now.\n'),
    ("void Glue3DSprite::ConfigurationTabs(Value tab)",
     re.compile(r'    if \(Eq\(tab, u"export program"_s\)\)\n    \{\n.*?ExportGeneral\(\);\n        \}\n    \}\n', re.S),
     '    // EDITED: "export program" tab removed (see File > build..).\n'),

    # ---- Unity-style camera (Custom/CameraControls.cpp) ----
    ("void Glue3DSprite::EngineFreeCamera(Value speed, Value turn)",
     '        CreateAddress(u"engine.camera_direction_x"_s, (Num(KeyPressed(u"right arrow"_s)) - Num(KeyPressed(u"left arrow"_s))) * turn.Num() + stage.ramEngineData.Item(118).Num(), 59);',
     '        // EDITED: hold the right mouse button and drag to turn (was: left/right arrow keys).\n'
     '        CreateAddress(u"engine.camera_direction_x"_s, MouseLookX() + stage.ramEngineData.Item(118).Num(), 59);'),
    ("void Glue3DSprite::EngineFreeCamera(Value speed, Value turn)",
     '    CreateAddress(u"engine.camera_direction_y"_s, (Num(KeyPressed(u"up arrow"_s)) - Num(KeyPressed(u"down arrow"_s))) * turn.Num() + stage.ramEngineData.Item(120).Num(), 60);',
     '    // EDITED: drag up/down with the right mouse button to look up/down (was: up/down arrow keys).\n'
     '    CreateAddress(u"engine.camera_direction_y"_s, MouseLookY() + stage.ramEngineData.Item(120).Num(), 60);'),
    ("void Glue3DSprite::EngineFreeCamera(Value speed, Value turn)",
     '    if (Gt(stage.ramEngineData.Item(120), 45))\n'
     '    {\n'
     '        CreateAddress(u"engine.camera_direction_y"_s, 45, 60);\n'
     '    }\n'
     '    if (Lt(stage.ramEngineData.Item(120), -45))\n'
     '    {\n'
     '        CreateAddress(u"engine.camera_direction_y"_s, -45, 60);\n'
     '    }\n',
     '    // EDITED: up/down limit is MaxCameraPitch (90, like Unity) instead of 45.\n'
     '    if (Gt(stage.ramEngineData.Item(120), MaxCameraPitch))\n'
     '    {\n'
     '        CreateAddress(u"engine.camera_direction_y"_s, MaxCameraPitch, 60);\n'
     '    }\n'
     '    if (Lt(stage.ramEngineData.Item(120), -MaxCameraPitch))\n'
     '    {\n'
     '        CreateAddress(u"engine.camera_direction_y"_s, -MaxCameraPitch, 60);\n'
     '    }\n'),
    ("void Glue3DSprite::EngineFreeCamera(Value speed, Value turn)",
     re.compile(r'(        CreateAddress\(u"engine\.camera_direction_z"_s, 0, 61\);\n    \}\n)\}\n$'),
     r'\1    // EDITED: mouse wheel zoom, middle-button pan, Q/E up/down while right-dragging (Custom/CameraControls.cpp).\n'
     r'    UnityStyleCameraExtras(speed);\n}\n'),
    # ---- game.movement: arrow keys up/down, and mouse controls (Custom/CameraControls.cpp) ----
    ("void Glue3DSprite::ProccessGame()",
     "            GameMoveplayer(stage.dataVal1, stage.dataVal2, stage.dataReturn);",
     "            // EDITED: 3rd value = controls (\"arrows\", \"mouse\", \"both\"), 4th = view (\"left/right\", \"up/down\", \"both\").\n"
     "            SetGameControls(stage.dataVal3, stage.dataVal4);\n"
     "            GameMoveplayer(stage.dataVal1, stage.dataVal2, stage.dataReturn);"),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     '(Num(KeyPressed(u"w"_s)) - Num(KeyPressed(u"s"_s)))',
     "MoveForwardAxis() /* EDITED: W/S or Up/Down */", 5),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     '(Num(KeyPressed(u"d"_s)) - Num(KeyPressed(u"a"_s)))',
     "MoveStrafeAxis() /* EDITED: A/D (and Left/Right with mouse controls) */", 2),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     '        EditDataInObjectDataOperation(stage.scriptObject, u"rx"_s, (Num(KeyPressed(u"right arrow"_s)) - Num(KeyPressed(u"left arrow"_s))) * ',
     '        // EDITED: Left/Right turn (unless the mouse turns), and the mouse turns with "mouse"/"both" controls.\n'
     '        EditDataInObjectDataOperation(stage.scriptObject, u"rx"_s, MouseTurn(turnSpeed), u"add"_s);\n'
     '        EditDataInObjectDataOperation(stage.scriptObject, u"rx"_s, TurnAxis() * '),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     '        EditDataInObjectDataOperation(stage.scriptObject, u"stand"_s, u"-"_s, u"read"_s);',
     '        GameLook(turnSpeed); // EDITED: look up/down with the arrows and/or mouse (4th value "up/down" or "both")\n'
     '        EditDataInObjectDataOperation(stage.scriptObject, u"stand"_s, u"-"_s, u"read"_s);'),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     '(Num(KeyPressed(u"d"_s)) - Num(KeyPressed(u"a"_s)) - Num(KeyPressed(u"left arrow"_s)))',
     "SteerAxis() /* EDITED: A/D or Left/Right steer (the original only had Left), plus the mouse */"),
    ("void Glue3DSprite::GameMoveplayer(Value speed, Value turnSpeed, Value type)",
     re.compile(r'(\n        EditDataInObjectDataOperation\(stage\.scriptObject, u"rx"_s, SteerAxis\(\)[^\n]*\n)(    \}\n\}\n)$'),
     r'\1        GameLook(turnSpeed); // EDITED: look up/down (4th value "up/down" or "both")\n\2'),
    # ---- the dragged item keeps its color (no hover glow, which flickered as the mouse moved) ----
    ("void Glue3DSprite::Render2DOnDirSizeGbcInteractiveShadowHoverMessage(Value name, Value x, Value y, Value dir, Value size, Value g, Value b, Value c, Value i, Value except1, Value except2, Value shadow, Value hoverMessage)",
     "SetEffect(Effect::Brightness, b.Num() + (stage.guiTemp1.Num() * Num(Lt(stage.guiTemp1, 35)) + (35 * Num(Gt(stage.guiTemp1, 35)))));",
     "SetEffect(Effect::Brightness, b.Num() + (drawingDragged_ ? 0.0 : (stage.guiTemp1.Num() * Num(Lt(stage.guiTemp1, 35)) + (35 * Num(Gt(stage.guiTemp1, 35)))))); // EDITED: no glow on a dragged item", 2),
    ("void Glue3DSprite::MainAfterControl()",
     "        Render2DOnDirSizeGbcInteractiveShadowHoverMessage(stage.dragIcon, MouseX(), MouseY(), 90, 75, 50, 0, 0, u\"\"_s, u\"\"_s, u\"\"_s, 1, u\"\"_s);",
     "        drawingDragged_ = true; // EDITED: the dragged item keeps its color (its hover glow flickered on and off)\n"
     "        Render2DOnDirSizeGbcInteractiveShadowHoverMessage(stage.dragIcon, MouseX(), MouseY(), 90, 75, 50, 0, 0, u\"\"_s, u\"\"_s, u\"\"_s, 1, u\"\"_s);\n"
     "        drawingDragged_ = false;"),
    # ---- mouse-wheel scrolling for every scrollbar (Custom/Scrolling.cpp) ----
    ("void Glue3DSprite::ScrollerElement()",
     re.compile(r'^(void Glue3DSprite::ScrollerElement\(\)\n\{\n)'),
     r'\1    // EDITED: the mouse wheel scrolls the scrollbar nearest the mouse (Custom/Scrolling.cpp).\n'
     r'    ScrollbarSeen(stage.elementData.Item(1), stage.elementData.Item(2), stage.elementData.Item(3), stage.elementData.Item(4), stage.elementData.Item(5), stage.elementData.Item(6));\n'),
    ("void Glue3DSprite::EngineScreenRender(Value fullScreen)",
     'stage.engineScreenMidy.Num() + (-1 * stage.ramEngineData.Item(120).Num()), 90, 100.5 * stage.engineSkyboxScale.Num()',
     'stage.engineScreenMidy.Num() + (-1 * std::clamp(stage.ramEngineData.Item(120).Num(), -45.0, 45.0)), 90, 100.5 * stage.engineSkyboxScale.Num()'),
    # ---- GluePP's extra script blocks (Custom/ScriptBlocks.cpp) ----
    ("void Glue3DSprite::RunLineMainObjectScript(Value clock, Value object, Value script)",
     '        DecodeIDSeperatorIncludeSeperatorSpecialdot(stage.scriptLine.Str() + u" "_s, u""_s, u" (),"_s, u""_s, u"."_s);\n',
     '        DecodeIDSeperatorIncludeSeperatorSpecialdot(stage.scriptLine.Str() + u" "_s, u""_s, u" (),"_s, u""_s, u"."_s);\n'
     '        if (CustomCommand()) return; // EDITED: GluePP\'s extra blocks (Custom/ScriptBlocks.cpp)\n'),
    ("void Glue3DSprite::Operation(Value type1, Value data1, Value op, Value type2, Value data2)",
     re.compile(r'\n(    if \(Contains\(u"<\\x0631>)'),
     r'\n    if (CustomOperation(op)) return; // EDITED: != >= <= contains (Custom/ScriptBlocks.cpp)\n\1'),
    # "else": a false if stops skipping at its else (and runs the else part)
    ("void Glue3DSprite::ProccessOther()",
     '                if (Eq(stage.decodedElements.Item(1), u"endif"_s))\n'
     '                {\n'
     '                    stage.scriptIfs = stage.scriptIfs.Num() + -1;\n'
     '                }\n',
     '                if (Eq(stage.decodedElements.Item(1), u"endif"_s))\n'
     '                {\n'
     '                    stage.scriptIfs = stage.scriptIfs.Num() + -1;\n'
     '                }\n'
     '                if (Eq(stage.decodedElements.Item(1), u"else"_s) && Eq(stage.scriptIfs, 1)) // EDITED: a false if runs its else part\n'
     '                {\n'
     '                    stage.scriptIfs = 0;\n'
     '                    stage.textIndex2 = stage.textIndex2.Num() + 1;\n'
     '                }\n'),
    # the block editor indents repeat/for/else blocks like if/while/forever
    ("void Glue3DSprite::BlockBasedEditor()",
     'Eq(stage.decodedElements.Item(1), u"endwhile "_s))))',
     'Eq(stage.decodedElements.Item(1), u"endwhile "_s) || BlockDedents(stage.decodedElements.Item(1))))) /* EDITED: endrepeat endfor else */'),
    ("void Glue3DSprite::BlockBasedEditor()",
     'Eq(stage.decodedElements.Item(1), u"if "_s)))',
     'Eq(stage.decodedElements.Item(1), u"if "_s) || BlockIndents(stage.decodedElements.Item(1)))) /* EDITED: repeat for else */'),
    # dropping a block that answers something also adds "var variable = data_return" under it (new blocks too)
    ("void Glue3DSprite::BlockBasedEditor()",
     'if (Eq(stage.decodedElements.Item(2), u"distance"_s) || (Eq(stage.decodedElements.Item(2), u"key_pressed"_s)',
     'if (BlockReturnsValue(stage.decodedElements.Item(1), stage.decodedElements.Item(2)) /* EDITED */ || Eq(stage.decodedElements.Item(2), u"distance"_s) || (Eq(stage.decodedElements.Item(2), u"key_pressed"_s)'),
    # dropping "func" adds its "endfunc"
    ("void Glue3DSprite::BlockBasedEditor()",
     '                if (Eq(stage.decodedElements.Item(1), u"for"_s))\n'
     '                {\n'
     '                    stage.loadedTextEditor.Insert(stage.textDraglocation.Num() + 2, u"endfor"_s);\n'
     '                }\n',
     '                if (Eq(stage.decodedElements.Item(1), u"for"_s))\n'
     '                {\n'
     '                    stage.loadedTextEditor.Insert(stage.textDraglocation.Num() + 2, u"endfor"_s);\n'
     '                }\n'
     '                if (Eq(stage.decodedElements.Item(1), u"func"_s)) // EDITED: func gets its endfunc\n'
     '                {\n'
     '                    stage.loadedTextEditor.Insert(stage.textDraglocation.Num() + 2, u"endfunc"_s);\n'
     '                }\n'),
    # reading or setting an object's variable stops at the end of that object: before, a variable the object
    # didn't have was looked for in the objects after it (and "var x = ..." could change another object's x)
    ("void Glue3DSprite::EditDataInObjectDataOperation(Value objectName, Value dataName, Value dataVal, Value operation)",
     '        stage.sceneObject = stage.sceneObject.Num() + 2;\n',
     '        stage.sceneObject = stage.sceneObject.Num() + 2;\n'
     '        if (Eq(stage.scene.Item(stage.sceneObject), objectName.Str() + u".end"_s)) break; // EDITED: only this object\'s variables\n'),
    # game.pause: paused objects' scripts don't run (Custom/ScriptSystems.cpp)
    ("void Glue3DSprite::Glue3dExecute()",
     'if (Eq(stage.scriptExit, 0) && (!(Eq(stage.scene.Item(stage.objectIndex.Num() + 29), u""_s) || Eq(stage.scene.Item(stage.objectIndex.Num() + 31), u"halt"_s))))',
     'if (Eq(stage.scriptExit, 0) && (!(Eq(stage.scene.Item(stage.objectIndex.Num() + 29), u""_s) || Eq(stage.scene.Item(stage.objectIndex.Num() + 31), u"halt"_s))) && !ScriptPaused()) /* EDITED: game.pause */'),
    # time.scale: slow motion / fast forward
    ("void Glue3DSprite::MainTime()",
     '    stage.timeFps = std::floor(Nz(1 / stage.timeDelta.Num()));\n',
     '    stage.timeFps = std::floor(Nz(1 / stage.timeDelta.Num()));\n'
     '    stage.timeDelta = stage.timeDelta.Num() * TimeScale(); // EDITED: time.scale\n'),
    # game.camera_shake moves the camera a little each frame
    ("void Glue3DSprite::MainControl()",
     re.compile(r'(stage\.ramEngineData\.Replace\(11([246]), stage\.scene\.Item\(stage\.objectIndex\.Num\(\) \+ 1[135]\))\);'),
     lambda m: m.group(1) + ".Num() + CameraShake(" + str((int(m.group(2)) - 2) // 2) + ")); // EDITED: game.camera_shake", 3),
    # UI elements: interface.color, font_size, text_brightness
    ("void Glue3DSprite::EngineRenderEngineSprites()",
     'stage.scene.Item(stage.engineObjectindex.Num() + 47), 0, 0, 1 * Num(Eq(stage.scene.Item(stage.engineObjectindex.Num() + 43), u"true"_s))',
     'stage.scene.Item(stage.engineObjectindex.Num() + 47), 0, UiStyle(u"ui-color", 0) /* EDITED: interface.color */, 1 * Num(Eq(stage.scene.Item(stage.engineObjectindex.Num() + 43), u"true"_s))'),
    ("void Glue3DSprite::EngineRenderEngineSprites()",
     '* (0.5 + (stage.engineFullscreen.Num() * 0.5)), 1, u""_s, 50 + stage.scene.Item(stage.engineObjectindex.Num() + 47).Num(), -100, 0,',
     '* (0.5 + (stage.engineFullscreen.Num() * 0.5)) * UiStyle(u"ui-font", 1), 1, u""_s, 50 + stage.scene.Item(stage.engineObjectindex.Num() + 47).Num(), UiStyle(u"ui-text-brightness", -100), 0, /* EDITED: font_size, text_brightness */'),
    # the sidebar scrollbar is as long as the block palette scrolls (a short category used to barely move)
    ("void Glue3DSprite::ElementSideBar()",
     '    stage.elementData.Add(-85 - stage.guiMainMargin.Num());\n    stage.elementData.Add(1);\n',
     '    stage.elementData.Add(SidebarScrollEnd(55 + stage.guiMainMargin.Num(), -85 - stage.guiMainMargin.Num())); // EDITED: as long as the block palette scrolls\n    stage.elementData.Add(1);\n'),
    # a mouse-wheel notch over the block palette moves at least one block
    ("void Glue3DSprite::EditorTexteditor()",
     'EditAddress(1, stage.ramEngineData.Item(2).Num() - (stage.guiScroll.Num() * 5));',
     'EditAddress(1, stage.ramEngineData.Item(2).Num() - (stage.guiScroll.Num() * PaletteWheelStep())); // EDITED: one block per notch'),
    # every terminal line goes through the terminal tools (Custom/ScriptTerminal.cpp)
    ("void Glue3DSprite::TerminalPrint(Value message, Value color)",
     re.compile(r'^(void Glue3DSprite::TerminalPrint\(Value message, Value color\)\n\{\n)'),
     r'\1    if (TerminalHook(message, color)) return; // EDITED: source, time, filters, log file (Custom/ScriptTerminal.cpp)\n'),
    # a long block name in the palette gets a little smaller instead of sticking out of its block
    ("void Glue3DSprite::BlockCategories()",
     re.compile(r'TextOnScaleCenterGbcCursorLimitScroll\((stage\.autocomplete\.Item\(\(stage\.textIndex\.Num\(\) - 1\) \* 3 \+ 2 \+ stage\.textCategoryStart\.Num\(\)\)), (165, [^\n]*?), 0\.45, 1,'),
     r'TextOnScaleCenterGbcCursorLimitScroll(\1, \2, PaletteLabelScale(\1) /* EDITED: fits its block */, 1,', 2),
    # every block's values can also be global variables and array items (Custom/ScriptVariables.cpp)
    ("void Glue3DSprite::DataRecall()",
     re.compile(r'\n\}\n$'),
     '\n    ApplyValueTypes(); // EDITED: global x, local x, var slot[2]\n}\n'),
    # the palette scrolls far enough to reach every block of a category
    ("void Glue3DSprite::BlockCategories()",
     "(7 * stage.ramEngineData.Item(2).Num())",
     "(PaletteScrollStep() * stage.ramEngineData.Item(2).Num()) /* EDITED: longer categories */", 7),
]

# A comment above the sky line (added separately so the long line above stays a plain replacement).
SKY_COMMENT = ("void Glue3DSprite::EngineScreenRender(Value fullScreen)",
               re.compile(r'(\n)(\s*)(Render2DOnDirSizeGbcInteractiveShadowHoverMessage\(u"_engine\.skybox_1"_s, .*?std::clamp)'),
               r'\1\2// EDITED: the sky image stops sliding past 45 degrees so its edge never shows when looking far up/down.\n\2\3')


def method_span(text, signature):
    start = text.find("\n" + signature + "\n")
    if start < 0:
        return None
    start += 1
    end = text.find("\n}\n", start)
    return start, end + 3


def apply(gen_dir):
    files = {p: open(p, encoding="utf-8").read() for p in glob.glob(os.path.join(gen_dir, "Glue3DSprite*.g.cpp"))}
    if any("// EDITED" in t for t in files.values()):
        print("already edited (found // EDITED); run sb3_to_cpp.py first to start fresh")
        return 1
    failed = 0
    for edit in EDITS + [SKY_COMMENT]:
        sig, old, new = edit[:3]
        expected = edit[3] if len(edit) > 3 else 1  # how many places it must change
        for path, text in files.items():
            span = method_span(text, sig)
            if span is None:
                continue
            a, b = span
            body = text[a:b]
            if isinstance(old, str):
                n = body.count(old)
                new_body = body.replace(old, new)
            else:
                new_body, n = old.subn(new, body)
            if n != expected:
                print(f"edit in {sig}: expected {expected} match(es), found {n}")
                failed += 1
            else:
                files[path] = text[:a] + new_body + text[b:]
            break
        else:
            print(f"method not found: {sig}")
            failed += 1
    if failed:
        print(f"{failed} edit(s) failed; nothing written")
        return 1
    for path, text in files.items():
        if "std::clamp" in text and "#include <algorithm>" not in text:
            text = text.replace("#include <cmath>\n", "#include <algorithm>\n#include <cmath>\n", 1)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"applied {len(EDITS) + 1} edits")
    if edit_lists(os.path.join(gen_dir, "..", "assets", "initial-lists.json")):
        return 1
    return mark_editor_only(gen_dir)


# ------------------------------------------------------------------------------------------------
# Glue3D's built-in texts (starting list contents in assets/initial-lists.json)
# ------------------------------------------------------------------------------------------------
LIST_EDITS = [
    # the script editor's autocomplete
    ("0/autocomplete", "movement(val speed, val turn_speed)", "movement(val speed, val turn_speed, str controls, str view)"),
    # the "movement" script you can import (import > +movement); "\u0621" separates values in these texts,
    # and a comma would end the template, so the comment has none
    ("0/preLoaded", "|forever |game.movement(val 0.1\u0621 val 1) |",
     '|//controls: "arrows" or "mouse" or "both" - view: "left/right" or "up/down" or "both"|'
     "forever |game.movement(val 0.1\u0621 val 1\u0621 str arrows\u0621 str left/right) |"),
]


# GluePP's extra blocks in the script editor's palette: (icon, label, template) added at the end of each
# category (before "<category>.end"). The blocks themselves are in Custom/ScriptBlocks.cpp.
LIST_EDITS.append(("0/autocomplete", "new_attribute(str object, val name, val value)", "new_attribute(str object, str name, val value)"))

PALETTE = {
    "object": [
        ("axis", "set position", "set_pos(val x, val y, val z)"),
        ("rotate", "set rotation", "set_rotation(val rx, val ry, val rz)"),
        ("stretch", "set scale", "set_scale(val sx, val sy, val sz)"),
        ("stretch", "change scale", "change_scale(val 0, val 0, val 0)"),
        ("axis", "get position", "get_pos(str x)"),
        ("rotate", "get rotation", "get_rotation(str x)"),
        ("stretch", "get scale", "get_scale(str x)"),
        ("rotate", "look at object", "look_at(str object)"),
        ("axis", "move forward", "move_forward(val distance)"),
        ("group", "distance to point", "distance_to_point(val x, val y, val z)"),
        ("ghostdiv", "hide", "hide()"),
        ("ghostdiv", "show", "show()"),
        ("ghostdiv", "visible?", "is_visible()"),
        ("object", "object exists?", "exists(str object)"),
        ("object", "my name", "my_name()"),
        ("dictionary", "set tag", "set_tag(str enemy)"),
        ("dictionary", "get tag", "get_tag()"),
        ("group", "nearest with tag", "nearest(str enemy)"),
        ("group", "count with tag", "count(str enemy)"),
        ("game", "event to tag", "broadcast_tag(str enemy, str message)"),
        ("object", "clone with value", "clone_with(str script, str var_name, val value)"),
        ("object", "parent object", "parent()"),
        ("object", "last clone", "last_clone()"),
    ],
    "variable": [
        ("stats", "divide variable", "var variable /= val value "),
        ("stats", "modulo variable", "var variable %= val value "),
        ("stats", "power variable", "var variable ^= val value "),
        ("graph", "set to a block's answer", "var variable = object.distance(str object) "),
        ("graph", "set global variable", "global score = val 0 "),
        ("stats", "change global variable", "global score += val 10 "),
        ("graph", "read global variable", "var s = global score "),
        ("stats", "math in one line", "var d = (var x - var px) ^ val 2 + val 1 "),
        ("stats", "add 1", "var i ++ "),
        ("stats", "subtract 1", "var i -- "),
        ("stats", "flip true / false", "var on toggle "),
        ("stats", "add text to the end", "var name .= str _bob "),
        ("stats", "keep in range", "var hp clamp (val 0, val 100) "),
        ("stats", "no more than", "var hp min= val 100 "),
        ("stats", "no less than", "var hp max= val 0 "),
        ("graph", "swap two variables", "var a swap var b "),
        ("graph", "set if missing", "var hp ?= val 100 "),
        ("graph", "local (in a function)", "local tmp = val 0 "),
        ("graph", "constant", "const gravity = val 0.9 "),
        ("file", "saved variable", "saved var highscore = val 0 "),
        ("dictionary", "array item", "var slot[1] = str sword "),
        ("dictionary", "read array item", "var w = var slot[var i] "),
        ("dictionary", "array length", "var.length(str slot)"),
        ("dictionary", "add to array", "var.push(str slot, str shield)"),
        ("dictionary", "take last from array", "var.pop(str slot)"),
        ("trash", "empty array", "var.clear_array(str slot)"),
        ("dictionary", "variable by name", "var.get(str name)"),
        ("dictionary", "set by name", "var.set(str name, val 5)"),
        ("dictionary", "variable exists?", "var.exists(str hp)"),
        ("trash", "delete variable", "var.delete(str hp)"),
        ("dictionary", "all variable names", "var.names()"),
        ("object", "copy variables from", "var.copy_from(str player)"),
        ("clock", "changed since asked?", "var.changed(str hp)"),
        ("clock", "value before change", "var.previous(str hp)"),
        ("point", "type of value", "var.type(var x)"),
        ("point", "is it a number?", "var.is_number(var x)"),
        ("stats", "round to decimals", "var.round_to(var x, val 2)"),
        ("bsod", "warn on typos", "var.strict(str true)"),
    ],
    "terminal": [
        ("command", "print data return", "print_return(val color)"),
        ("command", "print label + value", "print_value(str label, var variable, val color)"),
        ("command", "print warning", "warn(str message)"),
        ("stats", "watch value (one line)", "watch(str hp, var hp)"),
        ("trash", "stop watching", "unwatch(str hp)"),
        ("stats", "count (one line)", "count(str shots)"),
        ("command", "print once", "print_once(str landed)"),
        ("clock", "print every n seconds", "print_every(val 1, var speed)"),
        ("command", "print with values {}", "format(str hp={}_ammo={}, var hp, var ammo)"),
        ("command", "info", "info(str message)"),
        ("command", "success", "success(str message)"),
        ("bsod", "error", "error(str message)"),
        ("object", "show who printed", "show_source(str true)"),
        ("clock", "show time", "timestamps(str true)"),
        ("group", "trace (where am i)", "trace()"),
        ("ghostdiv", "only this level and up", "filter(str warn)"),
        ("ghostdiv", "mute object / tag", "mute(str enemy)"),
        ("ghostdiv", "unmute", "unmute(str enemy)"),
        ("ghostdiv", "only object / tag", "only(str player)"),
        ("ghostdiv", "show everything", "show_all()"),
        ("bsod", "assert", "assert(var hp >= val 0, str hp_went_negative)"),
        ("bsod", "assert and pause", "assert_pause(var hp >= val 0, str hp_went_negative)"),
        ("bsod", "breakpoint (F8 goes on)", "break()"),
        ("dictionary", "print my variables", "dump()"),
        ("dictionary", "print object's variables", "dump(str player)"),
        ("dictionary", "print a list", "print_list(str inventory)"),
        ("clock", "timer start", "time_start(str ai)"),
        ("clock", "timer end (prints ms)", "time_end(str ai)"),
        ("stats", "watch fps", "fps_watch()"),
        ("command", "console command?", "command(str god)"),
        ("command", "console command word", "command_arg(val 1)"),
        ("command", "ask in console", "input(str name?)"),
        ("ui", "terminal over the game", "overlay(str true)"),
        ("file", "write log.txt", "log_to_file(str true)"),
        ("trash", "keep last n lines", "max_lines(val 200)"),
    ],
    "other": [
        ("unknown", "else", "else"),
        ("unknown", "if not equal", "if (var variable != val value)"),
        ("unknown", "if more or equal", "if (var variable >= val value)"),
        ("unknown", "if less or equal", "if (var variable <= val value)"),
        ("unknown", "repeat n times", "repeat (val 10)"),
        ("unknown", "end repeat", "endrepeat"),
        ("unknown", "for i from a to b", "for (var i, val 1, val 10)"),
        ("unknown", "end for", "endfor"),
        ("unknown", "wait until", "wait_until (var variable = val value)"),
        ("unknown", "stop script", "stop"),
        ("unknown", "function", "func name"),
        ("unknown", "end function", "endfunc"),
        ("unknown", "call function", "call name (val 1)"),
        ("unknown", "return value", "return (val 0)"),
        ("game", "broadcast event", "event.broadcast(str message)"),
        ("game", "send event to", "event.send(str object, str message)"),
        ("game", "event received?", "event.received(str message)"),
        ("game", "wait for event", "event.wait_for(str message)"),
        ("trash", "forget my events", "event.clear()"),
        ("file", "save value", "save.set(str highscore, var score)"),
        ("file", "load value", "save.get(str highscore, val 0)"),
        ("file", "saved?", "save.has(str highscore)"),
        ("trash", "delete saved value", "save.delete(str highscore)"),
        ("trash", "delete all saves", "save.clear_all()"),
        ("unknown", "if this and that", "if (var a > val 0 and var b < val 10)"),
        ("unknown", "if this or that", "if (var a = val 0 or var b = val 0)"),
        ("tile", "show hitboxes", "debug.show_hitboxes(str true)"),
        ("point", "draw 3d line", "debug.draw_line(val 0, val 0, val 0, val 1, val 1, val 1, val 2)"),
        ("trash", "clear debug lines", "debug.clear()"),
        ("command", "log variable", "debug.log(var variable)"),
    ],
    "game": [
        ("game", "mouse down?", "mouse_down()"),
        ("game", "right mouse down?", "right_mouse_down()"),
        ("game", "any key pressed?", "any_key()"),
        ("game", "mouse motion", "mouse_motion(str x)"),
        ("game", "lock mouse", "lock_mouse(str true)"),
        ("game", "mouse locked?", "mouse_locked()"),
        ("scene", "get camera", "get_camera()"),
        ("bsod", "show message", "message(str hello_world, val 2)"),
        ("trash", "quit game", "quit()"),
        ("game", "key just pressed?", "key_down(str e)"),
        ("game", "key just released?", "key_up(str e)"),
        ("game", "mouse clicked?", "mouse_clicked()"),
        ("scene", "load level", "load_scene(str level2)"),
        ("scene", "restart level", "restart()"),
        ("scene", "current level", "current_scene()"),
        ("clock", "pause game", "pause(str true)"),
        ("clock", "paused?", "is_paused()"),
        ("scene", "camera shake", "camera_shake(val 3, val 0.5)"),
        ("game", "gamepad connected?", "gamepad_connected()"),
        ("game", "gamepad button held?", "gamepad_button(str a)"),
        ("game", "gamepad button pressed?", "gamepad_pressed(str a)"),
        ("game", "gamepad stick / trigger", "gamepad_axis(str left_x)"),
    ],
    "physics": [
        ("tile", "set velocity", "set_velocity(val 0, val 0, val 0)"),
        ("tile", "get velocity", "get_velocity(str x)"),
        ("tile", "stop moving", "stop()"),
        ("tile", "jump", "jump()"),
        ("tile", "set gravity", "set_gravity(val 0.9)"),
        ("tile", "set friction", "set_friction(val 0.85)"),
        ("tile", "set bounce", "set_bounce(val 0.1)"),
        ("tile", "set jump height", "set_jump(val 1.5)"),
        ("tile", "moving?", "is_moving()"),
        ("tile", "speed", "speed()"),
        ("group", "touching?", "touching(str object)"),
        ("group", "touching what", "touching_any()"),
        ("axis", "raycast", "raycast(val dir, val pitch, val distance)"),
    ],
    "math": [
        ("stats", "min a b", "min(val a, val b)"),
        ("stats", "max a b", "max(val a, val b)"),
        ("stats", "round a", "round(val a)"),
        ("stats", "ceil a", "ceil(val a)"),
        ("stats", "sign a", "sign(val a)"),
        ("stats", "snap a to step", "snap(val a, val step)"),
        ("stats", "lerp a b t", "lerp(val a, val b, val t)"),
        ("ghostdiv", "random whole number", "random_int(val a, val b)"),
        ("graph", "atan2 y x", "atan2(val y, val x)"),
        ("graph", "pi", "pi()"),
        ("graph", "e ^ a", "exp(val a)"),
        ("graph", "ln a", "ln(val a)"),
        ("group", "distance 2d", "distance_2d(val x1, val y1, val x2, val y2)"),
        ("group", "distance 3d", "distance_3d(val x1, val y1, val z1, val x2, val y2, val z2)"),
        ("group", "length of text", "length(str text)"),
        ("group", "text contains", "contains(str text, str part)"),
        ("group", "upper case", "upper(str text)"),
        ("group", "lower case", "lower(str text)"),
        ("point", "a != b", "not_equals(val a, val b)"),
        ("point", "a >= b", "greater_equal(val a, val b)"),
        ("point", "a <= b", "less_equal(val a, val b)"),
        ("group", "split text", "split(str a-b-c, str -, val 1)"),
        ("group", "part of text", "substring(str text, val 1, val 3)"),
        ("group", "replace in text", "replace(str text, str find, str with)"),
        ("stats", "text to number", "to_number(str 12)"),
        ("ghostdiv", "chance %", "chance(val 25)"),
    ],
    "model": [
        ("model", "get model", "get_model()"),
        ("model", "copy model of", "copy_model(str object)"),
        ("bone", "add keyframe", "keyframe(str walk, val 0, str bone, val 0, val 0, val 0)"),
        ("bone", "play animation", "play_animation(str walk, str loop, val 1)"),
        ("bone", "stop animation", "stop_animation()"),
        ("bone", "animating?", "is_animating()"),
        ("bone", "animated bone angle", "bone_angle(str bone, str x)"),
    ],
    "sound": [
        ("music", "stop all sounds", "stop_all()"),
        ("stats", "set volume", "volume(val 100)"),
        ("stats", "set pitch", "pitch(val 0)"),
        ("music", "play sound at object", "play_at(str sound_name, str object, val 30)"),
    ],
    "interface": [
        ("axis", "get position", "get_pos(str x)"),
        ("axis", "move by", "move_by(val x, val y)"),
        ("rotate", "turn", "turn(val degrees)"),
        ("stretch", "change scale", "change_scale(val amount)"),
        ("color", "hide", "hide()"),
        ("color", "show", "show()"),
        ("bsod", "get text", "get_text()"),
        ("ui", "mouse over?", "hovered(val 20)"),
        ("color", "color", "color(val 50)"),
        ("bsod", "text size", "font_size(val 1.5)"),
        ("bsod", "text brightness", "text_brightness(val 100)"),
        ("stats", "bar (health bar)", "bar(var hp, val 100)"),
        ("trash", "hide bar", "hide_bar()"),
    ],
    "time": [
        ("clock", "frames per second", "fps()"),
        ("clock", "date / time part", "date(str hour)"),
        ("clock", "seconds since 1970", "seconds()"),
        ("clock", "time scale", "scale(val 0.5)"),
        ("clock", "get time scale", "get_scale()"),
        ("clock", "cooldown ready?", "cooldown(str shoot, val 0.5)"),
    ],
    "list": [
        ("dictionary", "list contains?", "contains(str list, str item)"),
        ("dictionary", "index of item", "index_of(str list, str item)"),
        ("dictionary", "insert item at", "insert_item(str list, val 1, str item)"),
        ("dictionary", "random item", "random_item(str list)"),
        ("dictionary", "last item", "last_item(str list)"),
        ("trash", "remove last item", "remove_last(str list)"),
        ("dictionary", "join items", "join(str list, str _)"),
    ],
}
# Colors for the new keywords in the script editor (name, brightness, color), like Glue3D's own.
COLORS = [("global", "25", "5"), ("local", "25", "5"), ("const", "25", "5"), ("saved", "25", "5"),
          ("++", "-10", "75"), ("--", "-10", "75"), ("toggle", "-10", "75"), (".=", "-10", "75"), ("?=", "-10", "75"),
          ("min=", "-10", "75"), ("max=", "-10", "75"), ("clamp", "-10", "75"), ("swap", "-10", "75"), ("%", "-10", "75"),
          ("debug", "-25", "50"), ("save", "25", "120"), ("func", "10", "20"), ("endfunc", "10", "20"), ("call", "10", "20"), ("return", "10", "20"), ("event", "10", "75"),
          ("else", "10", "20"), ("stop", "10", "20"), ("!=", "-10", "75"), (">=", "-10", "75"), ("<=", "-10", "75"),
          ("/=", "-10", "75"), ("%=", "-10", "75"), ("^=", "-10", "75")]


def insert_palette(data):
    items = data["0/autocomplete"]
    for cat, blocks in PALETTE.items():
        end = items.index(cat + ".end")
        new = [x for b in blocks for x in b]
        items[end:end] = new
    colors = data["0/textColoration"]
    for c in COLORS:
        if c[0] not in colors[0::3]:
            colors.extend(c)
    return sum(len(b) for b in PALETTE.values())

def edit_lists(path):
    import json
    if not os.path.exists(path):
        print("no " + path)
        return 1
    data = json.load(open(path, encoding="utf-8"))
    for key, old, new in LIST_EDITS:
        items = data.get(key, [])
        hits = [i for i, x in enumerate(items) if isinstance(x, str) and old in x]
        if len(hits) != 1:
            print(f"list edit in {key}: expected 1 match, found {len(hits)}")
            return 1
        items[hits[0]] = items[hits[0]].replace(old, new)
    added = insert_palette(data)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
    print(f"applied {len(LIST_EDITS)} text edits to {os.path.basename(path)}, added {added} blocks to the palette")
    return 0


# ------------------------------------------------------------------------------------------------
# Editor-only methods
# ------------------------------------------------------------------------------------------------
DEF_RE = re.compile(r"^(?:void|Script) Glue3DSprite::(\w+)\(", re.M)
CALL_RE = re.compile(r"\b([A-Z_]\w*)\(")
# Hand-written files compiled into the game player (see CMakeLists.txt); their calls are starting points too.
PLAYER_FILES = ["../Custom/CameraControls.cpp", "../Custom/ScriptBlocks.cpp", "../Custom/ScriptSystems.cpp", "../Custom/ScriptTerminal.cpp", "../Custom/ScriptVariables.cpp", "../Custom/ProjectFiles.cpp", "../Custom/PlayerSettings.cpp",
                "../Player/PlayerMain.cpp", "../Player/PlayerStubs.cpp"]


def methods_in(text):
    """[(name, start offset of the definition line, end offset after its closing brace)]"""
    out = []
    for m in DEF_RE.finditer(text):
        end = text.find("\n}\n", m.start())
        out.append((m.group(1), m.start(), end + 3))
    return out


def player_view(body):
    """The method body without the branches a game never runs (`if (BuildFlavor::IsEditor ...)`)."""
    lines = body.split("\n")
    out, i = [], 0
    while i < len(lines):
        t = lines[i].strip()
        if t.startswith("if (BuildFlavor::IsEditor") or t.startswith("else if (BuildFlavor::IsEditor"):
            if "{" not in t and t.split("//")[0].rstrip().endswith(";"):
                i += 1  # one-line form: if (BuildFlavor::IsEditor) Foo();
                continue
            indent = len(lines[i]) - len(lines[i].lstrip())
            i += 1  # the "{" line, then everything up to the matching "}"
            while i < len(lines) and not (lines[i].strip() == "}" and len(lines[i]) - len(lines[i].lstrip()) == indent):
                i += 1
            i += 1
            continue
        out.append(lines[i])
        i += 1
    return "\n".join(out)


def mark_editor_only(gen_dir):
    paths = sorted(glob.glob(os.path.join(gen_dir, "Glue3DSprite*.g.cpp")))
    texts = {p: open(p, encoding="utf-8").read() for p in paths}
    bodies, outside = {}, []
    for p, text in texts.items():
        last = 0
        for name, a, b in methods_in(text):
            bodies[name] = player_view(text[a:b])
            outside.append(text[last:a])
            last = b
        outside.append(text[last:])
    names = set(bodies)
    roots = set()
    for chunk in outside:  # the Hats() table
        roots |= set(CALL_RE.findall(chunk)) & names
    for rel in PLAYER_FILES:
        path = os.path.normpath(os.path.join(gen_dir, rel))
        if os.path.exists(path):
            roots |= set(CALL_RE.findall(open(path, encoding="utf-8").read())) & names
    reached, todo = set(), list(roots)
    while todo:
        n = todo.pop()
        if n in reached:
            continue
        reached.add(n)
        todo.extend((set(CALL_RE.findall(bodies[n])) & names) - reached)
    editor_only = names - reached
    for p, text in texts.items():
        for name, a, b in reversed(methods_in(text)):
            if name in editor_only:
                # An empty stand-in keeps every build type linking (unoptimized builds still see the calls).
                head = text[a:text.find("\n", a)]
                stub = head + (" { co_return; }" if head.startswith("Script") else " {}")
                text = (text[:a] + "#ifndef GLUE3D_PLAYER // editor only: a built game never runs this (tools/glue3d_edits.py)\n"
                        + text[a:b] + "#else\n" + stub + "\n#endif\n" + text[b:])
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"{len(editor_only)} of {len(names)} methods are editor-only (left out of the game player)")
    return 0


if __name__ == "__main__":
    sys.exit(apply(sys.argv[1] if len(sys.argv) > 1 else "Generated"))
