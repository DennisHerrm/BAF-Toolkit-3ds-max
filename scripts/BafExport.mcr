-- ============================================================
--  BafExport.mcr - the export window
--  Version 1.8.3
--
--  ------------------------------------------------------------
--  WHY THIS IS A WINDOW OF ITS OWN
--  ------------------------------------------------------------
--  Exporting must NOT depend on this same Max session having
--  imported the model first. Save a scene, close Max, come back
--  the next day, and there is no model in memory any more - but
--  the scene is still there, complete.
--
--  So this window does not start with a file dialog. It starts
--  with a FINDING: what is in this scene, and where did it come
--  from? The question of where to write comes after that.
--
--  ------------------------------------------------------------
--  THE WORD "TEMPLATE"
--  ------------------------------------------------------------
--  The writer does not build a new file. It PATCHES the changes
--  back into the original bytes. That is the only way every
--  opaque byte survives - stale pointers, compiler padding from
--  1999, the rubbish behind a NUL in a name field. And it is the
--  only reason the guarantee holds: with nothing changed, the
--  file comes out byte for byte identical.
--
--  Without a template there is therefore nothing to write into.
--  Three cases:
--
--    1. model still loaded in the plugin  -> nothing to do
--    2. scene came from an import         -> the template is
--       (even after a restart)               fetched by itself
--    3. your own model                    -> pick a template;
--                                            header, palette and
--                                            materials come from
--                                            there, skeleton and
--                                            geometry from the
--                                            scene
--
--  ------------------------------------------------------------
--  LAYOUT RULES - see BafImport.mcr, they apply here as well.
--  In short: across:N splits into N EQUALLY WIDE cells, height
--  counts text lines on a listBox, no pos:[x,y], comments with
--  --, ASCII only, no "continue", controls BEFORE the functions.
-- ============================================================

macroScript BafExport_Open
  category:"DH Tools"
  buttonText:"BAF Export"
  toolTip:"BAF Export - write the scene as a Phantom Menace model"
