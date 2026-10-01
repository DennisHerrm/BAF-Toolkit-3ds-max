-- ============================================================
--  BafImport.mcr - the import window
--  Version 1.8.3
--
--  IMPORT ONLY. Exporting has a window of its own
--  (BafExport.mcr) and a good reason for it: it has to cope with
--  a scene that this Max session never imported.
--
--  Layout:
--    left    what is loaded  - archive, search, model list, clips
--    right   what to do with it - numbers, build, animation
--    bottom  a status line
--
--  Options and tools sit in collapsed rollouts on the right. The
--  cross-checks are DEVELOPER tools and have no business on the
--  same level as "Build model".
--
--  ------------------------------------------------------------
--  WHY THE FILE IS LAID OUT LIKE THIS
--  ------------------------------------------------------------
--  MAXScript has no columns. Two blocks side by side are only
--  possible with subRollout - or with absolute positions, and
--  those do not scale with the Windows display scaling.
--
--  That creates a problem: the helper functions have to reach
--  controls in BOTH rollouts, and MAXScript resolves names
--  strictly from top to bottom. A function placed above a
--  rollout cannot see its controls - that is exactly what 1.1.0
--  fell over:
--      Unknown property: "text" in undefined
--
--  Hence the usual remedy:
--    1. DECLARE the function names at the top as "local"
--    2. write the rollouts - they may use those names
--    3. DEFINE the functions afterwards with "fn", once every
--       rollout is known
--
--  This is the way the MAXScript documentation itself describes
--  ("Visibility of Locals, Functions, Structures and
--  User-Interface Items in Rollout Code"): a variable declared
--  as local is undefined at first but counts as KNOWN -
--  MAXScript walks past it and fills in the value once the
--  definition arrives.
--
--  Defining one of these functions further up instead of merely
--  declaring it brings the bug straight back.
--
--  ------------------------------------------------------------
--  LAYOUT RULES
--  ------------------------------------------------------------
--  * across:N splits into N EQUALLY WIDE cells. At a width of
--    280 that is 140 per cell, and a field of width:176 reaches
--    into the second cell - the button next to it then sits ON
--    TOP of it. That was visible in 1.2.0.
--    So: either both controls below the cell width, or put them
--    underneath each other instead of side by side.
--  * height counts TEXT LINES on a listBox, not pixels.
--  * No pos:[x,y].
--  * Comments with --, never //.
--  * ASCII only.
--  * MAXScript has no "continue".
-- ============================================================

macroScript BafImport_Open
  category:"DH Tools"
  buttonText:"BAF Import"
  toolTip:"BAF Import - Phantom Menace models and animations"
(
  -- Has to match the version in baftoolkit.h. Catches the most
  -- common mishap: scripts installed, plugin not rebuilt.
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
    -- ----------------------------------------------------
    --  Shared state
    -- ----------------------------------------------------
    local aAllNames = #()      -- every model in the archive
    local aModelNames = #()    -- what the list currently shows

    -- Read by roBuild, written by roOptions. NOT
    -- "roOptions.chkLayers.checked" from inside roBuild: that
    -- rollout is declared further down, and MAXScript resolves
    -- names strictly from top to bottom - it would be undefined
    -- there. Same rule that 1.1.0 fell over, and the linter
    -- caught it this time.
    local bSortLayers = true

    -- ----------------------------------------------------
    --  DECLARE the function names, do not define them.
    --  See the note at the top of this file.
    -- ----------------------------------------------------
    local SetStatus, ReportError, ShowLimits, ShowModel
    local FillClips, FillSlots, ApplyFilter, LoadArchive, LoadModel
    local BuildSkeletonNow, BuildMeshesNow, BuildMaterialsNow
    local WriteNoteTrack, VerifyPair, SortIntoLayers, LayerFor

    -- ====================================================
    --  LEFT COLUMN - what is loaded
    -- ====================================================
    rollout roSource "Source" width:280
    (
      group "Archive"
      (
        edittext edtArchive "" width:252 readOnly:true
        button btnBrowse "Pick archive..." width:252
      )

      label lblGap1 "" height:2

      group "Model"
      (
        edittext edtFilter "" width:252
        button btnClearFilter "Clear filter" width:252
        listbox lbxModels "" height:10
        label lblCount "no archive open" align:#left
      )

      label lblGap2 "" height:2

      group "Clips"
      (
        listbox lbxClips "" height:8
      )

      on btnBrowse pressed do
      (
        local sPath = getOpenFileName caption:"Pick big.lab" \
          types:"LAB archive (*.lab)|*.lab|All files (*.*)|*.*|"
        if (sPath != undefined) do
        (
          LoadArchive sPath
        ) -- end of cancel check
      ) -- end of btnBrowse

      on edtFilter changed sText do
      (
        ApplyFilter()
      ) -- end of edtFilter

      on btnClearFilter pressed do
      (
        edtFilter.text = ""
        ApplyFilter()
      ) -- end of btnClearFilter

      on lbxModels selected iIndex do
      (
        if (iIndex >= 1 and iIndex <= aModelNames.count) do
        (
          LoadModel aModelNames[iIndex]
        ) -- end of range check
      ) -- end of lbxModels
    ) -- end of roSource

    -- ====================================================
    --  RIGHT COLUMN - what to do with it
    -- ====================================================
    rollout roBuild "Build" width:280
    (
      group "Model"
      (
        label lblInfo1 "no model open" align:#left
        label lblInfo2 "" align:#left
        label lblInfo3 "" align:#left
        label lblLimits "" align:#left
      )

      label lblGap3 "" height:2

      group "Into the scene"
      (
        spinner spnScale "Scale:" range:[0.001, 1000.0, 80.0] type:#float \
          scale:1.0 fieldWidth:42 across:2
        spinner spnBone "Bones:" range:[0.0, 100.0, 0.0] type:#float \
          scale:0.1 fieldWidth:42 align:#right
        edittext edtTexDir "" width:252 readOnly:true text:"(automatic)"
        button btnTexDir "Pick texture folder..." width:252
        label lblGap4 "" height:2
        button btnAll "Build model" width:252 height:28
        button btnSkeleton "skeleton only" width:124 across:2
        button btnMeshes "meshes only" width:124 align:#right
        button btnMaterials "materials only" width:124 across:2
        button btnClear "Clear the scene" width:124 align:#right
        button btnLayers "Sort into layers" width:252
      )

      label lblGap5 "" height:2

      group "Animation"
      (
        button btnAnim "Apply clip" width:124 across:2
        button btnSequence "All as a sequence" width:124 align:#right
        spinner spnSpacing "Gap:" range:[0, 200, 5] type:#integer \
          fieldWidth:42 across:2
        checkbox chkBake "bake every frame" align:#right checked:true
      )

      on btnTexDir pressed do
      (
        local sDir = getSavePath caption:"Folder for the textures"
        if (sDir != undefined) do
        (
          edtTexDir.text = BafCpp.setTextureDir sDir
        ) -- end of cancel check
      ) -- end of btnTexDir

      on btnAll pressed do
      (
        if (BafCpp.isOpen() != 1) then
        (
          SetStatus "Pick a model first."
        )
        else
        (
          local iBones = BuildSkeletonNow()
          if (iBones > 0) do
          (
            local iMeshes = BuildMeshesNow()
            -- Always export: without a chosen folder the plugin
            -- picks one itself. Skipping here left the model grey
            -- with nothing saying why.
            local iTex = BafCpp.exportTextures()
            edtTexDir.text = BafCpp.setTextureDir ""
            local iSlots = BuildMaterialsNow()
            local sLayers = ""
            if bSortLayers do
            (
              sLayers = ", " + (SortIntoLayers()) as string + " sorted"
            ) -- end of layer check
            SetStatus (iBones as string + " bones, " + iMeshes as string \
              + " meshes, " + iSlots as string + " materials, " \
              + iTex as string + " textures" + sLayers)
            max zoomext sel all
          ) -- end of skeleton check
        ) -- end of open check
      ) -- end of btnAll

      on btnSkeleton pressed do
      (
        if (BafCpp.isOpen() != 1) then
        (
          SetStatus "Pick a model first."
        )
        else
        (
          local iBones = BuildSkeletonNow()
          if (iBones > 0) do
          (
            SetStatus (iBones as string + " bones created")
            max zoomext sel all
          ) -- end of build check
        ) -- end of open check
      ) -- end of btnSkeleton

      on btnMeshes pressed do
      (
        if (BafCpp.sceneBoneCount() <= 0) then
        (
          SetStatus "Build a skeleton first."
        )
        else
        (
          local iMeshes = BuildMeshesNow()
          if (iMeshes > 0) do
          (
            SetStatus (iMeshes as string + " meshes created")
            max zoomext sel all
          ) -- end of build check
        ) -- end of skeleton check
      ) -- end of btnMeshes

      on btnMaterials pressed do
      (
        if (BafCpp.isOpen() != 1) then
        (
          SetStatus "Pick a model first."
        )
        else
        (
          -- Write the TGA files first, then the materials:
          -- otherwise the bitmap points at a file that does not
          -- exist yet, and Max remembers the failure.
          local iTex = BafCpp.exportTextures()
          edtTexDir.text = BafCpp.setTextureDir ""
          local iSlots = BuildMaterialsNow()
          if (iSlots > 0) do
          (
            SetStatus (iSlots as string + " material slots, " \
              + iTex as string + " textures")
          ) -- end of build check
        ) -- end of open check
      ) -- end of btnMaterials

      on btnLayers pressed do
      (
        local iMoved = SortIntoLayers()
        if (iMoved <= 0) then
        (
          SetStatus "Nothing to sort - no BAF objects in the scene."
        )
        else
        (
          SetStatus (iMoved as string + " objects sorted into layers")
        ) -- end of moved check
      ) -- end of btnLayers

      on btnClear pressed do
      (
        local iGone = BafCpp.clearScene()
        SetStatus (iGone as string + " objects removed.")
      ) -- end of btnClear

      on chkBake changed bState do
      (
        BafCpp.setBake (if bState then 1 else 0)
      ) -- end of chkBake

      on btnAnim pressed do
      (
        local iClip = roSource.lbxClips.selection - 1
        if (BafCpp.sceneBoneCount() <= 0) then
        (
          SetStatus "Build a skeleton first."
        )
        else if (iClip < 0) then
        (
          SetStatus "Pick a clip from the list first."
        )
        else
        (
          local iKeys = BafCpp.buildAnim iClip
          if (iKeys <= 0) then
          (
            ReportError "Apply clip"
          )
          else
          (
            SetStatus (iKeys as string + " keys, " \
              + (BafCpp.clipSceneFrames iClip) as string + " frames")
          ) -- end of build check
        ) -- end of checks
      ) -- end of btnAnim

      on btnSequence pressed do
      (
        if (BafCpp.sceneBoneCount() <= 0) then
        (
          SetStatus "Build a skeleton first."
        )
        else
        (
          local iTotal = BafCpp.buildSequence spnSpacing.value
          if (iTotal <= 0) then
          (
            ReportError "Build sequence"
          )
          else
          (
            WriteNoteTrack()
            SetStatus (BafCpp.clipCount() as string + " clips, " \
              + iTotal as string + " frames")
          ) -- end of build check
        ) -- end of skeleton check
      ) -- end of btnSequence
    ) -- end of roBuild

    -- ====================================================
    --  Options - collapsed
    -- ====================================================
    rollout roOptions "Options" width:280
    (
      checkbox chkFaceFront "Turn to face the front view" checked:true
      checkbox chkNormals "Use the file's vertex normals" checked:true
      checkbox chkHide "Hide meshes the engine never draws" checked:true
      checkbox chkProps "Store the raw data" checked:true
      checkbox chkBounds "Show bounding spheres" checked:false
      checkbox chkLayers "Sort into layers after building" checked:true

      -- The engine looks along +Y, and so does Max' front view -
      -- without the turn you see the character from behind. The
      -- turn is taken back out on export, so the file stays
      -- unchanged.
      on chkFaceFront changed bState do
      (
        BafCpp.setFaceFront (if bState then 1 else 0)
        SetStatus "Takes effect the next time you build."
      ) -- end of chkFaceFront

      on chkNormals changed bState do
      (
        BafCpp.setImportNormals (if bState then 1 else 0)
      ) -- end of chkNormals

      on chkHide changed bState do
      (
        BafCpp.setHideUndrawn (if bState then 1 else 0)
      ) -- end of chkHide

      on chkProps changed bState do
      (
        BafCpp.setStoreProps (if bState then 1 else 0)
        if (not bState) do
        (
          SetStatus "Without the raw data a later export is not possible."
        ) -- end of warning
      ) -- end of chkProps

      on chkBounds changed bState do
      (
        BafCpp.setShowBounds (if bState then 1 else 0)
      ) -- end of chkBounds

      on chkLayers changed bState do
      (
        bSortLayers = bState
      ) -- end of chkLayers
    ) -- end of roOptions

    -- ====================================================
    --  Tools - collapsed
    --
    --  Developer tools. Every cross-check writes two files: one
    --  from the file data, one from the scene that was built.
    --  If they match, what stands in Max is what stands in the
    --  file - regardless of whether it looks right in the
    --  viewport.
    -- ====================================================
    rollout roTools "Tools" width:280
    (
      button btnVerify "Cross-check the rest pose..." width:252
      button btnVerifyMesh "Cross-check the geometry..." width:252
      button btnVerifyAnim "Cross-check the animation..." width:252
      button btnDump "Write a full dump..." width:252
      label lblSep2 "" height:4
      button btnLog "Save the log..." width:124 across:2
      checkbox chkDebug "verbose" align:#right

      on btnVerify pressed do
      (
        VerifyPair "rest" "_expected.txt"
      ) -- end of btnVerify

      on btnVerifyMesh pressed do
      (
        VerifyPair "geo" "_geo_expected.txt"
      ) -- end of btnVerifyMesh

      on btnVerifyAnim pressed do
      (
        VerifyPair "anim" "_anim_expected.txt"
      ) -- end of btnVerifyAnim

      on btnDump pressed do
      (
        if (BafCpp.isOpen() != 1) then
        (
          SetStatus "Pick a model first."
        )
        else
        (
          local sTarget = getSaveFileName caption:"Save the dump" \
            filename:(BafCpp.modelName() + ".txt") \
            types:"Text file (*.txt)|*.txt|"
          if (sTarget != undefined) do
          (
            -- 63 = all six sections
            if ((BafCpp.dump sTarget 63) == 1) then
            (
              SetStatus "Dump written."
              format "[BAF] compare with:  fc /L \"%\" dump_py\\%.txt\n" \
                sTarget (BafCpp.modelName())
            )
            else
            (
              ReportError "Dump"
            ) -- end of dump check
          ) -- end of cancel check
        ) -- end of open check
      ) -- end of btnDump

      on chkDebug changed bState do
      (
        BafCpp.setDebug (if bState then 1 else 0)
      ) -- end of chkDebug

      on btnLog pressed do
      (
        local sTarget = getSaveFileName caption:"Save the log" \
          filename:"baf_toolkit_log.txt" \
          types:"Text file (*.txt)|*.txt|"
        if (sTarget != undefined) do
        (
          if ((BafCpp.saveLog sTarget) == 1) then
          (
            SetStatus ("Log written: " + sTarget)
          )
          else
          (
            ReportError "Save log"
          ) -- end of save check
        ) -- end of cancel check
      ) -- end of btnLog
    ) -- end of roTools

    -- ====================================================
    --  The window: two columns and a status line
    -- ====================================================
    rollout roBafImport ("BAF Import " + sPluginVersion) width:592
    (
      subRollout srLeft width:288 height:430 across:2
      subRollout srRight width:288 height:430
      label lblGap7 "" height:2
      label lblStatus "No archive open." align:#left width:576

      on roBafImport open do
      (
        addSubRollout srLeft roSource
        addSubRollout srRight roBuild
        addSubRollout srRight roOptions rolledUp:true
        addSubRollout srRight roTools rolledUp:true

        -- If this came from Max' import dialog, the path of the
        -- chosen file is waiting for us. A .lab opens as an
        -- archive, a .baf as a loose file.
        local sPending = BafCpp.pendingPath()
        if (sPending != "" and (doesFileExist sPending)) do
        (
          if ((toLower (getFilenameType sPending)) == ".lab") then
          (
            LoadArchive sPending
          )
          else
          (
            local iNodes = BafCpp.openFile sPending
            if (iNodes < 0) then
            (
              ReportError "Open file"
            )
            else
            (
              ShowModel()
              ShowLimits()
              SetStatus (filenameFromPath sPending)
            ) -- end of open check
          ) -- end of extension check
        ) -- end of pending check
      ) -- end of open

      -- Closing releases NOTHING.
      --
      -- This used to call close() and closeArchive(). That was
      -- wrong: afterwards the plugin knew no model any more, and
      -- File > Export failed although the scene stood there
      -- complete. Closing the window is exactly what one does
      -- BECAUSE one wants to work in the scene.
      on roBafImport close do
      (
        OK
      ) -- end of close
    ) -- end of roBafImport

    -- ====================================================
    --  The functions - only here, where every rollout is known
    -- ====================================================

    fn SetStatus sText =
    (
      roBafImport.lblStatus.text = sText
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
        roBuild.lblLimits.text = "Engine limits are fine"
      )
      else
      (
        roBuild.lblLimits.text = ("CAREFUL: " + iOver as string \
          + " limit(s) exceeded")
        format "[BAF] limits:\n%\n" (BafCpp.limitsReport())
      ) -- end of check
    ) -- end of ShowLimits

    -- Clip fps range from 2 to 160 across the data, while a Max
    -- scene has exactly one frame rate. The converted length is
    -- therefore shown right in the list - you want to see it
    -- BEFORE you build, not afterwards.
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
          local fFps = BafCpp.clipFps iIdx
          aLines[i] = ((BafCpp.clipName iIdx) + "  " \
            + (fFps as integer) as string + " fps  " \
            + (BafCpp.clipFrames iIdx) as string + " -> " \
            + (BafCpp.clipSceneFrames iIdx) as string)
        ) -- end of clip loop
      ) -- end of count check
      roSource.lbxClips.items = aLines
    ) -- end of FillClips

    fn FillSlots =
    (
      local iCount = BafCpp.slotCount()
      if (iCount > 0) do
      (
        format "[BAF] % material slots:\n" iCount
        for i = 0 to (iCount - 1) do
        (
          format "  %\n" (BafCpp.slotInfo i)
        ) -- end of slot loop
      ) -- end of count check
    ) -- end of FillSlots

    fn ShowModel =
    (
      roBuild.lblInfo1.text = ((BafCpp.nodeCount()) as string + " nodes    " \
        + (BafCpp.meshCount()) as string + " meshes")
      roBuild.lblInfo2.text = ((BafCpp.vertexCount()) as string + " vertices    " \
        + (BafCpp.faceCount()) as string + " faces")
      roBuild.lblInfo3.text = ((BafCpp.materialCount()) as string + " materials    " \
        + (BafCpp.clipCount()) as string + " clips")
      FillClips()
      local sWarn = BafCpp.warnings()
      if (sWarn != "") do
      (
        format "[BAF] notes:\n%\n" sWarn
      ) -- end of warning check
    ) -- end of ShowModel

    -- Boil the list down to the search word. Without it you
    -- scroll through 303 entries.
    fn ApplyFilter =
    (
      local sNeedle = toLower (trimLeft (trimRight roSource.edtFilter.text))
      if (sNeedle == "") then
      (
        aModelNames = aAllNames
      )
      else
      (
        local aHits = #()
        for sName in aAllNames do
        (
          if ((findString (toLower sName) sNeedle) != undefined) do
          (
            append aHits sName
          ) -- end of match check
        ) -- end of name loop
        aModelNames = aHits
      ) -- end of empty check
      roSource.lbxModels.items = aModelNames
      roSource.lblCount.text = (aModelNames.count as string + " of " \
        + aAllNames.count as string + " models")
    ) -- end of ApplyFilter

    fn LoadArchive sPath =
    (
      local iModels = BafCpp.openArchive sPath
      if (iModels < 0) then
      (
        ReportError "Open archive"
        roSource.lbxModels.items = #()
        aAllNames = #()
        aModelNames = #()
      )
      else
      (
        aAllNames = filterString (BafCpp.modelNames()) "\n"
        roSource.edtArchive.text = filenameFromPath sPath
        ApplyFilter()
        SetStatus (iModels as string + " models in " \
          + BafCpp.entryCount() as string + " entries")
      ) -- end of open check
    ) -- end of LoadArchive

    fn LoadModel sName =
    (
      local iNodes = BafCpp.openModel sName
      if (iNodes < 0) then
      (
        ReportError "Open model"
      )
      else
      (
        ShowModel()
        ShowLimits()
        SetStatus (sName + "  (" + BafCpp.timings() + ")")
      ) -- end of open check
    ) -- end of LoadModel

    fn BuildSkeletonNow =
    (
      BafCpp.setScale roBuild.spnScale.value
      BafCpp.setBoneSize roBuild.spnBone.value
      local iBones = BafCpp.buildSkeleton()
      if (iBones <= 0) then
      (
        ReportError "Build skeleton"
      )
      else
      (
        local sWarn = BafCpp.warnings()
        if (sWarn != "") do
        (
          format "[BAF] %\n" sWarn
        ) -- end of warning check
      ) -- end of build check
      iBones
    ) -- end of BuildSkeletonNow

    fn BuildMeshesNow =
    (
      local iMeshes = BafCpp.buildMeshes()
      if (iMeshes <= 0) then
      (
        ReportError "Build meshes"
      )
      else
      (
        local sWarn = BafCpp.warnings()
        if (sWarn != "") do
        (
          format "[BAF] %\n" sWarn
        ) -- end of warning check
      ) -- end of build check
      iMeshes
    ) -- end of BuildMeshesNow

    fn BuildMaterialsNow =
    (
      local iSlots = BafCpp.buildMaterials()
      if (iSlots <= 0) then
      (
        ReportError "Build materials"
      )
      else
      (
        FillSlots()
        local sWarn = BafCpp.warnings()
        if (sWarn != "") do
        (
          format "[BAF] %\n" sWarn
        ) -- end of warning check
      ) -- end of build check
      iSlots
    ) -- end of BuildMaterialsNow

    -- The note track is called "animations" and hangs off
    -- rootNode.
    --
    -- rootNode is the MAXScript constant for the ROOT NODE OF THE
    -- SCENE, not the root bone of the figure. Mixing those two up
    -- cost XFBIN Import a whole version.
    --
    -- addNewNoteKey, NOT "noteKey ..." plus appendItem: neither
    -- of those is a valid call, and MAXScript silently turns both
    -- names into empty locals. The error then reads
    --     Type error: Call needs function or class, got: undefined
    fn WriteNoteTrack =
    (
      while ((numNoteTracks rootNode) != 0) do
      (
        deleteNoteTrack rootNode (getNoteTrack rootNode 1)
      ) -- end of cleanup loop

      local NT = notetrack "animations"
      addNoteTrack rootNode NT

      -- Two keys per clip, at its start and at its end. The same
      -- shape AnimMerge writes - whoever reads one reads the
      -- other.
      for i = 0 to (BafCpp.clipCount() - 1) do
      (
        local iStart = BafCpp.clipStart i
        local iEnd = BafCpp.clipEnd i
        if (iStart >= 0) do
        (
          local sName = BafCpp.clipName i
          local k0 = addNewNoteKey NT.keys iStart
          k0.value = sName
          if (iEnd > iStart) do
          (
            local k1 = addNewNoteKey NT.keys iEnd
            k1.value = sName
          ) -- end of end check
        ) -- end of start check
      ) -- end of clip loop
    ) -- end of WriteNoteTrack

    -- ----------------------------------------------------
    --  Which layer does this object belong to?
    --
    --  The decision runs on the NODE NAME, and the weapon names
    --  are the same across the whole data set: gun01..gun24 and
    --  sabre/sabreblad/sabreglow hang off a node called "weapon"
    --  under the right hand. 194 of the models carry a "gun"
    --  name.
    --
    --  Weapons win over bones and meshes. A gun bone therefore
    --  lands in "Guns", not in "Bones" - that is the point of
    --  the whole thing: getting the weapon out of the way as one
    --  piece.
    -- ----------------------------------------------------
    fn LayerFor sName bIsBone =
    (
      local sLow = toLower sName
      local sGroup = undefined

      if ((matchPattern sLow pattern:"sabre*") \
        or (matchPattern sLow pattern:"saber*")) then
      (
        sGroup = "Sabers"
      )
      else if ((matchPattern sLow pattern:"gun*") \
        or (matchPattern sLow pattern:"pistol*") \
        or (matchPattern sLow pattern:"shot*") \
        or (matchPattern sLow pattern:"weapon*")) then
      (
        sGroup = "Guns"
      )
      else if bIsBone then
      (
        sGroup = "Bones"
      )
      else
      (
        sGroup = "Meshes"
      ) -- end of name check
      sGroup
    ) -- end of LayerFor

    -- ----------------------------------------------------
    --  Sort the BAF objects of the scene into layers.
    --
    --  Only objects carrying the raw data from the import are
    --  touched. Whatever else stands in the scene is none of our
    --  business - it might be a set, a camera, or work in
    --  progress.
    --
    --  Layer names are prefixed with the model name, so two
    --  imported models do not end up in the same layers.
    -- ----------------------------------------------------
    fn SortIntoLayers =
    (
      local sModel = BafCpp.modelName()
      if (sModel == "") do
      (
        sModel = "BAF"
      ) -- end of empty check
      -- Die Endung stoert im Layernamen.
      local iDot = findString sModel "."
      if (iDot != undefined) do
      (
        sModel = substring sModel 1 (iDot - 1)
      ) -- end of dot check

      local iMoved = 0
      for obj in objects do
      (
        local sProps = getUserPropBuffer obj
        if (sProps != undefined and sProps != "") do
        (
          local bIsBone = (findString sProps "bafNodeIndex" ) != undefined
          local bIsMesh = (findString sProps "bafMeshName" ) != undefined
          if (bIsBone or bIsMesh) do
          (
            local sLayer = sModel + " " + (LayerFor obj.name bIsBone)
            local L = LayerManager.getLayerFromName sLayer
            if (L == undefined) do
            (
              L = LayerManager.newLayerFromName sLayer
            ) -- end of create check
            if (L != undefined) do
            (
              L.addNode obj
              iMoved = iMoved + 1
            ) -- end of layer check
          ) -- end of baf check
        ) -- end of props check
      ) -- end of object loop

      if (iMoved > 0) do
      (
        format "[BAF] % objects sorted into layers under \"%\"\n" iMoved sModel
      ) -- end of report check
      iMoved
    ) -- end of SortIntoLayers

    -- Both files in one go, so they cannot accidentally belong to
    -- different models.
    fn VerifyPair sKind sSuffix =
    (
      if (BafCpp.isOpen() != 1) then
      (
        SetStatus "Pick a model first."
      )
      else
      (
        local sTarget = getSaveFileName caption:"Save the cross-check" \
          filename:(BafCpp.modelName() + sSuffix) \
          types:"Text file (*.txt)|*.txt|"
        if (sTarget != undefined) do
        (
          local sActual = (getFilenamePath sTarget) \
            + (getFilenameFile sTarget) + "_actual" + (getFilenameType sTarget)
          local bOk = false

          if (sKind == "rest") then
          (
            bOk = (BafCpp.restDump sTarget) == 1
            if bOk do
            (
              bOk = (BafCpp.sceneBoneDump sActual) == 1
            ) -- end of first check
          )
          else if (sKind == "geo") then
          (
            bOk = (BafCpp.buildDump sTarget) == 1
            if bOk do
            (
              bOk = (BafCpp.sceneMeshDump sActual) == 1
            ) -- end of first check
          )
          else
          (
            local iClip = roSource.lbxClips.selection - 1
            if (iClip >= 0) do
            (
              bOk = (BafCpp.animDump iClip sTarget) == 1
              if bOk do
              (
                bOk = (BafCpp.sceneAnimDump iClip sActual) == 1
              ) -- end of first check
            ) -- end of clip check
          ) -- end of kind check

          if bOk then
          (
            SetStatus "Two files written - now compare them."
            format "[BAF] compare:  fc /L \"%\" \"%\"\n" sTarget sActual
          )
          else
          (
            ReportError "Cross-check"
          ) -- end of write check
        ) -- end of cancel check
      ) -- end of open check
    ) -- end of VerifyPair

    -- Note in the listener which .dlu is actually loaded. Building
    -- and installing can easily go half-done, and then one hunts
    -- for a bug in code that is not even running.
    format "[BAF] BAF Import - plugin %, script %\n" (BafCpp.version()) sExpectedVersion

    createDialog roBafImport width:592
  ) -- end of version check
) -- end of macroScript
