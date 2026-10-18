#include "pch.h"
#include "Pages.h"

namespace
{
    const int kHistoryMax = 10;
    const LPCWSTR kSection = L"History";

    CString DefaultLookIn()
    {
        WCHAR drive[MAX_PATH] = L"C:";
        GetEnvironmentVariableW(L"SystemDrive", drive, MAX_PATH);
        return CString(drive) + L"\\";
    }

    void AddKnownFolder(CComboBox& combo, REFKNOWNFOLDERID id)
    {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &path)))
        {
            combo.AddString(path);
            CoTaskMemFree(path);
        }
    }

    // Splits on any of the delimiter characters, trimming each token.
    std::vector<CString> Split(const CString& text, LPCWSTR delims)
    {
        std::vector<CString> out;
        int pos = 0;
        CString tok = text.Tokenize(delims, pos);
        while (!tok.IsEmpty())
        {
            tok.Trim();
            if (!tok.IsEmpty())
            {
                out.push_back(tok);
            }

            tok = text.Tokenize(delims, pos);
        }

        return out;
    }

    ULONGLONG LocalToUtc(SYSTEMTIME local)
    {
        SYSTEMTIME utc;
        FILETIME ft;
        TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc);
        SystemTimeToFileTime(&utc, &ft);
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    struct FileTypeDef { LPCWSTR name; LPCWSTR exts; };
    const FileTypeDef kTypes[] =
    {
        { L"All files and folders", nullptr },
        { L"All files",             L"" },
        { L"Documents",             L"txt;doc;docx;rtf;pdf;odt;ods;odp;xls;xlsx;csv;ppt;pptx;md;wri;xps" },
        { L"Images",                L"bmp;jpg;jpeg;png;gif;tif;tiff;ico;webp;heic;svg;raw;cr2;nef" },
        { L"Music",                 L"mp3;wav;wma;flac;aac;m4a;ogg;opus;mid;midi" },
        { L"Videos",                L"mp4;avi;mkv;mov;wmv;mpg;mpeg;webm;m4v;3gp" },
        { L"Compressed archives",   L"zip;rar;7z;tar;gz;tgz;bz2;xz;cab;iso" },
        { L"Programs",              L"exe;dll;com;bat;cmd;msi;ps1;scr;sys" },
        { L"Source code",           L"c;cpp;cc;cxx;h;hpp;cs;java;py;js;ts;rc;sln;vcxproj;html;css;xml;json" },
    };
}

/////////////////////////////////////////////////////////////////////////////
// CNamePage

BEGIN_MESSAGE_MAP(CNamePage, CSearchPage)
    ON_BN_CLICKED(IDC_BROWSE, &CNamePage::OnBrowse)
END_MESSAGE_MAP()

void CNamePage::DoDataExchange(CDataExchange* pDX)
{
    CSearchPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_NAMED, m_named);
    DDX_Control(pDX, IDC_LOOKIN, m_lookIn);
}

BOOL CNamePage::OnInitDialog()
{
    CSearchPage::OnInitDialog();

    CWinApp* app = AfxGetApp();
    for (int i = 0; i < kHistoryMax; i++)
    {
        CString key;
        key.Format(L"Named%d", i);
        CString v = app->GetProfileString(kSection, key);
        if (!v.IsEmpty())
        {
            m_named.AddString(v);
        }
    }

    WCHAR drives[512];
    DWORD len = GetLogicalDriveStringsW(_countof(drives), drives);
    for (LPCWSTR d = drives; len && *d; d += wcslen(d) + 1)
    {
        m_lookIn.AddString(d);
    }

    AddKnownFolder(m_lookIn, FOLDERID_Desktop);
    AddKnownFolder(m_lookIn, FOLDERID_Documents);
    AddKnownFolder(m_lookIn, FOLDERID_Downloads);

    Reset();
    return TRUE;
}

void CNamePage::Reset()
{
    m_named.SetWindowText(L"");
    m_lookIn.SetWindowText(AfxGetApp()->GetProfileString(kSection, L"LastLookIn", DefaultLookIn()));
    CheckDlgButton(IDC_SUBFOLDERS, BST_CHECKED);
}

CString CNamePage::GetNamedText() const
{
    CString s;
    m_named.GetWindowText(s);
    s.Trim();
    return s;
}

