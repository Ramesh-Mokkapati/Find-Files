#pragma once

#include "Pages.h"
#include "SearchEngine.h"

class CFindFilesDlg : public CDialog
{
public:
    explicit CFindFilesDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_FINDFILES };

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    void OnOK() override;
    void OnCancel() override;

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnClose();
    afx_msg void OnDestroy();

    afx_msg void OnTabChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListDblClk(NMHDR* pNMHDR, LRESULT* pResult);

    afx_msg void OnFindNow();
    afx_msg void OnStop();
    afx_msg void OnNewSearch();

    afx_msg LRESULT OnSearchResults(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSearchStatus(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSearchDone(WPARAM wParam, LPARAM lParam);

    afx_msg void OnFileOpen();
    afx_msg void OnFileOpenFolder();
    afx_msg void OnFileDelete();
    afx_msg void OnFileMoveTo();
    afx_msg void OnFileProperties();
    afx_msg void OnFileSaveResults();
    afx_msg void OnFileClose();
    afx_msg void OnEditCopyPath();
    afx_msg void OnEditSelectAll();
    afx_msg void OnEditInvert();
    afx_msg void OnSelectDuplicates(UINT id);
    afx_msg void OnViewMode(UINT id);
    afx_msg void OnArrange(UINT id);
    afx_msg void OnOptCase();
    afx_msg void OnOptDups();
    afx_msg void OnHelpAbout();

    afx_msg void OnUpdateNeedSelection(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNeedItems(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNeedDupItems(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewMode(CCmdUI* pCmdUI);
    afx_msg void OnUpdateOptCase(CCmdUI* pCmdUI);
    afx_msg void OnUpdateOptDups(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFindNow(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()

private:
    struct TypeInfo
    {
        CString type;
        int icon = 0;
    };

    void Layout();
    void ShowPage(int index);
    void SetSearching(bool searching);
    void ClearResults();
    void InsertEntry(size_t index);
    void EnsureGroup(int group);
    const TypeInfo& GetTypeInfo(const FileEntry& e);
    void UpdateGroupHeaders();
    void UpdateStatus();
    void SortList();
    void UpdateSortArrow();
    std::vector<int> GetSelectedItems() const;
    void RemoveListItems(std::vector<int> items);
    void ShutdownSearch();
    const FileEntry& EntryAt(int item) const { return m_results[m_list.GetItemData(item)]; }
    static int CALLBACK CompareItems(LPARAM a, LPARAM b, LPARAM param);

    CTabCtrl m_tab;
    CListCtrl m_list;
    CStatusBarCtrl m_status;
    CNamePage m_pageName;
    CDatePage m_pageDate;
    CAdvPage m_pageAdv;
    CDupPage m_pageDup;
    CSearchPage* m_pages[4];

    CSearchEngine m_engine;
    std::vector<FileEntry> m_results;
    std::map<CString, TypeInfo> m_typeCache;
    bool m_dupMode = false;
    bool m_searching = false;
    bool m_lastStopped = false;
    int m_groupCount = 0;
    int m_sortCol = -1;
    bool m_sortAsc = true;
    bool m_matchCase = false;
    UINT m_viewMode = ID_VIEWMODE_DETAILS;
    double m_animPhase = 0.0;
    HACCEL m_accel = nullptr;
    HICON m_iconBig = nullptr;
    HICON m_iconSmall = nullptr;
    CSize m_minTrack;
};
