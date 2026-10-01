cmake_minimum_required(VERSION 3.20)
project(baftoolkit-max LANGUAGES CXX)

# ------------------------------------------------------------
#  C++17, nicht 20 - dieselbe Begruendung wie bei XFBIN Import.
#  Gebraucht wird davon nur std::to_chars fuer die locale-feste
#  Zahlenausgabe. C++17 uebersetzt auch gegen die Header der
#  aelteren Max-SDKs.
#
#  Achtung: std::to_chars fuer FLOAT gibt es in MSVC erst ab
#  Visual Studio 2019 16.4. Fuer Max 2016-2018 wird mit dem
#  aelteren Toolset gebaut - deshalb steht in baf_reader.cpp
#  eine Weiche auf snprintf mit fest gesetztem C-Locale.
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MAX_VERSION "0" CACHE STRING "3ds Max Jahrgang, z.B. 2016")

set(3DSMAX_SDK_DIR "C:/Program Files/Autodesk/3ds Max 2025 SDK/maxsdk"
    CACHE PATH "3ds Max SDK root (maxsdk-Ordner)")

option(BAF_BUILD_TOOL   "Eigenstaendiges bafdump.exe mitbauen" ON)
option(BAF_BUILD_PLUGIN "Max-Plugin bauen (braucht das Max SDK)" ON)
option(BAF_STRICT       "Strenge Compiler-Warnungen (/W4 /permissive-)" ON)

# ============================================================
#  Gemeinsame Parser-Quellen
#
#  Bewusst als Liste statt als eigene Bibliothek: das Plugin
#  braucht /Zp8 (Struct-Packing des Max SDK), das Werkzeug nicht.
# ============================================================
set(BAF_PARSER_SOURCES
    src/baf_lab.h
    src/baf_lab.cpp
    src/baf_reader.h
    src/baf_reader.cpp
    src/baf_writer.cpp
)

# ------------------------------------------------------------
#  Keine Kontraktion von Gleitkommaoperationen.
#
#  Der Compiler darf a*b+c zu einem einzigen FMA-Befehl
#  zusammenziehen. Das ist SCHNELLER und GENAUER - es faellt nur
#  ein Zwischenrundungsschritt weg. Trotzdem ist es hier
#  unerwuenscht: die Weltmatrizen der Ruhelage entstehen aus
#  genau solchen Summen, und ob kontrahiert wird, haengt vom
#  Compiler und den Zielbefehlssaetzen ab.
#
#  Nachgemessen an den Retail-Daten: mit Kontraktion aendern sich
#  18 von 21.166 Ruhelage-Zeilen in 13 der 303 Modelle. Der
#  Vergleich gegen die Python-Referenz haette dort Abweichungen
#  gemeldet, an denen nichts falsch ist - und die Suche danach
#  waere lang geworden.
#
#  Also ausgeschaltet, auf beiden Compilern. Der Geschwindigkeits-
#  verlust ist bei 80 Mikrosekunden je Modell kein Thema.
# ------------------------------------------------------------
if(MSVC)
    set(BAF_FP_FLAGS /fp:precise)
else()
    set(BAF_FP_FLAGS -ffp-contract=off)
endif()