bool CNamePage::Fill(SearchCriteria& c)
{
    // Like the classic Find dialog, a plain word matches anywhere in the name.
    for (CString p : Split(GetNamedText(), L";,"))
    {
        if (p.FindOneOf(L"*?") < 0)
        {
            p = L"*" + p + L"*";
        }

        c.patterns.push_back(p);
    }

    CString look;
    m_lookIn.GetWindowText(look);
    for (const CString& raw : Split(look, L";"))
    {
        // Expand %VARS% and make the path absolute (no MAX_PATH-sized buffers,
        // so very deep folders still work).
        CString dir = raw;
        DWORD n = ExpandEnvironmentStringsW(raw, nullptr, 0);
        if (n > 0)
        {
            ExpandEnvironmentStringsW(raw, dir.GetBuffer(n), n);
            dir.ReleaseBuffer();
        }

        n = GetFullPathNameW(dir, 0, nullptr, nullptr);
        if (n > 0)
        {
            CString full;
            GetFullPathNameW(dir, n, full.GetBuffer(n), nullptr);
            full.ReleaseBuffer();
            dir = full;
        }

        DWORD attr = GetFileAttributesW(ExtendedPath(dir));
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            CString msg;
            msg.Format(L"The folder '%s' was not found.\n\nCheck the folder name in the Look in box.", (LPCWSTR)dir);
            GetParent()->GetParent()->MessageBox(msg, L"Find", MB_OK | MB_ICONWARNING);
            m_lookIn.SetFocus();
            return false;
        }

        if (dir[dir.GetLength() - 1] != L'\\')
        {
            dir += L'\\';
        }

        c.roots.push_back(dir);
    }

    if (c.roots.empty())
    {
        GetParent()->GetParent()->MessageBox(L"Please type or choose a folder in the Look in box.",
            L"Find", MB_OK | MB_ICONWARNING);
        m_lookIn.SetFocus();
        return false;
    }

    c.subfolders = IsDlgButtonChecked(IDC_SUBFOLDERS) == BST_CHECKED;
    return true;
}

void CNamePage::SaveHistory()
{
    CWinApp* app = AfxGetApp();
    CString named = GetNamedText();
    if (!named.IsEmpty())
    {
        int existing = m_named.FindStringExact(-1, named);
        if (existing != CB_ERR)
        {
            m_named.DeleteString(existing);
        }

        m_named.InsertString(0, named);
        while (m_named.GetCount() > kHistoryMax)
        {
            m_named.DeleteString(m_named.GetCount() - 1);
        }

        m_named.SetWindowText(named);

        for (int i = 0; i < m_named.GetCount(); i++)
        {
            CString key, v;
            key.Format(L"Named%d", i);
            m_named.GetLBText(i, v);
            app->WriteProfileString(kSection, key, v);
        }
    }

    CString look;
    m_lookIn.GetWindowText(look);
    app->WriteProfileString(kSection, L"LastLookIn", look);
    if (m_lookIn.FindStringExact(-1, look) == CB_ERR)
    {
        m_lookIn.InsertString(0, look);
    }
}

void CNamePage::OnBrowse()
{
    CString current;
    m_lookIn.GetWindowText(current);
    CFolderPickerDialog dlg(current, 0, this);
    if (dlg.DoModal() == IDOK)
    {
        m_lookIn.SetWindowText(dlg.GetPathName());
    }
}

/////////////////////////////////////////////////////////////////////////////
// CDatePage

BEGIN_MESSAGE_MAP(CDatePage, CSearchPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_DATE_ALL, IDC_DATE_DAYS, &CDatePage::OnModeChanged)
END_MESSAGE_MAP()

void CDatePage::DoDataExchange(CDataExchange* pDX)
{
    CSearchPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DATE_FROM, m_from);
    DDX_Control(pDX, IDC_DATE_TO, m_to);
    DDX_Control(pDX, IDC_MONTHS_SPIN, m_monthsSpin);
    DDX_Control(pDX, IDC_DAYS_SPIN, m_daysSpin);
}

BOOL CDatePage::OnInitDialog()
{
    CSearchPage::OnInitDialog();
    m_monthsSpin.SetRange32(1, 999);
    m_daysSpin.SetRange32(1, 9999);
    Reset();
    return TRUE;
}

