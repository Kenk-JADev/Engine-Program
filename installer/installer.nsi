; RPG Maker 3D Engine - NSIS Installer Script
; Erstellt ein Windows Setup für die Engine
; Benötigt NSIS (https://nsis.sourceforge.io/)

!define PRODUCT_NAME "RPG Maker 3D Engine"
!define PRODUCT_VERSION "0.2.0"
!define PRODUCT_PUBLISHER "RPG Maker 3D Team"
!define PRODUCT_WEB_SITE "https://github.com/Kenk-JADev/Engine-Program"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\RPGMaker3D.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI Settings
!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\icon.ico"
!define MUI_UNICON "..\resources\icon.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\README.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\RPGMaker3D.exe"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "RPGMaker3D-Setup-${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES64\RPG Maker 3D"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "Hauptprogramm" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite try
  File "..\build\Release\RPGMaker3D.exe"
  File /nonfatal "..\build\Release\RPGMaker3D_Player.exe"
  File /nonfatal "..\resources\icon.ico"
  File /nonfatal "..\README.md"
  File /nonfatal "..\BUILD_WINDOWS_DE.md"
  File /nonfatal "..\VERSION.txt"

  SetOutPath "$INSTDIR\assets"
  File /r /nonfatal "..\build\Release\assets\*.*"

  SetOutPath "$INSTDIR\SampleProject"
  File /r /nonfatal "..\build\Release\SampleProject\*.*"

  SetOutPath "$INSTDIR\ruby"
  File /r /nonfatal "..\build\Release\ruby\*.*"

  SetOutPath "$INSTDIR\docs"
  File /r /nonfatal "..\build\Release\docs\*.*"

  SetOutPath "$INSTDIR\resources"
  File /r /nonfatal "..\build\Release\resources\*.*"
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateDirectory "$SMPROGRAMS\RPG Maker 3D"
  CreateShortCut "$SMPROGRAMS\RPG Maker 3D\RPG Maker 3D.lnk" "$INSTDIR\RPGMaker3D.exe"
  CreateShortCut "$SMPROGRAMS\RPG Maker 3D\Player.lnk" "$INSTDIR\RPGMaker3D_Player.exe"
  CreateShortCut "$SMPROGRAMS\RPG Maker 3D\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\RPG Maker 3D\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\RPGMaker3D.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\RPGMaker3D.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) wurde erfolgreich deinstalliert."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Möchtest du $(^Name) wirklich deinstallieren?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\RPGMaker3D.exe"
  Delete "$INSTDIR\RPGMaker3D_Player.exe"
  Delete "$INSTDIR\icon.ico"

  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\SampleProject"
  RMDir /r "$INSTDIR\ruby"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\resources"

  RMDir "$SMPROGRAMS\RPG Maker 3D"
  RMDir /r "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