# ============================================================
#  Plugin-Target -> BafToolkit.dlu
# ============================================================
if(BAF_BUILD_PLUGIN)

    if(NOT EXISTS "${3DSMAX_SDK_DIR}/include/max.h")
        message(FATAL_ERROR
            "3ds Max SDK nicht gefunden unter: ${3DSMAX_SDK_DIR}\n"
            "Entweder -D3DSMAX_SDK_DIR=... setzen oder mit "
            "-DBAF_BUILD_PLUGIN=OFF nur das Werkzeug bauen.")
    endif()

    add_library(BafToolkit SHARED
        ${BAF_PARSER_SOURCES}
        src/baftoolkit.h
        src/baftoolkit.cpp
        src/baftoolkit_dll.cpp
        baftoolkit.def
    )

    set_target_properties(BafToolkit PROPERTIES
        SUFFIX ".dlu" PREFIX "" OUTPUT_NAME "BafToolkit")

    # 3ds Max laeuft ausschliesslich mit der dynamischen CRT (/MD).
    # Ein versehentliches /MT ergibt getrennte Heaps und damit
    # Abstuerze, die erst weit spaeter auffallen.
    set_property(TARGET BafToolkit PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    if(CMAKE_SIZEOF_VOID_P LESS 8)
        message(FATAL_ERROR "BafToolkit laesst sich nur als x64 bauen (cmake -A x64).")
    endif()

    target_include_directories(BafToolkit SYSTEM PRIVATE "${3DSMAX_SDK_DIR}/include")
    target_link_directories(BafToolkit PRIVATE "${3DSMAX_SDK_DIR}/lib/x64/Release")

    target_link_libraries(BafToolkit PRIVATE
        core.lib maxutil.lib bmm.lib Geom.lib mesh.lib
        Maxscrpt.lib MNMath.lib Paramblk2.lib comctl32.lib)

    target_compile_definitions(BafToolkit PRIVATE
        _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    if(MSVC)
        target_compile_options(BafToolkit PRIVATE
            /EHsc /Zp8 /bigobj /utf-8 /Zc:__cplusplus /external:W0 ${BAF_FP_FLAGS})

        # ------------------------------------------------------
        #  /permissive- nur, wenn die SDK-Header es aushalten.
        #  Nicht der eigene Quelltext ist das Problem, sondern die
        #  Header von Autodesk: strictStrings -> C2440,
        #  rvalueCast -> C2102, dazu C2216 in winutil.h.
        #  Eine Jahresgrenze waere geraten, also wird es einmal
        #  ausprobiert und im Cache behalten.
        # ------------------------------------------------------
        set(_permissive_ok FALSE)
        if(BAF_STRICT)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_INCLUDES "${3DSMAX_SDK_DIR}/include")
            set(CMAKE_REQUIRED_FLAGS    "/permissive- /Zp8 /EHsc /bigobj")
            set(CMAKE_REQUIRED_DEFINITIONS
                -D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
            set(CMAKE_REQUIRED_QUIET TRUE)
            check_cxx_source_compiles(
                "#include <max.h>\nint main() { return 0; }"
                BAF_SDK_LIKES_PERMISSIVE)
            unset(CMAKE_REQUIRED_INCLUDES)
            unset(CMAKE_REQUIRED_FLAGS)
            unset(CMAKE_REQUIRED_DEFINITIONS)
            unset(CMAKE_REQUIRED_QUIET)
            if(BAF_SDK_LIKES_PERMISSIVE)
                set(_permissive_ok TRUE)
            endif()
        endif()

        if(BAF_STRICT AND _permissive_ok)
            message(STATUS "SDK vertraegt /permissive- - Konformitaetspruefung an")
            target_compile_options(BafToolkit PRIVATE /W4 /permissive- /wd4100 /wd4458)
        elseif(BAF_STRICT)
            message(STATUS "SDK-Header vertragen /permissive- nicht - nur /W4")
            target_compile_options(BafToolkit PRIVATE /W4 /wd4100 /wd4458)
        else()
            target_compile_options(BafToolkit PRIVATE /W3)
        endif()

        target_link_options(BafToolkit PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    endif()

endif()

# ============================================================
#  Werkzeug-Target -> bafdump.exe (ohne Max SDK)
# ============================================================
if(BAF_BUILD_TOOL)
    add_executable(bafdump ${BAF_PARSER_SOURCES} tools/bafdump_main.cpp)
    if(MSVC)
        set_property(TARGET bafdump PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        target_compile_options(bafdump PRIVATE /EHsc /utf-8 /Zc:__cplusplus ${BAF_FP_FLAGS})
        if(BAF_STRICT)
            target_compile_options(bafdump PRIVATE /W4 /permissive-)
        endif()
        target_compile_definitions(bafdump PRIVATE
            _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    else()
        target_compile_options(bafdump PRIVATE -Wall -Wextra ${BAF_FP_FLAGS})
    endif()
endif()

message(STATUS "BafToolkit: Max ${MAX_VERSION}  Plugin=${BAF_BUILD_PLUGIN}  "
               "Werkzeug=${BAF_BUILD_TOOL}  Strict=${BAF_STRICT}  C++${CMAKE_CXX_STANDARD}")