void CDatePage::Reset()
{
    CheckRadioButton(IDC_DATE_ALL, IDC_DATE_DAYS, IDC_DATE_ALL);
    SYSTEMTIME now;
    GetLocalTime(&now);
    m_from.SetTime(&now);
    m_to.SetTime(&now);
    m_monthsSpin.SetPos32(1);
    m_daysSpin.SetPos32(1);
    EnableControls();
}

void CDatePage::OnModeChanged(UINT)
{
    EnableControls();
}

void CDatePage::EnableControls()
{
    int mode = GetCheckedRadioButton(IDC_DATE_ALL, IDC_DATE_DAYS);
    GetDlgItem(IDC_DATE_FROM)->EnableWindow(mode == IDC_DATE_BETWEEN);
    GetDlgItem(IDC_DATE_TO)->EnableWindow(mode == IDC_DATE_BETWEEN);
    GetDlgItem(IDC_MONTHS)->EnableWindow(mode == IDC_DATE_MONTHS);
    GetDlgItem(IDC_MONTHS_SPIN)->EnableWindow(mode == IDC_DATE_MONTHS);
    GetDlgItem(IDC_DAYS)->EnableWindow(mode == IDC_DATE_DAYS);
    GetDlgItem(IDC_DAYS_SPIN)->EnableWindow(mode == IDC_DATE_DAYS);
}

