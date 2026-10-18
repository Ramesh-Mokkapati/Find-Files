#include "pch.h"
#include "FindFilesDlg.h"

namespace
{
    const UINT_PTR kAnimTimer = 1;

    enum Column { COL_NAME, COL_FOLDER, COL_SIZE, COL_TYPE, COL_MODIFIED, COL_COUNT };

    CString FormatSize(const FileEntry& e)
    {
        if (e.IsFolder())
        {
            return CString();
        }

        WCHAR buf[64];
        StrFormatKBSizeW(static_cast<LONGLONG>(e.size), buf, _countof(buf));
        return buf;
    }

    CString FormatBytes(ULONGLONG n)
    {
        WCHAR buf[64];
        StrFormatByteSizeW(static_cast<LONGLONG>(n), buf, _countof(buf));
        return buf;
    }

    CString FormatTime(const FILETIME& ft)
    {
        DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
        WCHAR buf[64] = L"";
        SHFormatDateTimeW(&ft, &flags, buf, _countof(buf));
        return buf;
    }

    CString CsvField(const CString& s)
    {
        CString q = s;
        q.Replace(L"\"", L"\"\"");
        return L"\"" + q + L"\"";
    }

    bool FileExists(const CString& path)
    {
        return GetFileAttributesW(ExtendedPath(path)) != INVALID_FILE_ATTRIBUTES;
    }

    // Network shares and removable/optical drives have no Recycle Bin, so the
    // shell deletes files there permanently.
    bool HasRecycleBin(const CString& path)
    {
        if (PathIsUNCW(path) || path.GetLength() < 2 || path[1] != L':')
        {
            return false;
        }

        WCHAR root[] = { path[0], L':', L'\\', 0 };
        return GetDriveTypeW(root) == DRIVE_FIXED;
    }
}

CFindFilesDlg::CFindFilesDlg(CWnd* pParent)
    : CDialog(IDD_FINDFILES, pParent)
{
    m_pages[0] = &m_pageName;
    m_pages[1] = &m_pageDate;
    m_pages[2] = &m_pageAdv;
    m_pages[3] = &m_pageDup;
}

void CFindFilesDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB, m_tab);
    DDX_Control(pDX, IDC_RESULTS, m_list);
}

BEGIN_MESSAGE_MAP(CFindFilesDlg, CDialog)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_TIMER()
    ON_WM_DRAWITEM()
    ON_WM_CONTEXTMENU()
    ON_WM_INITMENUPOPUP()
    ON_WM_CLOSE()
    ON_WM_DESTROY()

    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, &CFindFilesDlg::OnTabChange)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_RESULTS, &CFindFilesDlg::OnColumnClick)
    ON_NOTIFY(NM_DBLCLK, IDC_RESULTS, &CFindFilesDlg::OnListDblClk)

    ON_BN_CLICKED(IDC_FINDNOW, &CFindFilesDlg::OnFindNow)
    ON_BN_CLICKED(IDC_STOP, &CFindFilesDlg::OnStop)
    ON_BN_CLICKED(IDC_NEWSEARCH, &CFindFilesDlg::OnNewSearch)

    ON_MESSAGE(WM_APP_RESULTS, &CFindFilesDlg::OnSearchResults)
    ON_MESSAGE(WM_APP_STATUS, &CFindFilesDlg::OnSearchStatus)
    ON_MESSAGE(WM_APP_DONE, &CFindFilesDlg::OnSearchDone)

    ON_COMMAND(ID_FILE_OPEN_ITEM, &CFindFilesDlg::OnFileOpen)
    ON_COMMAND(ID_FILE_OPENFOLDER, &CFindFilesDlg::OnFileOpenFolder)
    ON_COMMAND(ID_FILE_DELETE_ITEM, &CFindFilesDlg::OnFileDelete)
    ON_COMMAND(ID_FILE_MOVETO, &CFindFilesDlg::OnFileMoveTo)
    ON_COMMAND(ID_FILE_PROPERTIES, &CFindFilesDlg::OnFileProperties)
    ON_COMMAND(ID_FILE_SAVERESULTS, &CFindFilesDlg::OnFileSaveResults)
    ON_COMMAND(ID_FILE_CLOSEAPP, &CFindFilesDlg::OnFileClose)
    ON_COMMAND(ID_EDIT_COPYPATH, &CFindFilesDlg::OnEditCopyPath)
    ON_COMMAND(ID_EDIT_SELECTALL, &CFindFilesDlg::OnEditSelectAll)
    ON_COMMAND(ID_EDIT_INVERT, &CFindFilesDlg::OnEditInvert)
    ON_COMMAND_RANGE(ID_DUP_KEEPFIRST, ID_DUP_KEEPSHORTEST, &CFindFilesDlg::OnSelectDuplicates)
    ON_COMMAND_RANGE(ID_VIEWMODE_LARGEICONS, ID_VIEWMODE_DETAILS, &CFindFilesDlg::OnViewMode)
    ON_COMMAND_RANGE(ID_ARRANGE_NAME, ID_ARRANGE_DATE, &CFindFilesDlg::OnArrange)
    ON_COMMAND(ID_VIEW_REFRESH, &CFindFilesDlg::OnFindNow)
    ON_COMMAND(ID_OPT_CASE, &CFindFilesDlg::OnOptCase)
    ON_COMMAND(ID_OPT_DUPS, &CFindFilesDlg::OnOptDups)
    ON_COMMAND(ID_HELP_ABOUT, &CFindFilesDlg::OnHelpAbout)

    ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_ITEM, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENFOLDER, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_FILE_DELETE_ITEM, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_FILE_MOVETO, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_FILE_PROPERTIES, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPYPATH, &CFindFilesDlg::OnUpdateNeedSelection)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVERESULTS, &CFindFilesDlg::OnUpdateNeedItems)
    ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, &CFindFilesDlg::OnUpdateNeedItems)
    ON_UPDATE_COMMAND_UI(ID_EDIT_INVERT, &CFindFilesDlg::OnUpdateNeedItems)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ARRANGE_NAME, ID_ARRANGE_DATE, &CFindFilesDlg::OnUpdateNeedItems)
    ON_UPDATE_COMMAND_UI_RANGE(ID_DUP_KEEPFIRST, ID_DUP_KEEPSHORTEST, &CFindFilesDlg::OnUpdateNeedDupItems)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEWMODE_LARGEICONS, ID_VIEWMODE_DETAILS, &CFindFilesDlg::OnUpdateViewMode)
    ON_UPDATE_COMMAND_UI(ID_VIEW_REFRESH, &CFindFilesDlg::OnUpdateFindNow)
    ON_UPDATE_COMMAND_UI(ID_OPT_CASE, &CFindFilesDlg::OnUpdateOptCase)
    ON_UPDATE_COMMAND_UI(ID_OPT_DUPS, &CFindFilesDlg::OnUpdateOptDups)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Setup and layout

