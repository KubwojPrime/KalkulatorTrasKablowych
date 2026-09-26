Unicode True
SetCompressor /SOLID zlib

!ifndef APP_VERSION
  !error "APP_VERSION is required"
!endif
!ifndef SOURCE_DIR
  !error "SOURCE_DIR is required"
!endif
!ifndef OUTPUT_DIR
  !error "OUTPUT_DIR is required"
!endif
!ifndef LICENSE_FILE
  !error "LICENSE_FILE is required"
!endif

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "${UNINSTALL_FILES}"

Var NoShortcuts

Name "Kalkulator Tras Kablowych"
OutFile "${OUTPUT_DIR}\KalkulatorTrasKablowych-${APP_VERSION}-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Kalkulator Tras Kablowych"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
BrandingText "Kalkulator Tras Kablowych ${APP_VERSION}"

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=1045 "ProductName" "Kalkulator Tras Kablowych"
VIAddVersionKey /LANG=1045 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1045 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1045 "FileDescription" "Instalator Kalkulatora Tras Kablowych"
VIAddVersionKey /LANG=1045 "LegalCopyright" "Copyright (c) 2026 Jakub"

!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\KalkulatorTrasKablowych.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Wybierz docelowy pusty katalog albo katalog wcześniejszej instalacji. Jeśli wybierasz istniejącą lokalizację, utwórz w niej osobny podfolder dla programu."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryPageLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext all
  SetRegView 64
  ReadRegStr $R2 HKLM "Software\KubwojPrime\KalkulatorTrasKablowych" "InstallDir"
  StrCmp $R2 "" no_previous_installation
  StrCpy $INSTDIR $R2

  no_previous_installation:
  ; Respect an explicit /D= path even when a previous install is registered.
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/D=" $R2
  IfErrors no_directory_override
  StrCpy $INSTDIR $R2
  no_directory_override:
  ClearErrors
  ${GetOptions} $R0 "/NO_SHORTCUTS=" $R1
  IfErrors no_shortcut_override
  StrCpy $NoShortcuts $R1

  no_shortcut_override:
FunctionEnd

Function un.onInit
  SetShellVarContext all
  SetRegView 64
FunctionEnd

Function ValidateInstallDirectory
  StrCpy $R9 "0"
  StrCmp $INSTDIR "" validate_done
  IfFileExists "$INSTDIR\KalkulatorTrasKablowych.exe" validate_existing_install scan_directory

  validate_existing_install:
    ; Identify the actual program, including older/portable installs without a marker.
    ; A filename alone is insufficient: check embedded product and company metadata.
    System::Call 'version::GetFileVersionInfoSizeW(w "$INSTDIR\KalkulatorTrasKablowych.exe", *i .r2) i .r3'
    IntCmp $3 0 scan_directory scan_directory
    System::Alloc $3
    Pop $4
    StrCmp $4 0 scan_directory
    StrCpy $8 ""
    StrCpy $1 ""
    System::Call 'version::GetFileVersionInfoW(w "$INSTDIR\KalkulatorTrasKablowych.exe", i 0, i r3, p r4) i .r5'
    StrCmp $5 0 free_version
    System::Call 'version::VerQueryValueW(p r4, w "\StringFileInfo\041504b0\ProductName", *p .r6, *i .r7) i .r5'
    StrCmp $5 0 free_version
    System::Call '*$6(&w${NSIS_MAX_STRLEN} .r8)'
    System::Call 'version::VerQueryValueW(p r4, w "\StringFileInfo\041504b0\CompanyName", *p .r6, *i .r7) i .r5'
    StrCmp $5 0 free_version
    System::Call '*$6(&w${NSIS_MAX_STRLEN} .r1)'
    free_version:
    System::Free $4
    StrCmp $8 "Kalkulator Tras Kablowych" 0 scan_directory
    StrCmp $1 "KubwojPrime" directory_valid scan_directory

  scan_directory:
    ClearErrors
    FindFirst $R0 $R1 "$INSTDIR\*"
    IfErrors directory_valid

  scan_next_entry:
    StrCmp $R1 "." continue_scan
    StrCmp $R1 ".." continue_scan
    FindClose $R0
    Goto validate_done

  continue_scan:
    ClearErrors
    FindNext $R0 $R1
    IfErrors directory_empty
    Goto scan_next_entry

  directory_empty:
    FindClose $R0

  directory_valid:
    StrCpy $R9 "1"

  validate_done:
FunctionEnd

Function DirectoryPageLeave
  Call ValidateInstallDirectory
  ${If} $R9 != "1"
    MessageBox MB_ICONEXCLAMATION|MB_OK "Wybrany katalog nie jest pusty i nie jest katalogiem wcześniejszej instalacji. Utwórz osobny pusty podfolder, np. 'Kalkulator Tras Kablowych'. Chroni to inne pliki przed usunięciem podczas deinstalacji."
    Abort
  ${EndIf}
FunctionEnd

Section "Program (wymagane)" MainSection
  SectionIn RO
  Call ValidateInstallDirectory
  ${If} $R9 != "1"
    SetErrorLevel 2
    Quit
  ${EndIf}

  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*"
  FileOpen $0 "$INSTDIR\.ktk-install-root" w
  FileWrite $0 "KTK-INSTALL-ROOT-v1"
  FileClose $0
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  Delete "$DESKTOP\Kalkulator Tras Kablowych.lnk"
  Delete "$SMPROGRAMS\Kalkulator Tras Kablowych\Kalkulator Tras Kablowych.lnk"
  Delete "$SMPROGRAMS\Kalkulator Tras Kablowych\Odinstaluj.lnk"
  RMDir "$SMPROGRAMS\Kalkulator Tras Kablowych"

  WriteRegStr HKLM "Software\KubwojPrime\KalkulatorTrasKablowych" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayName" "Kalkulator Tras Kablowych"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "Publisher" "KubwojPrime"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayIcon" "$INSTDIR\KalkulatorTrasKablowych.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "NoRepair" 1
SectionEnd

Section "Skrót w menu Start" StartMenuShortcutSection
  StrCmp $NoShortcuts "1" start_menu_done
  CreateDirectory "$SMPROGRAMS\Kalkulator Tras Kablowych"
  CreateShortcut "$SMPROGRAMS\Kalkulator Tras Kablowych\Kalkulator Tras Kablowych.lnk" "$INSTDIR\KalkulatorTrasKablowych.exe"
  CreateShortcut "$SMPROGRAMS\Kalkulator Tras Kablowych\Odinstaluj.lnk" "$INSTDIR\Uninstall.exe"

  start_menu_done:
SectionEnd

Section /o "Skrót na pulpicie" DesktopShortcutSection
  StrCmp $NoShortcuts "1" desktop_done
  CreateShortcut "$DESKTOP\Kalkulator Tras Kablowych.lnk" "$INSTDIR\KalkulatorTrasKablowych.exe"

  desktop_done:
SectionEnd

Section "Uninstall"
  FileOpen $0 "$INSTDIR\.ktk-install-root" r
  IfErrors unsafe_install_root
  FileRead $0 $1
  FileClose $0
  StrCmp $1 "KTK-INSTALL-ROOT-v1" safe_install_root unsafe_install_root

  unsafe_install_root:
    MessageBox MB_ICONSTOP "Nie można bezpiecznie potwierdzić katalogu instalacji. Pliki nie zostały usunięte."
    SetErrorLevel 2
    Quit

  safe_install_root:
  Delete "$SMPROGRAMS\Kalkulator Tras Kablowych\Kalkulator Tras Kablowych.lnk"
  Delete "$SMPROGRAMS\Kalkulator Tras Kablowych\Odinstaluj.lnk"
  RMDir "$SMPROGRAMS\Kalkulator Tras Kablowych"
  Delete "$DESKTOP\Kalkulator Tras Kablowych.lnk"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych"
  DeleteRegKey HKLM "Software\KubwojPrime\KalkulatorTrasKablowych"
  Delete "$INSTDIR\.ktk-install-root"
  !insertmacro RemoveInstalledFiles
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
