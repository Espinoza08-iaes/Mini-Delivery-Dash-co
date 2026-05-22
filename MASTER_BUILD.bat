@echo off
REM ============================================================
REM MASTER BUILD SCRIPT - Lab8 Graphics Project
REM Complete setup and compilation in one go
REM ============================================================

setlocal enabledelayedexpansion

REM Set the working directory
cd /d "c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8"

cls
color 0A
echo.
echo ╔═══════════════════════════════════════════════════════════╗
echo ║      Lab8 - Master Build Script                          ║
echo ║      Carga de modelo glTF                               ║
echo ╚═══════════════════════════════════════════════════════════╝
echo.

REM Counter for steps
set step=1

REM =====  STEP 1: Verify source files =====
echo [%step%/5] Verifying source files...
set /A step+=1
if not exist "src\main.cpp" (
    echo  ERROR: main.cpp not found!
    pause & exit /b 1
)
if not exist "src\glad_loader.c" (
    echo  ERROR: glad_loader.c not found!
    pause & exit /b 1
)
echo  [OK] Source files verified
echo.

REM =====  STEP 2: Create directories =====
echo [%step%/5] Creating necessary directories...
set /A step+=1
if not exist "Dependencies\lib" mkdir "Dependencies\lib"
if not exist "Dependencies\include\SOIL2" mkdir "Dependencies\include\SOIL2"
echo  [OK] Directories created
echo.

REM =====  STEP 3: Copy SOIL2 files =====
echo [%step%/5] Setting up SOIL2 library...
set /A step+=1

REM Copy headers
echo  Copying SOIL2 headers...
for %%f in (SOIL2-master\src\SOIL2\*.h) do (
    copy /Y "%%f" "Dependencies\include\SOIL2\%%~nxf" >nul 2>&1
)

REM Copy library
if exist "SOIL2-master\build\libsoil2.a" (
    copy /Y "SOIL2-master\build\libsoil2.a" "Dependencies\lib\libsoil2.a" >nul 2>&1
    if exist "Dependencies\lib\libsoil2.a" (
        echo  [OK] SOIL2 library ready
    ) else (
        echo  ERROR: Could not copy SOIL2 library!
        pause & exit /b 1
    )
) else (
    echo  ERROR: SOIL2 library not found!
    echo  Expected: SOIL2-master\build\libsoil2.a
    echo.
    echo  To compile SOIL2, run: build_soil2.bat
    pause & exit /b 1
)
echo.

REM =====  STEP 4: Compile the project =====
echo [%step%/5] Compiling Lab8...
set /A step+=1
echo.

g++.exe -std=c++11 -fdiagnostics-color=always -g ^
  "-Ic:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\src\engine" ^
  "-Ic:\Universidad\QuintoSemestre\Programacion Grafica\Lab8" ^
  "-Ic:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\Dependencies\include" ^
  "-Ic:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\Dependencies\include\SOIL2" ^
  "-Ic:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\Dependencies\include\glm" ^
  "src\main.cpp" "src\engine\Mesh.cpp" "src\engine\Model.cpp" "src\engine\Texture.cpp" "src\glad_loader.c" ^
  -o "main.exe" ^
  "-Lc:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\Dependencies\lib" ^
  -lsoil2 -lglfw3 -lglew32s -lopengl32 -lgdi32 -luser32

if %errorlevel% neq 0 (
    echo.
    echo.
    echo ╔═══════════════════════════════════════════════════════════╗
    echo ║  COMPILATION FAILED!                                      ║
    echo ╚═══════════════════════════════════════════════════════════╝
    echo.
    echo Check the error messages above for details.
    echo.
    pause
    exit /b 1
)

REM =====  STEP 5: Verify compilation success =====
echo [%step%/5] Verifying compilation...
set /A step+=1
if exist "main.exe" (
    echo  [OK] main.exe created successfully
) else (
    echo  ERROR: main.exe was not created!
    pause & exit /b 1
)
echo.

REM =====  SUCCESS =====
cls
color 0A
echo.
echo ╔═══════════════════════════════════════════════════════════╗
echo ║                   SUCCESS!                                ║
echo ║  Lab8 has been compiled successfully!                    ║
echo ╚═══════════════════════════════════════════════════════════╝
echo.
echo Executable: main.exe
echo Textures:   Textures/ folder
echo Shaders:    res/shaders/ folder
echo.
echo Press any key to run the program...
echo.

pause

REM =====  RUN THE PROGRAM =====
echo Running main.exe...
echo.

main.exe

REM Cleanup
if %errorlevel% equ 0 (
    echo.
    echo Program exited normally.
) else (
    echo.
    echo Program exited with error code: %errorlevel%
)

pause

endlocal