BOOL CFindFilesDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SHSTOCKICONINFO sii{ sizeof(sii) };
    if (SUCCEEDED(SHGetStockIconInfo(SIID_FIND, SHGSI_ICON | SHGSI_LARGEICON, &sii)))
    {
        SetIcon(m_iconBig = sii.hIcon, TRUE);
    }

    if (SUCCEEDED(SHGetStockIconInfo(SIID_FIND, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
    {
        SetIcon(m_iconSmall = sii.hIcon, FALSE);
    }

    // Tabs host child dialogs; WS_EX_CONTROLPARENT lets Tab/mnemonics reach them.
    m_tab.ModifyStyleEx(0, WS_EX_CONTROLPARENT);
    static const LPCWSTR kTabNames[] = { L"Name && Location", L"Date Modified", L"Advanced", L"Duplicates" };
    static const UINT kPageIds[] = { IDD_PAGE_NAME, IDD_PAGE_DATE, IDD_PAGE_ADV, IDD_PAGE_DUP };
    for (int i = 0; i < 4; i++)
    {
        m_tab.InsertItem(i, kTabNames[i]);
        m_pages[i]->Create(kPageIds[i], &m_tab);
        EnableThemeDialogTexture(m_pages[i]->m_hWnd, ETDT_ENABLETAB);
    }

    // Results list, using the shell's system image list for file icons.
    SHFILEINFOW sfi{};
    HIMAGELIST smallImages = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON));
    HIMAGELIST largeImages = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_LARGEICON));
    ListView_SetImageList(m_list.m_hWnd, smallImages, LVSIL_SMALL);
    ListView_SetImageList(m_list.m_hWnd, largeImages, LVSIL_NORMAL);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);
    SetWindowTheme(m_list.m_hWnd, L"Explorer", nullptr);

    UINT dpi = GetDpiForWindow(m_hWnd);
    auto px = [dpi](int w) { return MulDiv(w, dpi, 96); };
    m_list.InsertColumn(COL_NAME, L"Name", LVCFMT_LEFT, px(200));
    m_list.InsertColumn(COL_FOLDER, L"In Folder", LVCFMT_LEFT, px(260));
    m_list.InsertColumn(COL_SIZE, L"Size", LVCFMT_RIGHT, px(80));
    m_list.InsertColumn(COL_TYPE, L"Type", LVCFMT_LEFT, px(140));
    m_list.InsertColumn(COL_MODIFIED, L"Modified", LVCFMT_LEFT, px(130));

    m_status.Create(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | CCS_BOTTOM, CRect(0, 0, 0, 0), this, IDC_STATUSBAR);

    m_accel = LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_ACCEL));

    CRect wr;
    GetWindowRect(&wr);
    m_minTrack = CSize(wr.Width() * 4 / 5, wr.Height() * 4 / 5);

    Layout();
    ShowPage(0);
    SetSearching(false);

    m_pageName.GetDlgItem(IDC_NAMED)->SetFocus();
    return FALSE;
}

void CFindFilesDlg::Layout()
{
    if (!m_tab.m_hWnd || !m_status.m_hWnd)
    {
        return;
    }

    CRect rc;
    GetClientRect(&rc);

    CRect du(7, 96, 65, 14);    // margin, tab height, button width, button height
    MapDialogRect(&du);
    CRect gapRc(0, 0, 4, 18);   // gap between buttons, top offset of buttons
    MapDialogRect(&gapRc);
    const int margin = du.left, tabH = du.top, btnW = du.right, btnH = du.bottom;

    m_status.SendMessage(WM_SIZE);
    CRect sr;
    m_status.GetWindowRect(&sr);
    const int statusH = sr.Height();

    const int btnX = rc.right - margin - btnW;
    const int tabW = (std::max)(50, btnX - 2 * margin);
    m_tab.MoveWindow(margin, margin, tabW, tabH);

    int y = margin + gapRc.bottom;
    for (UINT id : { IDC_FINDNOW, IDC_STOP, IDC_NEWSEARCH })
    {
        GetDlgItem(id)->MoveWindow(btnX, y, btnW, btnH);
        y += btnH + gapRc.right;
    }

    int animH = (std::min)(btnW, margin + tabH - y);
    GetDlgItem(IDC_ANIM)->MoveWindow(btnX, y + 2, btnW, (std::max)(animH - 2, 8));

    int listTop = margin * 2 + tabH;
    m_list.MoveWindow(0, listTop, rc.Width(), (std::max)(0, static_cast<int>(rc.bottom) - statusH - listTop));

    CRect pr;
    m_tab.GetClientRect(&pr);
    m_tab.AdjustRect(FALSE, &pr);
    for (CSearchPage* page : m_pages)
    {
        page->MoveWindow(&pr);
    }
}

void CFindFilesDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    Layout();
}

void CFindFilesDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    if (m_minTrack.cx > 0)
    {
        lpMMI->ptMinTrackSize.x = m_minTrack.cx;
        lpMMI->ptMinTrackSize.y = m_minTrack.cy;
    }

    CDialog::OnGetMinMaxInfo(lpMMI);
}

void CFindFilesDlg::ShowPage(int index)
{
    for (int i = 0; i < 4; i++)
    {
        m_pages[i]->ShowWindow(i == index ? SW_SHOW : SW_HIDE);
    }
}

void CFindFilesDlg::OnTabChange(NMHDR*, LRESULT* pResult)
{
    ShowPage(m_tab.GetCurSel());
    *pResult = 0;
}

BOOL CFindFilesDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
    {
        OnFindNow();
        return TRUE;
    }

    // Shortcuts like Del, Ctrl+A and Enter only apply to the results list,
    // so they keep working normally inside the edit boxes.
    if ((pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN) &&
        pMsg->hwnd == m_list.m_hWnd && m_accel &&
        ::TranslateAccelerator(m_hWnd, m_accel, pMsg))
    {
        return TRUE;
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CFindFilesDlg::OnOK()
{
    // Enter is routed to the default button (Find Now); never close here.
}

void CFindFilesDlg::OnCancel()
{
    if (m_engine.IsRunning())
    {
        OnStop();
        return;
    }

    ShutdownSearch();
    CDialog::OnCancel();
}

void CFindFilesDlg::OnClose()
{
    ShutdownSearch();
    EndDialog(IDCANCEL);
}

void CFindFilesDlg::OnFileClose()
{
    OnClose();
}

void CFindFilesDlg::OnDestroy()
{
    ShutdownSearch();
    KillTimer(kAnimTimer);
    CDialog::OnDestroy();
    if (m_iconBig)
    {
        DestroyIcon(m_iconBig);
    }

    if (m_iconSmall)
    {
        DestroyIcon(m_iconSmall);
    }

    m_iconBig = m_iconSmall = nullptr;
}

void CFindFilesDlg::ShutdownSearch()
{
    m_engine.Stop();
    m_engine.Wait();

    // Free any payloads still sitting in the queue.
    MSG msg;
    while (::PeekMessage(&msg, m_hWnd, WM_APP_RESULTS, WM_APP_DONE, PM_REMOVE))
    {
        if (msg.message == WM_APP_RESULTS)
        {
            delete reinterpret_cast<std::vector<FileEntry>*>(msg.lParam);
        }
        else if (msg.message == WM_APP_STATUS)
        {
            delete reinterpret_cast<CString*>(msg.lParam);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// Searching

void CFindFilesDlg::OnFindNow()
{
    if (m_engine.IsRunning())
    {
        return;
    }

    SearchCriteria c;
    for (CSearchPage* page : m_pages)
    {
        if (!page->Fill(c))
        {
            return;
        }
    }

    c.matchCase = m_matchCase;
    m_pageName.SaveHistory();

    ClearResults();
    m_dupMode = c.findDups;
    m_list.EnableGroupView(m_dupMode);

    CString named = m_pageName.GetNamedText();
    CString caption = named.IsEmpty() ? CString(L"Find: All Files") : L"Find: Files named " + named;
    if (m_dupMode)
    {
        caption = named.IsEmpty() ? CString(L"Find: Duplicate Files") : L"Find: Duplicate files named " + named;
    }

    SetWindowText(caption);

    if (!m_engine.Start(m_hWnd, c))
    {
        MessageBox(L"Could not start the search thread.", L"Find", MB_OK | MB_ICONERROR);
        return;
    }

    SetSearching(true);
    m_status.SetText(L"Searching...", 0, 0);
}

void CFindFilesDlg::OnStop()
{
    m_engine.Stop();
    GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
    m_status.SetText(L"Stopping...", 0, 0);
}

void CFindFilesDlg::OnNewSearch()
{
    if (m_engine.IsRunning())
    {
        return;
    }

    if (m_list.GetItemCount() > 0 &&
        MessageBox(L"This will clear your current search.", L"Find", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
    {
        return;
    }

    for (CSearchPage* page : m_pages)
    {
        page->Reset();
    }

    ClearResults();
    m_sortCol = -1;
    UpdateSortArrow();
    SetWindowText(L"Find: All Files");
    m_status.SetText(L"", 0, 0);
    m_tab.SetCurSel(0);
    ShowPage(0);
    m_pageName.GetDlgItem(IDC_NAMED)->SetFocus();
}

void CFindFilesDlg::SetSearching(bool searching)
{
    m_searching = searching;
    GetDlgItem(IDC_FINDNOW)->EnableWindow(!searching);
    GetDlgItem(IDC_STOP)->EnableWindow(searching);
    GetDlgItem(IDC_NEWSEARCH)->EnableWindow(!searching);

    if (searching)
    {
        SetTimer(kAnimTimer, 60, nullptr);
    }
    else
    {
        KillTimer(kAnimTimer);
        m_animPhase = 0.0;

        // Keep Enter mapped to Find Now after the buttons re-enable.
        SendMessage(DM_SETDEFID, IDC_FINDNOW);
    }
    GetDlgItem(IDC_ANIM)->Invalidate(FALSE);
}

void CFindFilesDlg::ClearResults()
{
    m_list.DeleteAllItems();
    m_list.RemoveAllGroups();
    m_results.clear();
    m_groupCount = 0;
}

LRESULT CFindFilesDlg::OnSearchResults(WPARAM, LPARAM lParam)
{
    std::unique_ptr<std::vector<FileEntry>> batch(reinterpret_cast<std::vector<FileEntry>*>(lParam));
    m_list.SetRedraw(FALSE);
    for (FileEntry& e : *batch)
    {
        if (m_dupMode)
        {
            EnsureGroup(e.group);
        }

        m_results.push_back(std::move(e));
        InsertEntry(m_results.size() - 1);
    }

    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
    return 0;
}

LRESULT CFindFilesDlg::OnSearchStatus(WPARAM, LPARAM lParam)
{
    std::unique_ptr<CString> text(reinterpret_cast<CString*>(lParam));
    if (m_engine.IsRunning())
    {
        m_status.SetText(*text, 0, 0);
    }

    return 0;
}

LRESULT CFindFilesDlg::OnSearchDone(WPARAM wParam, LPARAM)
{
    m_engine.Wait();
    m_lastStopped = wParam != 0;
    SetSearching(false);
    UpdateGroupHeaders();
    if (m_sortCol >= 0)
    {
        SortList();
    }

    UpdateStatus();
    if (m_list.GetItemCount() > 0 && GetFocus() == GetDlgItem(IDC_STOP))
    {
        m_list.SetFocus();
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Result list

const CFindFilesDlg::TypeInfo& CFindFilesDlg::GetTypeInfo(const FileEntry& e)
{
    CString key;
    if (e.IsFolder())
    {
        key = L"\\folder";
    }
    else
    {
        key = PathFindExtensionW(e.name);
        key.MakeLower();
        if (key.IsEmpty())
        {
            key = L"\\none";
        }
    }

    auto it = m_typeCache.find(key);
    if (it != m_typeCache.end())
    {
        return it->second;
    }

    SHFILEINFOW sfi{};
    SHGetFileInfoW(e.name, e.IsFolder() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_TYPENAME);
    TypeInfo ti;
    ti.type = sfi.szTypeName;
    ti.icon = sfi.iIcon;
    if (ti.type.IsEmpty())
    {
        ti.type = e.IsFolder() ? L"File folder" : L"File";
    }

    return m_typeCache.emplace(key, ti).first->second;
}

void CFindFilesDlg::EnsureGroup(int group)
{
    while (m_groupCount <= group)
    {
        LVGROUP lg{ sizeof(lg) };
        lg.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
        lg.iGroupId = m_groupCount + 1;
        lg.pszHeader = const_cast<LPWSTR>(L"");
        lg.state = lg.stateMask = LVGS_COLLAPSIBLE;
        m_list.InsertGroup(-1, &lg);
        ++m_groupCount;
    }
}

void CFindFilesDlg::InsertEntry(size_t index)
{
    FileEntry& e = m_results[index];
    const TypeInfo& ti = GetTypeInfo(e);
    e.type = ti.type;

    LVITEM it{};
    it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    it.iItem = m_list.GetItemCount();
    it.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(e.name));
    it.iImage = ti.icon;
    it.lParam = static_cast<LPARAM>(index);

    if (m_dupMode)
    {
        it.mask |= LVIF_GROUPID;
        it.iGroupId = e.group + 1;
    }

    int n = m_list.InsertItem(&it);
    m_list.SetItemText(n, COL_FOLDER, e.folder);
    m_list.SetItemText(n, COL_SIZE, FormatSize(e));
    m_list.SetItemText(n, COL_TYPE, e.type);
    m_list.SetItemText(n, COL_MODIFIED, FormatTime(e.modified));
}

void CFindFilesDlg::UpdateGroupHeaders()
{
    if (!m_dupMode)
    {
        return;
    }

    std::map<int, std::vector<int>> byGroup;
    for (int i = 0; i < m_list.GetItemCount(); i++)
    {
        byGroup[EntryAt(i).group].push_back(i);
    }

    // A set with one file left is no longer a duplicate; drop it.
    std::vector<int> singles;
    for (auto& kv : byGroup)
    {
        if (kv.second.size() < 2)
        {
            singles.insert(singles.end(), kv.second.begin(), kv.second.end());
        }
    }

    std::sort(singles.rbegin(), singles.rend());
    for (int item : singles)
    {
        m_list.DeleteItem(item);
    }

    int setNo = 0;
    for (int g = 0; g < m_groupCount; g++)
    {
        auto it = byGroup.find(g);
        size_t count = it == byGroup.end() ? 0 : it->second.size();
        if (count < 2)
        {
            if (m_list.HasGroup(g + 1))
            {
                m_list.RemoveGroup(g + 1);
            }

            continue;
        }

        const FileEntry& first = EntryAt(it->second.front());
        CString header;
        header.Format(L"Set %d  \u2014  %d copies of %s each  (%s reclaimable)",
            ++setNo, static_cast<int>(count), (LPCWSTR)FormatBytes(first.size),
            (LPCWSTR)FormatBytes(first.size * (count - 1)));

        LVGROUP lg{ sizeof(lg) };
        lg.mask = LVGF_HEADER;
        lg.pszHeader = const_cast<LPWSTR>(static_cast<LPCWSTR>(header));
        m_list.SetGroupInfo(g + 1, &lg);
    }
}

void CFindFilesDlg::UpdateStatus()
{
    CString text;
    int n = m_list.GetItemCount();
    if (m_dupMode)
    {
        std::map<int, std::pair<int, ULONGLONG>> sets;   // group -> (count, size)
        for (int i = 0; i < n; i++)
        {
            const FileEntry& e = EntryAt(i);
            sets[e.group].first++;
            sets[e.group].second = e.size;
        }

        ULONGLONG wasted = 0;
        for (auto& kv : sets)
        {
            wasted += kv.second.second * (kv.second.first - 1);
        }

        if (n == 0)
        {
            text = L"No duplicate files found";
        }
        else
        {
            text.Format(L"%s duplicate file(s) in %s set(s)  \u2014  %s can be reclaimed",
                (LPCWSTR)FormatCount(n), (LPCWSTR)FormatCount(sets.size()), (LPCWSTR)FormatBytes(wasted));
        }
    }
    else
    {
        text.Format(L"%s file(s) found", (LPCWSTR)FormatCount(n));
    }

    if (m_lastStopped)
    {
        text += L"  (search stopped)";
    }

    m_status.SetText(text, 0, 0);
}

int CALLBACK CFindFilesDlg::CompareItems(LPARAM a, LPARAM b, LPARAM param)
{
    auto* self = reinterpret_cast<CFindFilesDlg*>(param);
    const FileEntry& x = self->m_results[a];
    const FileEntry& y = self->m_results[b];

    int r = 0;
    switch (self->m_sortCol)
    {
    case COL_NAME:     r = StrCmpLogicalW(x.name, y.name); break;
    case COL_FOLDER:   r = StrCmpLogicalW(x.folder, y.folder); break;
    case COL_SIZE:     r = x.size < y.size ? -1 : x.size > y.size ? 1 : 0; break;
    case COL_TYPE:     r = x.type.CompareNoCase(y.type); break;
    case COL_MODIFIED: r = CompareFileTime(&x.modified, &y.modified); break;
    }

    if (r == 0)
    {
        r = StrCmpLogicalW(x.FullPath(), y.FullPath());
    }

    return self->m_sortAsc ? r : -r;
}

void CFindFilesDlg::SortList()
{
    if (m_sortCol < 0)
    {
        return;
    }

    m_list.SortItems(&CFindFilesDlg::CompareItems, reinterpret_cast<DWORD_PTR>(this));
    UpdateSortArrow();
}

void CFindFilesDlg::UpdateSortArrow()
{
    CHeaderCtrl* header = m_list.GetHeaderCtrl();
    for (int i = 0; i < header->GetItemCount(); i++)
    {
        HDITEM hi{};
        hi.mask = HDI_FORMAT;
        header->GetItem(i, &hi);
        hi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

        if (i == m_sortCol)
        {
            hi.fmt |= m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN;
        }

        header->SetItem(i, &hi);
    }
}

void CFindFilesDlg::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* nm = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if (nm->iSubItem == m_sortCol)
    {
        m_sortAsc = !m_sortAsc;
    }
    else
    {
        m_sortCol = nm->iSubItem;
        m_sortAsc = true;
    }

    SortList();
    *pResult = 0;
}

void CFindFilesDlg::OnListDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (reinterpret_cast<NMITEMACTIVATE*>(pNMHDR)->iItem >= 0)
    {
        OnFileOpen();
    }

    *pResult = 0;
}

std::vector<int> CFindFilesDlg::GetSelectedItems() const
{
    std::vector<int> items;
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos)
    {
        items.push_back(m_list.GetNextSelectedItem(pos));
    }

    return items;
}

void CFindFilesDlg::RemoveListItems(std::vector<int> items)
{
    std::sort(items.rbegin(), items.rend());
    m_list.SetRedraw(FALSE);
    for (int item : items)
    {
        m_list.DeleteItem(item);
    }

    UpdateGroupHeaders();
    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
    UpdateStatus();
}

void CFindFilesDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
    if (pWnd != &m_list)
    {
        CDialog::OnContextMenu(pWnd, point);
        return;
    }

    if (point.x == -1 && point.y == -1)
    {
        // Keyboard invocation: show at the focused item.
        int item = m_list.GetNextItem(-1, LVNI_FOCUSED);
        CRect r;
        if (item >= 0 && m_list.GetItemRect(item, &r, LVIR_LABEL))
        {
            point = CPoint(r.left, r.bottom);
        }
        else
        {
            point = CPoint(0, 0);
        }

        m_list.ClientToScreen(&point);
    }

    CMenu menu;
    menu.LoadMenu(IDR_CONTEXTMENU);
    CMenu* popup = menu.GetSubMenu(0);
    popup->SetDefaultItem(ID_FILE_OPEN_ITEM);
    popup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

/////////////////////////////////////////////////////////////////////////////
// File commands

void CFindFilesDlg::OnFileOpen()
{
    auto sel = GetSelectedItems();
    if (sel.size() > 10 &&
        MessageBox(L"Open all " + FormatCount(sel.size()) + L" selected items?", L"Find", MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    for (int item : sel)
    {
        const FileEntry& e = EntryAt(item);
        ShellExecuteW(m_hWnd, nullptr, e.FullPath(), nullptr, e.folder, SW_SHOWNORMAL);
    }
}

void CFindFilesDlg::OnFileOpenFolder()
{
    for (int item : GetSelectedItems())
    {
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(EntryAt(item).FullPath());
        if (pidl)
        {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
        }
        break;  // one Explorer window is plenty
    }
}

void CFindFilesDlg::OnFileDelete()
{
    auto sel = GetSelectedItems();
    if (sel.empty() || m_searching)
    {
        return;
    }

    size_t permanent = 0;
    for (int item : sel)
    {
        if (!HasRecycleBin(EntryAt(item).FullPath()))
        {
            ++permanent;
        }
    }

    CString msg;
    if (permanent == 0)
    {
        if (sel.size() == 1)
        {
            msg.Format(L"Are you sure you want to send '%s' to the Recycle Bin?", (LPCWSTR)EntryAt(sel[0]).name);
        }
        else
        {
            msg.Format(L"Are you sure you want to send these %s items to the Recycle Bin?", (LPCWSTR)FormatCount(sel.size()));
        }
    }
    else if (permanent == sel.size())
    {
        if (sel.size() == 1)
        {
            msg.Format(L"'%s' is on a network or removable drive, which has no Recycle Bin.\n\n"
                L"Are you sure you want to permanently delete it? This cannot be undone.", (LPCWSTR)EntryAt(sel[0]).name);
        }
        else
        {
            msg.Format(L"These %s items are on a network or removable drive, which has no Recycle Bin.\n\n"
                L"Are you sure you want to permanently delete them? This cannot be undone.", (LPCWSTR)FormatCount(sel.size()));
        }
    }
    else
    {
        msg.Format(L"Are you sure you want to delete these %s items?\n\n"
            L"%s of them are on a network or removable drive and will be permanently deleted. "
            L"The rest will go to the Recycle Bin.",
            (LPCWSTR)FormatCount(sel.size()), (LPCWSTR)FormatCount(permanent));
    }

    if (m_dupMode)
    {
        // Warn if every copy of some file is about to go.
        std::map<int, int> total, chosen;
        for (int i = 0; i < m_list.GetItemCount(); i++)
        {
            total[EntryAt(i).group]++;
        }

        for (int item : sel)
        {
            chosen[EntryAt(item).group]++;
        }

        int wholeSets = 0;
        for (auto& kv : chosen)
        {
            if (kv.second == total[kv.first])
            {
                ++wholeSets;
            }
        }

        if (wholeSets > 0)
        {
            CString warn;
            warn.Format(L"\n\nWarning: the selection includes every copy in %d duplicate set(s), "
                L"so no copy of those files would be kept.", wholeSets);
            msg += warn;
        }
    }

    LPCWSTR title = permanent > 0 ? L"Confirm Permanent Delete" : L"Confirm File Delete";
    if (MessageBox(msg, title, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }

    std::wstring from;
    for (int item : sel)
    {
        from += (LPCWSTR)EntryAt(item).FullPath();
        from.push_back(L'\0');
    }

    from.push_back(L'\0');

    SHFILEOPSTRUCTW op{};
    op.hwnd = m_hWnd;
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    // The prompt above already covered permanent deletes. Otherwise keep the
    // shell's own warning for files the Recycle Bin turns away (e.g. too large).
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | (permanent == 0 ? FOF_WANTNUKEWARNING : 0);
    SHFileOperationW(&op);

    std::vector<int> gone;
    for (int item : sel)
    {
        if (!FileExists(EntryAt(item).FullPath()))
        {
            gone.push_back(item);
        }
    }

    RemoveListItems(gone);
}

void CFindFilesDlg::OnFileMoveTo()
{
    auto sel = GetSelectedItems();
    if (sel.empty() || m_searching)
    {
        return;
    }

    CFolderPickerDialog dlg(nullptr, 0, this);
    dlg.m_ofn.lpstrTitle = L"Move selected files to";
    if (dlg.DoModal() != IDOK)
    {
        return;
    }

    CString dest = dlg.GetPathName();
    if (dest.IsEmpty())
    {
        return;
    }

    if (dest[dest.GetLength() - 1] != L'\\')
    {
        dest += L'\\';
    }

    std::vector<int> moved;
    int failed = 0;
    for (int item : sel)
    {
        const FileEntry& e = EntryAt(item);
        CString target = dest + e.name;

        // Never overwrite: add " (2)", " (3)"... before the extension.
        for (int n = 2; FileExists(target); n++)
        {
            CString base = e.name;
            LPCWSTR ext = PathFindExtensionW(e.name);
            base.Truncate(static_cast<int>(ext - static_cast<LPCWSTR>(e.name)));
            target.Format(L"%s%s (%d)%s", (LPCWSTR)dest, (LPCWSTR)base, n, ext);
        }

        if (MoveFileExW(ExtendedPath(e.FullPath()), ExtendedPath(target), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        {
            moved.push_back(item);
        }
        else
        {
            ++failed;
        }
    }

    RemoveListItems(moved);

    if (failed > 0)
    {
        CString msg;
        msg.Format(L"%d item(s) could not be moved. They may be in use or you may not have permission.", failed);
        MessageBox(msg, L"Move", MB_OK | MB_ICONWARNING);
    }
}

void CFindFilesDlg::OnFileProperties()
{
    auto sel = GetSelectedItems();
    if (!sel.empty())
    {
        SHObjectProperties(m_hWnd, SHOP_FILEPATH, EntryAt(sel[0]).FullPath(), nullptr);
    }
}

void CFindFilesDlg::OnFileSaveResults()
{
    CFileDialog dlg(FALSE, L"csv", L"Find Results.csv", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        L"CSV Files (*.csv)|*.csv|Text Files (*.txt)|*.txt||", this);
    if (dlg.DoModal() != IDOK)
    {
        return;
    }

    CString text = m_dupMode ? L"Set,Name,In Folder,Size (bytes),Type,Modified,SHA-256\r\n"
        : L"Name,In Folder,Size (bytes),Type,Modified\r\n";
    for (int i = 0; i < m_list.GetItemCount(); i++)
    {
        const FileEntry& e = EntryAt(i);
        CString line;
        if (m_dupMode)
        {
            line.AppendFormat(L"%d,", e.group + 1);
        }

        line.AppendFormat(L"%s,%s,%llu,%s,%s", (LPCWSTR)CsvField(e.name), (LPCWSTR)CsvField(e.folder), e.size,
            (LPCWSTR)CsvField(e.type), (LPCWSTR)CsvField(FormatTime(e.modified)));
        if (m_dupMode)
        {
            line += L"," + e.HashHex();
        }

        text += line + L"\r\n";
    }

    CFile file;
    CFileException ex;
    if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite, &ex))
    {
        WCHAR err[512];
        ex.GetErrorMessage(err, _countof(err));
        MessageBox(err, L"Save Results", MB_OK | MB_ICONERROR);
        return;
    }

    static const BYTE kBom[] = { 0xEF, 0xBB, 0xBF };
    CStringA utf8(CW2A(text, CP_UTF8));
    file.Write(kBom, sizeof(kBom));
    file.Write(static_cast<LPCSTR>(utf8), utf8.GetLength());
    file.Close();
}

/////////////////////////////////////////////////////////////////////////////
// Edit / View / Options / Help

void CFindFilesDlg::OnEditCopyPath()
{
    CString text;
    for (int item : GetSelectedItems())
    {
        text += EntryAt(item).FullPath() + L"\r\n";
    }

    text.TrimRight();
    if (text.IsEmpty() || !OpenClipboard())
    {
        return;
    }

    EmptyClipboard();
    size_t bytes = (text.GetLength() + 1) * sizeof(WCHAR);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem)
    {
        memcpy(GlobalLock(mem), static_cast<LPCWSTR>(text), bytes);
        GlobalUnlock(mem);
        if (!SetClipboardData(CF_UNICODETEXT, mem))
        {
            GlobalFree(mem);
        }
    }

    CloseClipboard();
}

void CFindFilesDlg::OnEditSelectAll()
{
    m_list.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
    m_list.SetFocus();
}

void CFindFilesDlg::OnEditInvert()
{
    for (int i = 0; i < m_list.GetItemCount(); i++)
    {
        m_list.SetItemState(i, m_list.GetItemState(i, LVIS_SELECTED) ^ LVIS_SELECTED, LVIS_SELECTED);
    }

    m_list.SetFocus();
}

void CFindFilesDlg::OnSelectDuplicates(UINT id)
{
    if (!m_dupMode)
    {
        return;
    }

    std::map<int, std::vector<int>> byGroup;
    for (int i = 0; i < m_list.GetItemCount(); i++)
    {
        byGroup[EntryAt(i).group].push_back(i);
    }

    m_list.SetItemState(-1, 0, LVIS_SELECTED);
    int selected = 0;
    ULONGLONG bytes = 0;

    for (auto& kv : byGroup)
    {
        const std::vector<int>& items = kv.second;
        int keep = items.front();
        for (int item : items)
        {
            const FileEntry& e = EntryAt(item);
            const FileEntry& k = EntryAt(keep);
            switch (id)
            {
            case ID_DUP_KEEPNEWEST:
                if (CompareFileTime(&e.modified, &k.modified) > 0)
                {
                    keep = item;
                }
                break;
            case ID_DUP_KEEPOLDEST:
                if (CompareFileTime(&e.modified, &k.modified) < 0)
                {
                    keep = item;
                }
                break;
            case ID_DUP_KEEPSHORTEST:
                if (e.FullPath().GetLength() < k.FullPath().GetLength())
                {
                    keep = item;
                }
                break;
            }
        }

        for (int item : items)
        {
            if (item == keep)
            {
                continue;
            }

            m_list.SetItemState(item, LVIS_SELECTED, LVIS_SELECTED);
            ++selected;
            bytes += EntryAt(item).size;
        }
    }

    m_list.SetFocus();

    CString text;
    text.Format(L"%s file(s) selected (%s). One copy in each set is kept. Press Delete to remove the selection.",
        (LPCWSTR)FormatCount(selected), (LPCWSTR)FormatBytes(bytes));
    m_status.SetText(text, 0, 0);
}

void CFindFilesDlg::OnViewMode(UINT id)
{
    static const DWORD kViews[] = { LV_VIEW_ICON, LV_VIEW_SMALLICON, LV_VIEW_LIST, LV_VIEW_DETAILS };
    m_viewMode = id;
    m_list.SetView(kViews[id - ID_VIEWMODE_LARGEICONS]);
}

void CFindFilesDlg::OnArrange(UINT id)
{
    m_sortCol = static_cast<int>(id - ID_ARRANGE_NAME);
    m_sortAsc = true;
    SortList();
}

void CFindFilesDlg::OnOptCase()
{
    m_matchCase = !m_matchCase;
}

void CFindFilesDlg::OnOptDups()
{
    m_pageDup.SetDupEnabled(!m_pageDup.IsDupEnabled());
}

void CFindFilesDlg::OnHelpAbout()
{
    MessageBox(L"Find Files\nVersion 1.0\n\n"
        L"Search for files by name, location, date, type, size and contents, "
        L"and find duplicate files by name, size or SHA-256 content hash.\n\n"
        L"Built with MFC.",
        L"About Find Files", MB_OK | MB_ICONINFORMATION);
}

void CFindFilesDlg::OnUpdateNeedSelection(CCmdUI* pCmdUI)
{
    bool busy = m_searching && (pCmdUI->m_nID == ID_FILE_DELETE_ITEM || pCmdUI->m_nID == ID_FILE_MOVETO);
    pCmdUI->Enable(m_list.GetSelectedCount() > 0 && !busy);
}

void CFindFilesDlg::OnUpdateNeedItems(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetItemCount() > 0);
}

void CFindFilesDlg::OnUpdateNeedDupItems(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_dupMode && !m_searching && m_list.GetItemCount() > 0);
}

void CFindFilesDlg::OnUpdateViewMode(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(pCmdUI->m_nID == m_viewMode);
}

void CFindFilesDlg::OnUpdateOptCase(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_matchCase);
}

void CFindFilesDlg::OnUpdateOptDups(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_pageDup.IsDupEnabled());
}

void CFindFilesDlg::OnUpdateFindNow(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_searching);
}

// Dialogs don't get ON_UPDATE_COMMAND_UI for menus automatically; route it here.
void CFindFilesDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
    if (bSysMenu)
    {
        return;
    }

    CCmdUI state;
    state.m_pMenu = pPopupMenu;
    state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
    for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; state.m_nIndex++)
    {
        state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
        if (state.m_nID == 0)
        {
            continue;   // separator
        }

        if (state.m_nID == static_cast<UINT>(-1))
        {
            // Sub-popup: enable it based on its first command.
            state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
            if (!state.m_pSubMenu ||
                (state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
                state.m_nID == static_cast<UINT>(-1))
            {
                continue;
            }

            state.DoUpdate(this, TRUE);
        }
        else
        {
            state.m_pSubMenu = nullptr;
            state.DoUpdate(this, FALSE);
        }

        UINT count = pPopupMenu->GetMenuItemCount();
        if (count < state.m_nIndexMax)
        {
            state.m_nIndex -= (state.m_nIndexMax - count);
            while (state.m_nIndex < count && pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID)
                state.m_nIndex++;
        }

        state.m_nIndexMax = count;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Magnifier animation

void CFindFilesDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kAnimTimer)
    {
        m_animPhase += 0.35;
        GetDlgItem(IDC_ANIM)->Invalidate(FALSE);
        return;
    }

    CDialog::OnTimer(nIDEvent);
}

void CFindFilesDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
    if (nIDCtl != IDC_ANIM)
    {
        CDialog::OnDrawItem(nIDCtl, lpDIS);
        return;
    }

    CRect rc(lpDIS->rcItem);
    CDC memDC;
    CBitmap bmp;
    CDC* target = CDC::FromHandle(lpDIS->hDC);
    memDC.CreateCompatibleDC(target);
    bmp.CreateCompatibleBitmap(target, rc.Width(), rc.Height());
    CBitmap* oldBmp = memDC.SelectObject(&bmp);
    memDC.FillSolidRect(0, 0, rc.Width(), rc.Height(), GetSysColor(COLOR_BTNFACE));

    const double s = (std::min)(rc.Width(), rc.Height());
    const double r = s * 0.24;
    double cx = rc.Width() / 2.0 - s * 0.1;
    double cy = rc.Height() / 2.0 - s * 0.1;

    if (m_searching)
    {
        // Sweep the lens in a small circle, like the classic Find animation.
        cx += cos(m_animPhase) * s * 0.1;
        cy += sin(m_animPhase) * s * 0.1;
    }

    LOGBRUSH lb{ BS_SOLID, RGB(110, 70, 30), 0 };
    CPen handlePen;
    handlePen.Attach(ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND,
        (std::max)(3, static_cast<int>(s / 7)), &lb, 0, nullptr));
    CPen rimPen(PS_SOLID, (std::max)(2, static_cast<int>(s / 14)), RGB(60, 60, 60));
    CBrush glass(RGB(205, 232, 255));

    CPen* oldPen = memDC.SelectObject(&handlePen);
    const double hx = cx + r * 0.72, hy = cy + r * 0.72;
    memDC.MoveTo(static_cast<int>(hx), static_cast<int>(hy));
    memDC.LineTo(static_cast<int>(hx + s * 0.26), static_cast<int>(hy + s * 0.26));

    memDC.SelectObject(&rimPen);
    CBrush* oldBrush = memDC.SelectObject(&glass);
    memDC.Ellipse(static_cast<int>(cx - r), static_cast<int>(cy - r), static_cast<int>(cx + r), static_cast<int>(cy + r));

    // Glint on the glass.
    CPen glint(PS_SOLID, (std::max)(1, static_cast<int>(s / 20)), RGB(255, 255, 255));
    memDC.SelectObject(&glint);
    memDC.Arc(static_cast<int>(cx - r * 0.6), static_cast<int>(cy - r * 0.6),
        static_cast<int>(cx + r * 0.6), static_cast<int>(cy + r * 0.6),
        static_cast<int>(cx), static_cast<int>(cy - r), static_cast<int>(cx - r), static_cast<int>(cy));

    memDC.SelectObject(oldBrush);
    memDC.SelectObject(oldPen);
    target->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(oldBmp);
}
