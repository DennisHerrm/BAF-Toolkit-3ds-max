-- ============================================================
--  BAF_TEST.ms - Stufe 9 im Listener durchspielen
--
--  Legt nichts in der Szene an. Prueft nur, dass das Plugin
--  geladen ist, alle API-Funktionen antworten und das Modell im
--  MAX-PROZESS dasselbe ergibt wie ausserhalb.
--
--  Benutzung: Pfade unten anpassen, dann die Datei in Max per
--  Drag&Drop oder ueber Scripting > Run Script ausfuehren.
--
--  Der geschriebene Dump laesst sich danach mit
--    fc /L max_dump.txt dump_py\anakin.baf.txt
--  gegen die Python-Referenz halten. Das ist die eigentliche
--  Gegenprobe fuer Stufe 1: der Parser laeuft in Max genauso wie
--  in bafdump.exe.
-- ============================================================

-- ---- anpassen ----
global BAF_LAB   = @"D:\Episode1\big.lab"
global BAF_MODEL = "anakin.baf"
global BAF_DUMP  = @"D:\tmp\max_anakin.txt"
-- ------------------

(
    local erwarteteVersion = "1.8.3"

    format "\n=== BAF Import Stufe 9 ===\n"

    -- Ist das Plugin ueberhaupt geladen? "BafCpp" ist erst da,
    -- wenn die .dlu geladen wurde - ein nicht neu gebautes Plugin
    -- ist die haeufigste Ursache fuer "Unknown property".
    if (isKindOf BafCpp Interface) != true then
    (
        format "FEHLER: BafCpp ist nicht da. Ist BafImport.dlu installiert\n"
        format "        und Max danach neu gestartet worden?\n"
    )
    else
    (
        local version = BafCpp.version()
        format "Plugin-Version: %\n" version
        if version != erwarteteVersion do
        (
            format "WARNUNG: Skript erwartet %, Plugin meldet %.\n" \
                erwarteteVersion version
            format "         Vermutlich installiert, aber nicht neu gebaut.\n"
        )

        BafCpp.setDebug 1

        -- ---- Archiv ----
        if not (doesFileExist BAF_LAB) then
            format "FEHLER: % gibt es nicht. Pfad oben anpassen.\n" BAF_LAB
        else
        (
            local modelle = BafCpp.openArchive BAF_LAB
            if modelle < 0 then
                format "FEHLER: %\n" (BafCpp.lastError())
            else
            (
                format "Archiv: % Eintraege, % Modelle (%)\n" \
                    (BafCpp.entryCount()) modelle (BafCpp.timings())

                -- Erste fuenf Modellnamen zeigen
                local namen = filterString (BafCpp.modelNames()) "\n"
                format "Erste Modelle: "
                for i = 1 to (amin 5 namen.count) do format "% " namen[i]
                format "...\n"

                -- ---- Modell ----
                local knoten = BafCpp.openModel BAF_MODEL
                if knoten < 0 then
                    format "FEHLER: %\n" (BafCpp.lastError())
                else
                (
                    format "\n%\n" (BafCpp.summary())
                    format "  Zeiten:      %\n" (BafCpp.timings())

                    local hinweise = BafCpp.warnings()
                    if hinweise != "" do format "Hinweise:\n%\n" hinweise

                    -- ---- Knoten ----
                    format "\nKnoten (erste 8 von %):\n" (BafCpp.nodeCount())
                    for i = 0 to (amin 7 (BafCpp.nodeCount() - 1)) do
                        format "  % % parent=% mesh=%\n" i \
                            (BafCpp.nodeName i) (BafCpp.nodeParent i) (BafCpp.nodeMesh i)

                    -- ---- Clips ----
                    -- Die FPS je Clip sind der Grund, warum die Zeit
                    -- spaeter umgerechnet werden muss: eine Szene hat
                    -- eine Framerate, ein Modell hat viele.
                    format "\nClips (% Stueck), Szene laeuft mit % fps:\n" \
                        (BafCpp.clipCount()) (frameRate as integer)
                    for i = 0 to (BafCpp.clipCount() - 1) do
                    (
                        local fps = BafCpp.clipFps i
                        local frames = BafCpp.clipFrames i
                        local inSzene = if fps > 0 then \
                            (frames * frameRate / fps) else 0
                        format "  % % % fps, % Frames -> % Frames in der Szene, % Keys\n" \
                            i (BafCpp.clipName i) fps frames \
                            (inSzene as integer) (BafCpp.clipKeys i)
                    )

                    -- ---- Dump ----
                    if BAF_DUMP != "" do
                    (
                        local ordner = getFilenamePath BAF_DUMP
                        if ordner != "" and not (doesFileExist ordner) do
                            makeDir ordner all:true
                        if (BafCpp.dump BAF_DUMP 3) == 1 then
                        (
                            format "\nDump geschrieben: % (%)\n" BAF_DUMP (BafCpp.timings())
                            format "Gegenprobe:\n  fc /L \"%\" dump_py\\%.txt\n" \
                                BAF_DUMP BAF_MODEL
                        )
                        else
                            format "\nFEHLER beim Dump: %\n" (BafCpp.lastError())
                    )
                )
            )
        )

        local protokoll = BafCpp.log()
        if protokoll != "" do format "\nProtokoll:\n%" protokoll
        BafCpp.setDebug 0
    )
    format "=== Ende ===\n"
    OK
)
