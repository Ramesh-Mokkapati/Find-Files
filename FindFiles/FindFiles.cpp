#include "pch.h"
#include "FindFilesDlg.h"

class CFindFilesApp : public CWinApp
{
public:
    BOOL InitInstance() override;
};

CFindFilesApp theApp;

BOOL CFindFilesApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc{ sizeof(icc),
        ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES |
        ICC_BAR_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();
    AfxOleInit();   // COM for the shell (folder picker, properties, Recycle Bin)

    // Search history lives under HKCU\Software\FindFilesMFC.
    SetRegistryKey(L"FindFilesMFC");

    CFindFilesDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();
    return FALSE;
}
