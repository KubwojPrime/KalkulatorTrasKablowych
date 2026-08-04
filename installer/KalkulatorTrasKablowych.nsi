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

Var NoShortcuts

Name "Kalkulator Tras Kablowych"
OutFile "${OUTPUT_DIR}\KalkulatorTrasKablowych-${APP_VERSION}-win64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\Kalkulator Tras Kablowych"
InstallDirRegKey HKCU "Software\KubwojPrime\KalkulatorTrasKablowych" "InstallDir"
RequestExecutionLevel user
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
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/NO_SHORTCUTS=" $R1
  IfErrors no_shortcut_override
  StrCpy $NoShortcuts $R1

  no_shortcut_override:
FunctionEnd

Function ValidateInstallDirectory
  StrCpy $R9 "0"
  StrCmp $INSTDIR "" validate_done
  IfFileExists "$INSTDIR\*.*" directory_not_empty directory_valid

  directory_not_empty:
    IfFileExists "$INSTDIR\.ktk-install-root" 0 validate_done
    ClearErrors
    FileOpen $0 "$INSTDIR\.ktk-install-root" r
    IfErrors validate_done
    FileRead $0 $1
    FileClose $0
    StrCmp $1 "KTK-INSTALL-ROOT-v1" directory_valid validate_done

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

  WriteRegStr HKCU "Software\KubwojPrime\KalkulatorTrasKablowych" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayName" "Kalkulator Tras Kablowych"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "Publisher" "Jakub"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "DisplayIcon" "$INSTDIR\KalkulatorTrasKablowych.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych" "NoRepair" 1
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
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych"
  DeleteRegKey HKCU "Software\KubwojPrime\KalkulatorTrasKablowych"
  Delete "$INSTDIR\.ktk-install-root"
  RMDir /r "$INSTDIR"
SectionEnd