(
  local sExpectedVersion = "1.8.3"

  fn GetPluginVersion =
  (
    local sResult = undefined
    try
    (
      sResult = BafCpp.version()
    )
    catch
    (
      sResult = undefined
    ) -- end of try/catch
    sResult
  ) -- end of GetPluginVersion

  local sPluginVersion = GetPluginVersion()

  if (sPluginVersion == undefined) then
  (
    messageBox "BafToolkit.dlu is not loaded.\n\nRun BUILD_ALL.bat and INSTALL.bat, then restart 3ds Max." \
      title:"BAF Toolkit"
  )
  else if (sPluginVersion != sExpectedVersion) then
  (
    messageBox ("Script and plugin do not match.\n\nThe script expects " \
      + sExpectedVersion + ", the plugin reports " + sPluginVersion \
      + ".\n\nMost likely the scripts were installed but the plugin was not rebuilt. Run BUILD_ALL.bat, then INSTALL.bat.") \
      title:"BAF Toolkit"
  )
  else
  (
    local aTemplateNames = #()

    -- Declare the function names here, see the note in
    -- BafImport.mcr.
    local SetStatus, ReportError, ScanNow, ShowTemplate, EnsureTemplate
    local FillTemplateList, ShowLimits, ApplyNow, FillClips, FillEntry

    rollout roExport ("BAF Export " + sPluginVersion) width:400
    (
      group "Scene"
      (
        label lblScene1 "not scanned yet" align:#left
        label lblScene2 "" align:#left
        label lblScene3 "" align:#left
        button btnScan "Scan the scene again" width:372
      )

      group "Template"
      (
        radiobuttons rdoSource labels:#("detected from the scene", "pick one myself") \
          default:1 columns:1
        edittext edtArchive "" width:300 across:2 readOnly:true
        button btnArchive "..." width:60 align:#right
        edittext edtFilter "" width:372
        listbox lbxTemplate "" height:6
        label lblTemplate "no template loaded" align:#left
      )

      group "What gets taken over"
      (
        checkbox chkAnimOnly "Animation only - leave skeleton and meshes alone" \
          checked:false
        checkbox chkForceRest "Overwrite the rest pose from frame 0" checked:false
        label lblRest "" align:#left
        checkbox chkRebuild "allow topology rebuild" checked:false across:2
        checkbox chkBounds "recompute bounding spheres" checked:false align:#right
        spinner spnScale "Scale:" range:[0.001, 1000.0, 80.0] type:#float \
          scale:1.0 fieldWidth:50 across:2
        label lblScaleHint "same as on import" align:#left
        button btnApply "Take over the scene" width:372
        label lblApply "" align:#left
      )

      group "Animation"
      (
        listbox lbxClips "" height:6
        spinner spnTol "Key tolerance (deg):" range:[0.0, 10.0, 0.5] type:#float \
          scale:0.1 fieldWidth:48 across:2
        label lblTolHint "0 = one key per frame" align:#left
        button btnCapture "Capture motion into this clip" width:372
        label lblCapture "" align:#left
        label lblSepA "" height:2
        edittext edtClipName "Name:" width:372
        spinner spnClipFps "fps:" range:[1, 200, 30] type:#integer \
          fieldWidth:46 across:2
        spinner spnClipFrames "Frames:" range:[1, 10000, 30] type:#integer \
          fieldWidth:52 align:#right
        button btnAddClip "Create a new clip" width:182 across:2
        button btnDelClip "Delete clip" width:182 align:#right
      )

      group "Check"
      (
        button btnRoundTrip "Verify round trip" width:372
        label lblLimits "" align:#left
      )

      group "Destination"
      (
        edittext edtTarget "" width:372 readOnly:true
        button btnFile "Write as .baf..." width:372 height:26
        button btnArchiveWrite "Back into the archive (with backup)" width:372
        button btnRestore "Restore the archive from its backup" width:372
        label lblSepB "" height:2
        edittext edtEntry "Entry:" width:372
        button btnPutFile "Put a ready .baf into the archive..." width:372
      )

      label lblStatus "Ready." align:#left width:380

      on roExport open do
      (
        -- If this came from Max' export dialog, a destination
        -- path is waiting for us.
        local sPending = BafCpp.pendingExportPath()
        if (sPending != "") do
        (
          edtTarget.text = sPending
        ) -- end of pending check

        -- Suggest the length of the scene's animation range:
        -- whoever just animated something usually wants exactly
        -- that.
        spnClipFrames.value = (animationRange.end.frame as integer) + 1
        spnClipFps.value = frameRate as integer
        ScanNow()
      ) -- end of open

      on btnScan pressed do
      (
        ScanNow()
      ) -- end of btnScan

      on rdoSource changed iState do
      (
        if (iState == 1) then
        (
          EnsureTemplate()
        )
        else
        (
          SetStatus "Pick an archive, then a model from the list."
        ) -- end of state check
      ) -- end of rdoSource

      on btnArchive pressed do
      (
        local sPath = getOpenFileName caption:"Pick the archive holding the template" \
          types:"LAB archive (*.lab)|*.lab|All files (*.*)|*.*|"
        if (sPath != undefined) do
        (
          local iModels = BafCpp.openArchive sPath
          if (iModels < 0) then
          (
            ReportError "Open archive"
          )
          else
          (
            edtArchive.text = sPath
            aTemplateNames = filterString (BafCpp.modelNames()) "\n"
            FillTemplateList()
            rdoSource.state = 2
            SetStatus (iModels as string + " models - now pick the template")
          ) -- end of open check
        ) -- end of cancel check
      ) -- end of btnArchive

      on edtFilter changed sText do
      (
        FillTemplateList()
      ) -- end of edtFilter

      on lbxTemplate selected iIndex do
      (
        local aShown = lbxTemplate.items
        if (iIndex >= 1 and iIndex <= aShown.count) do
        (
          if ((BafCpp.openModel aShown[iIndex]) < 0) then
          (
            ReportError "Load template"
          )
          else
          (
            ShowTemplate()
            ShowLimits()
            SetStatus ("Template: " + aShown[iIndex])
          ) -- end of open check
        ) -- end of range check
      ) -- end of lbxTemplate

      -- Der Notausgang zur Sperre. applyScene laesst die Ruhelage
      -- stehen, sobald die Bones Keys tragen - Frame 0 ist dann
      -- der erste Frame der Bewegung, nicht die Ruhelage. Wer
      -- wirklich eine neue Ruhelage setzen will, schaltet hier
      -- frei.
      on chkForceRest changed bState do
      (
        BafCpp.setForceRestPose (if bState then 1 else 0)
        if bState do
        (
          SetStatus ("CAREFUL: the rest pose will be taken from frame 0. " \
            + "If the scene is posed there, every animation breaks.")
        ) -- end of warning
      ) -- end of chkForceRest

      on chkRebuild changed bState do
      (
        BafCpp.setRebuild (if bState then 1 else 0)
      ) -- end of chkRebuild

      -- Every one of the 3,131 shipped spheres is larger than the
      -- tightly fitting one, by 16 percent on average. Recomputing
      -- therefore shrinks all of them - and on the head node that
      -- freezes the AI's look-at direction.
      on chkBounds changed bState do
      (
        BafCpp.setRecomputeBounds (if bState then 1 else 0)
        if bState do
        (
          SetStatus ("CAREFUL: recomputed spheres are tighter than the shipped ones.")
        ) -- end of warning
      ) -- end of chkBounds

      -- Create a new clip and fill it with the motion from the
      -- scene right away. Doing only the first half would be a
      -- step without a purpose.
      on btnAddClip pressed do
      (
        if (EnsureTemplate() != true) then
        (
          SetStatus "Load a template first."
        )
        else if ((trimLeft (trimRight edtClipName.text)) == "") then
        (
          SetStatus "The clip needs a name."
        )
        else
        (
          local sName = trimLeft (trimRight edtClipName.text)
          local iNew = BafCpp.addClip sName (spnClipFps.value as float) spnClipFrames.value
          if (iNew < 0) then
          (
            ReportError "Create clip"
          )
          else
          (
            FillClips()
            lbxClips.selection = iNew + 1
            BafCpp.setScale spnScale.value
            BafCpp.setKeyTolerance spnTol.value
            local iKeys = BafCpp.captureAnim iNew
            if (iKeys < 0) then
            (
              ReportError "Capture motion"
            )
            else
            (
              lblCapture.text = (iKeys as string + " keys from the scene")
              SetStatus ("Clip " + iNew as string + " '" + sName + "' created and filled")
            ) -- end of capture check
          ) -- end of add check
        ) -- end of checks
      ) -- end of btnAddClip

      on btnDelClip pressed do
      (
        local iClip = lbxClips.selection - 1
        if (iClip < 0) then
        (
          SetStatus "Pick a clip from the list first."
        )
        else if ((queryBox ("Really delete clip " + iClip as string + "?\n\n" \
          + "Every clip after it gets a new number.") \
          title:"BAF Toolkit") == true) then
        (
          if ((BafCpp.deleteClip iClip) == 1) then
          (
            FillClips()
            SetStatus "Clip deleted."
          )
          else
          (
            ReportError "Delete clip"
          ) -- end of delete check
        ) -- end of query
      ) -- end of btnDelClip

      on btnCapture pressed do
      (
        local iClip = lbxClips.selection - 1
        if (EnsureTemplate() != true) then
        (
          SetStatus "Load a template first."
        )
        else if (iClip < 0) then
        (
          SetStatus "Pick a clip from the list first."
        )
        else
        (
          BafCpp.setScale spnScale.value
          BafCpp.setKeyTolerance spnTol.value
          local iKeys = BafCpp.captureAnim iClip
          if (iKeys < 0) then
          (
            ReportError "Capture motion"
          )
          else
          (
            lblCapture.text = (iKeys as string + " keys from the scene")
            SetStatus (BafCpp.exportReport())
          ) -- end of capture check
        ) -- end of checks
      ) -- end of btnCapture

      on btnApply pressed do
      (
        ApplyNow()
      ) -- end of btnApply

      -- The sharpest check there is for a writer: with nothing
      -- changed the file must be byte for byte identical to the
      -- original. If it differs, the reported offset says which
      -- block drifts apart.
      --
      -- After adding a clip it WILL differ, and rightly so - the
      -- file really is a different one then.
      on btnRoundTrip pressed do
      (
        if (EnsureTemplate() != true) then
        (
          SetStatus "Load a template first."
        )
        else if ((BafCpp.verifyRoundTrip()) == 1) then
        (
          SetStatus "Round trip: byte for byte identical to the original."
        )
        else
        (
          SetStatus ("Round trip: " + BafCpp.lastError())
          format "[BAF] round trip: %\n" (BafCpp.lastError())
        ) -- end of check
      ) -- end of btnRoundTrip

      on btnFile pressed do
      (
        if (EnsureTemplate() != true) then
        (
          SetStatus "Load a template first."
        )
        else
        (
          ApplyNow()
          -- If Max' export dialog already named a destination,
          -- take it. Otherwise ask.
          local sTarget = undefined
          if (edtTarget.text != "") then
          (
            sTarget = edtTarget.text
          )
          else
          (
            sTarget = getSaveFileName caption:"Write as .baf" \
              filename:(BafCpp.modelName()) \
              types:"Big Ape Actor (*.baf)|*.baf|"
          ) -- end of pending check
          if (sTarget != undefined) do
          (
            if ((BafCpp.exportFile sTarget) == 1) then
            (
              SetStatus ("Written: " + sTarget)
            )
            else
            (
              ReportError "Write file"
            ) -- end of write check
          ) -- end of cancel check
        ) -- end of template check
      ) -- end of btnFile

      -- The first time round a .bak appears next to the archive;
      -- after that it is left alone, so the untouched state stays
      -- reachable.
      on btnArchiveWrite pressed do
      (
        if (EnsureTemplate() != true) then
        (
          SetStatus "Load a template first."
        )
        else if ((queryBox ("The model will be REPLACED IN THE ARCHIVE.\n\n" \
          + "The first time round a backup (.bak) appears next to it.\n" \
          + "After writing, every entry is verified.\n\n" \
          + "Continue?") title:"BAF Toolkit") == true) then
        (
          ApplyNow()
          if ((BafCpp.exportToArchive()) == 1) then
          (
            SetStatus (BafCpp.exportReport())
            format "[BAF] %\n" (BafCpp.exportReport())
          )
          else
          (
            ReportError "Write into archive"
          ) -- end of write check
        )
        else
        (
          SetStatus "Cancelled."
        ) -- end of query
      ) -- end of btnArchiveWrite

      -- Eine fertige Datei ohne Umweg ins Archiv legen. Zum
      -- Eingrenzen: eine unveraenderte Datei hineinlegen und im
      -- Spiel sehen, ob es am Schreibweg liegt oder am Inhalt.
      on btnPutFile pressed do
      (
        local sFile = getOpenFileName caption:"Which .baf goes into the archive?" \
          types:"Big Ape Actor (*.baf)|*.baf|All files (*.*)|*.*|"
        if (sFile != undefined) do
        (
          -- ------------------------------------------------
          --  Der Eintragsname steht im Feld darueber, nicht im
          --  Dateinamen.
          --
          --  Bis 1.8.2 fiel er still auf den Dateinamen zurueck,
          --  sobald kein Modell geladen war. Der kann aber nie
          --  stimmen - "TEST_C_deine_datei.baf" steht in keinem
          --  Archiv. Eine stille Voreinstellung, die nie richtig
          --  sein kann, ist schlimmer als gar keine.
          -- ------------------------------------------------
          local sEntry = trimLeft (trimRight edtEntry.text)
          if (sEntry == "") then
          (
            SetStatus "Fill in the entry name first - which entry gets replaced?"
          )
          else if ((queryBox ("Put\n  " + (filenameFromPath sFile) \
            + "\ninto the archive as\n  " + sEntry \
            + "\n\nA backup (.bak) is made the first time round.\n\nContinue?") \
            title:"BAF Toolkit") == true) then
          (
            if ((BafCpp.putFile sEntry sFile) == 1) then
            (
              SetStatus (BafCpp.exportReport())
              format "[BAF] %\n" (BafCpp.exportReport())
            )
            else
            (
              ReportError "Put file into archive"
            ) -- end of put check
          )
          else
          (
            SetStatus "Cancelled."
          ) -- end of query
        ) -- end of cancel check
      ) -- end of btnPutFile

      on btnRestore pressed do
      (
        if ((queryBox ("The archive will be restored from its backup (.bak)." \
          + "\n\nEverything written since then is lost.\n\nContinue?") \
          title:"BAF Toolkit") == true) do
        (
          if ((BafCpp.restoreArchive()) == 1) then
          (
            SetStatus (BafCpp.exportReport())
          )
          else
          (
            ReportError "Restore"
          ) -- end of restore check
        ) -- end of query
      ) -- end of btnRestore
    ) -- end of roExport

    -- ====================================================
    --  The functions - only here, where the rollout is known
    -- ====================================================

    fn SetStatus sText =
    (
      roExport.lblStatus.text = sText
    ) -- end of SetStatus

    fn ReportError sWhere =
    (
      local sError = BafCpp.lastError()
      if (sError == "") do
      (
        sError = "unknown error"
      ) -- end of empty check
      SetStatus (sWhere + ": " + sError)
      format "[BAF] %: %\n" sWhere sError
    ) -- end of ReportError

    fn ShowLimits =
    (
      local iOver = BafCpp.limitsExceeded()
      if (iOver == 0) then
      (
        roExport.lblLimits.text = "Engine limits are fine"
      )
      else
      (
        roExport.lblLimits.text = ("CAREFUL: " + iOver as string \
          + " engine limit(s) exceeded - the game will reject the model")
        format "[BAF] limits:\n%\n" (BafCpp.limitsReport())
      ) -- end of check
    ) -- end of ShowLimits

    -- Clip fps range from 2 to 160 across the data, while a Max
    -- scene has exactly one frame rate. The converted length is
    -- therefore shown right next to it - capturing samples over
    -- exactly that length.
    -- Die Clipliste fuellen und den Clip vorwaehlen, der gerade
    -- in der Szene steht. buildAnim hat ihn beim Import an den
    -- ersten Bone geschrieben; ohne diese Vorauswahl muss man ihn
    -- unter 114 Eintraegen selbst wiederfinden, und ein Griff
    -- daneben ueberschreibt den falschen.
    fn FillClips =
    (
      local aLines = #()
      local iCount = BafCpp.clipCount()
      if (iCount > 0) do
      (
        aLines[iCount] = ""
        for i = 1 to iCount do
        (
          local iIdx = i - 1
          aLines[i] = ((BafCpp.clipName iIdx) + "  " \
            + ((BafCpp.clipFps iIdx) as integer) as string + " fps  " \
            + (BafCpp.clipFrames iIdx) as string + " -> " \
            + (BafCpp.clipSceneFrames iIdx) as string)
        ) -- end of clip loop
      ) -- end of count check
      roExport.lbxClips.items = aLines

      local sInScene = BafCpp.sceneClip()
      if (sInScene != "") do
      (
        for i = 1 to iCount do
        (
          if ((BafCpp.clipName (i - 1)) == sInScene) do
          (
            roExport.lbxClips.selection = i
            roExport.lblCapture.text = ("this clip is in the scene: " + sInScene)
          ) -- end of match check
        ) -- end of clip loop
      ) -- end of scene clip check
    ) -- end of FillClips

    -- Den Eintragsnamen vorbelegen: erst das geladene Modell,
    -- sonst die Herkunft aus der Szene. Beides sind echte Namen
    -- aus dem Archiv, kein Dateiname von der Platte.
    fn FillEntry =
    (
      if (roExport.edtEntry.text == "") do
      (
        local sName = BafCpp.modelName()
        if (sName == "") do
        (
          sName = BafCpp.sceneOrigin()
        ) -- end of fallback
        roExport.edtEntry.text = sName
      ) -- end of empty check
    ) -- end of FillEntry

    fn ShowTemplate =
    (
      FillEntry()
      if ((BafCpp.isOpen()) != 1) then
      (
        roExport.lblTemplate.text = "no template loaded"
      )
      else
      (
        roExport.lblTemplate.text = (BafCpp.modelName() + ":  " \
          + (BafCpp.nodeCount()) as string + " nodes, " \
          + (BafCpp.meshCount()) as string + " meshes, " \
          + (BafCpp.materialCount()) as string + " materials")
        FillClips()
      ) -- end of open check
    ) -- end of ShowTemplate

    fn FillTemplateList =
    (
      local sNeedle = toLower (trimLeft (trimRight roExport.edtFilter.text))
      local aHits = #()
      for sName in aTemplateNames do
      (
        if (sNeedle == "" or (findString (toLower sName) sNeedle) != undefined) do
        (
          append aHits sName
        ) -- end of match check
      ) -- end of name loop
      roExport.lbxTemplate.items = aHits
    ) -- end of FillTemplateList

    -- Scan the scene and show what was found.
    fn ScanNow =
    (
      local iFound = BafCpp.scanScene()
      local sOrigin = BafCpp.sceneOrigin()

      local aLines = filterString (BafCpp.sceneReport()) "\n"
      roExport.lblScene1.text = if (aLines.count >= 1) then aLines[1] else ""
      roExport.lblScene2.text = if (aLines.count >= 2) then aLines[2] else ""
      roExport.lblScene3.text = if (aLines.count >= 3) then aLines[3] else ""

      if (iFound <= 0) then
      (
        roExport.rdoSource.state = 2
        SetStatus "No BAF data in the scene - please pick a template."
      )
      else if (sOrigin == "") then
      (
        roExport.rdoSource.state = 2
        SetStatus "Origin unknown - please pick a template."
      )
      else
      (
        roExport.rdoSource.state = 1
        EnsureTemplate()
      ) -- end of found check
      ShowTemplate()

      -- Sagen, was mit der Ruhelage passieren wird. Das ist die
      -- Frage, an der der erste Export im Spiel gescheitert ist.
      local iAnim = BafCpp.sceneIsAnimated()
      if (iAnim > 0) then
      (
        roExport.lblRest.text = (iAnim as string \
          + " bones carry keys - rest pose stays untouched")
      )
      else
      (
        roExport.lblRest.text = "no keys on the bones - frame 0 is the rest pose"
      ) -- end of animated check
    ) -- end of ScanNow

    -- Makes sure a template is loaded. Returns true if one is
    -- there afterwards.
    fn EnsureTemplate =
    (
      local bOk = (BafCpp.isOpen()) == 1
      if (not bOk and roExport.rdoSource.state == 1) do
      (
        if ((BafCpp.autoLoadTemplate()) == 1) then
        (
          bOk = true
          SetStatus ("Template fetched by itself: " + BafCpp.modelName())
        )
        else
        (
          SetStatus (BafCpp.lastError())
          roExport.rdoSource.state = 2
        ) -- end of auto check
      ) -- end of need check
      ShowTemplate()
      if bOk do
      (
        ShowLimits()
      ) -- end of ok check
      bOk
    ) -- end of EnsureTemplate

    -- Take the scene over into the loaded model.
    --
    -- CAREFUL, and this is why the switch above exists:
    -- applyScene reads the REST POSE from FRAME 0. If the scene
    -- sits in a posed frame there, that pose becomes the new
    -- rest pose of the model - and every clip is then built on
    -- top of it.
    --
    -- When only the animation changed, there is nothing here
    -- worth doing anyway: skeleton and meshes are unchanged, and
    -- writing them back only adds rounding noise.
    fn ApplyNow =
    (
      if (roExport.chkAnimOnly.checked) then
      (
        -- Die Vorlage wird trotzdem gebraucht: ohne sie gibt es
        -- nichts, worin die Keys landen koennten.
        EnsureTemplate()
        roExport.lblApply.text = "skeleton and meshes left alone"
      )
      else if (EnsureTemplate() != true) then
      (
        SetStatus "Load a template first."
      )
      else
      (
        BafCpp.setScale roExport.spnScale.value
        local iTaken = BafCpp.applyScene()
        if (iTaken < 0) then
        (
          ReportError "Take over the scene"
        )
        else
        (
          local aLines = filterString (BafCpp.exportReport()) "\n"
          roExport.lblApply.text = if (aLines.count >= 1) then aLines[1] else ""
          ShowLimits()
          SetStatus (BafCpp.exportReport())
          format "[BAF] %\n" (BafCpp.exportReport())
        ) -- end of apply check
      ) -- end of template check
    ) -- end of ApplyNow

    -- Note in the listener which .dlu is actually loaded. Building
    -- and installing can easily go half-done, and then one hunts
    -- for a bug in code that is not even running.
    format "[BAF] BAF Export - plugin %, script %\n" (BafCpp.version()) sExpectedVersion

    createDialog roExport width:400
  ) -- end of version check
) -- end of macroScript