bool CDatePage::Fill(SearchCriteria& c)
{
    int mode = GetCheckedRadioButton(IDC_DATE_ALL, IDC_DATE_DAYS);
    if (mode == IDC_DATE_BETWEEN)
    {
        SYSTEMTIME from, to;
        m_from.GetTime(&from);
        m_to.GetTime(&to);
        from.wHour = from.wMinute = from.wSecond = from.wMilliseconds = 0;
        to.wHour = 23; to.wMinute = 59; to.wSecond = 59; to.wMilliseconds = 999;
        c.useDate = true;
        c.dateFrom = LocalToUtc(from);
        c.dateTo = LocalToUtc(to);

        if (c.dateFrom > c.dateTo)
        {
            std::swap(c.dateFrom, c.dateTo);
        }
    }
    else if (mode == IDC_DATE_MONTHS)
    {
        int months = static_cast<int>(GetDlgItemInt(IDC_MONTHS));
        if (months < 1)
        {
            months = 1;
        }

        SYSTEMTIME t;
        GetLocalTime(&t);
        int m = (t.wYear * 12 + (t.wMonth - 1)) - months;
        t.wYear = static_cast<WORD>(m / 12);
        t.wMonth = static_cast<WORD>(m % 12 + 1);
        static const int kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int maxDay = kDays[t.wMonth - 1];
        if (t.wMonth == 2 && ((t.wYear % 4 == 0 && t.wYear % 100 != 0) || t.wYear % 400 == 0))
        {
            maxDay = 29;
        }

        if (t.wDay > maxDay)
        {
            t.wDay = static_cast<WORD>(maxDay);
        }

        c.useDate = true;
        c.dateFrom = LocalToUtc(t);
    }
    else if (mode == IDC_DATE_DAYS)
    {
        ULONGLONG days = GetDlgItemInt(IDC_DAYS);
        if (days < 1)
        {
            days = 1;
        }

        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        ULONGLONG n = (static_cast<ULONGLONG>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
        c.useDate = true;
        c.dateFrom = n - days * 864000000000ULL;   // 100 ns units per day
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// CAdvPage

BEGIN_MESSAGE_MAP(CAdvPage, CSearchPage)
    ON_CBN_SELCHANGE(IDC_SIZEOP, &CAdvPage::OnSizeOpChanged)
END_MESSAGE_MAP()

void CAdvPage::DoDataExchange(CDataExchange* pDX)
{
    CSearchPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_OFTYPE, m_type);
    DDX_Control(pDX, IDC_SIZEOP, m_sizeOp);
    DDX_Control(pDX, IDC_SIZE_SPIN, m_sizeSpin);
}

BOOL CAdvPage::OnInitDialog()
{
    CSearchPage::OnInitDialog();
    for (const FileTypeDef& t : kTypes)
    {
        m_type.AddString(t.name);
    }

    m_sizeOp.AddString(L"(any size)");
    m_sizeOp.AddString(L"At least");
    m_sizeOp.AddString(L"At most");
    m_sizeSpin.SetRange32(0, 0x7FFFFFFF);
    Reset();
    return TRUE;
}

void CAdvPage::Reset()
{
    m_type.SetCurSel(0);
    SetDlgItemText(IDC_CONTAINING, L"");
    m_sizeOp.SetCurSel(SIZE_ANY);
    m_sizeSpin.SetPos32(0);
    CheckDlgButton(IDC_HIDDEN, BST_UNCHECKED);
    OnSizeOpChanged();
}

void CAdvPage::OnSizeOpChanged()
{
    bool enable = m_sizeOp.GetCurSel() != SIZE_ANY;
    GetDlgItem(IDC_SIZEKB)->EnableWindow(enable);
    GetDlgItem(IDC_SIZE_SPIN)->EnableWindow(enable);
}

bool CAdvPage::Fill(SearchCriteria& c)
{
    int t = m_type.GetCurSel();
    if (t < 0 || t >= _countof(kTypes))
    {
        t = 0;
    }

    c.includeFolders = kTypes[t].exts == nullptr;
    if (kTypes[t].exts)
    {
        for (CString e : Split(kTypes[t].exts, L";"))
        {
            e.MakeLower();
            c.extensions.push_back(e);
        }
    }

    GetDlgItemText(IDC_CONTAINING, c.containing);
    c.sizeOp = (std::max)(0, m_sizeOp.GetCurSel());
    c.sizeBytes = static_cast<ULONGLONG>(GetDlgItemInt(IDC_SIZEKB, nullptr, FALSE)) * 1024;
    c.includeHidden = IsDlgButtonChecked(IDC_HIDDEN) == BST_CHECKED;

    // Folders have no contents or size, so these filters imply files only.
    if (!c.containing.IsEmpty() || c.sizeOp != SIZE_ANY)
    {
        c.includeFolders = false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// CDupPage

BEGIN_MESSAGE_MAP(CDupPage, CSearchPage)
    ON_BN_CLICKED(IDC_DUPS, &CDupPage::OnDupsClicked)
END_MESSAGE_MAP()

BOOL CDupPage::OnInitDialog()
{
    CSearchPage::OnInitDialog();
    Reset();
    return TRUE;
}

void CDupPage::Reset()
{
    CheckDlgButton(IDC_DUPS, BST_UNCHECKED);
    CheckRadioButton(IDC_DUP_NAMESIZE, IDC_DUP_ALL, IDC_DUP_CONTENT);
    SetDlgItemInt(IDC_DUP_MINSIZE, 0, FALSE);
    CheckDlgButton(IDC_DUP_SKIPEMPTY, BST_CHECKED);
    EnableControls();
}

bool CDupPage::IsDupEnabled() const
{
    return IsDlgButtonChecked(IDC_DUPS) == BST_CHECKED;
}

void CDupPage::SetDupEnabled(bool enable)
{
    CheckDlgButton(IDC_DUPS, enable ? BST_CHECKED : BST_UNCHECKED);
    EnableControls();
}

void CDupPage::OnDupsClicked()
{
    EnableControls();
}

void CDupPage::EnableControls()
{
    BOOL on = IsDupEnabled();
    static const UINT kIds[] = { IDC_DUP_GROUPBOX, IDC_DUP_NAMESIZE, IDC_DUP_CONTENT, IDC_DUP_ALL,
                                 IDC_DUP_MINLABEL, IDC_DUP_MINSIZE, IDC_DUP_KBLABEL, IDC_DUP_SKIPEMPTY };
    for (UINT id : kIds)
    {
        GetDlgItem(id)->EnableWindow(on);
    }
}

bool CDupPage::Fill(SearchCriteria& c)
{
    c.findDups = IsDupEnabled();
    c.dupMode = GetCheckedRadioButton(IDC_DUP_NAMESIZE, IDC_DUP_ALL) - IDC_DUP_NAMESIZE;
    if (c.dupMode < 0)
    {
        c.dupMode = DUP_CONTENT;
    }

    c.dupMinSize = static_cast<ULONGLONG>(GetDlgItemInt(IDC_DUP_MINSIZE, nullptr, FALSE)) * 1024;
    c.skipEmpty = IsDlgButtonChecked(IDC_DUP_SKIPEMPTY) == BST_CHECKED;
    if (c.findDups)
    {
        c.includeFolders = false;
    }

    return true;
}
