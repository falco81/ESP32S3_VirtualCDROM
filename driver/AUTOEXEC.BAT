@ECHO OFF

IF "%CONFIG%" == "U2CD1" GOTO CD_DRIVER
IF "%CONFIG%" == "U1CD1" GOTO CD_DRIVER
IF "%CONFIG%" == "U3CD1" GOTO CD_DRIVER
IF "%CONFIG%" == "U1DD"  GOTO DD_DRIVER
GOTO END

:CD_DRIVER
ECHO  [%CONFIG%] Nacitam MSCDEX pro CD-ROM...
LH C:\DOS\MSCDEX.EXE /D:ESPCD0
IF ERRORLEVEL 1 GOTO CD_ERROR
ECHO.
ECHO  OK -- CD-ROM drive namapovan na D:
ECHO.
ECHO  Zkus: DIR D:\
ECHO        C:\CDPLAY\CDPLAY.EXE    (pokud mas CD Player for DOS)
GOTO END

:CD_ERROR
ECHO.
ECHO  CHYBA: MSCDEX se nespustil!
ECHO  Zkontroluj:
ECHO   1. Je ESP32-S3 zapnuty PRED startem PC?
ECHO   2. Je v web UI nastaveno dos-driver = 2 (USBCD1)?
ECHO   3. Zkus jiny USBASPI (restart a zvol jinou variantu v menu)
ECHO.
GOTO END

:DD_DRIVER
ECHO  [U1DD] USB storage -- DI1000DD (bez audio, jen data)
ECHO  Disk bude videt jako nova pismenka automaticky.
ECHO.

:END
ECHO.
PROMPT $P$G
