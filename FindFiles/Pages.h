#pragma once

#include "resource.h"
#include "SearchEngine.h"

// Base for the child dialogs hosted inside the tab control.
class CSearchPage : public CDialog
{
public:
    explicit CSearchPage(UINT idd) : CDialog(idd) {}
    virtual bool Fill(SearchCriteria& c) = 0;   // false = validation failed
    virtual void Reset() = 0;

protected:
    void OnOK() override {}
    void OnCancel() override {}
};

class CNamePage : public CSearchPage
{
public:
    CNamePage() : CSearchPage(IDD_PAGE_NAME) {}
    bool Fill(SearchCriteria& c) override;
    void Reset() override;
    void SaveHistory();
    CString GetNamedText() const;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnBrowse();
    DECLARE_MESSAGE_MAP()

private:
    CComboBox m_named;
    CComboBox m_lookIn;
};

class CDatePage : public CSearchPage
{
public:
    CDatePage() : CSearchPage(IDD_PAGE_DATE) {}
    bool Fill(SearchCriteria& c) override;
    void Reset() override;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnModeChanged(UINT id);
    DECLARE_MESSAGE_MAP()

private:
    void EnableControls();
    CDateTimeCtrl m_from;
    CDateTimeCtrl m_to;
    CSpinButtonCtrl m_monthsSpin;
    CSpinButtonCtrl m_daysSpin;
};

class CAdvPage : public CSearchPage
{
public:
    CAdvPage() : CSearchPage(IDD_PAGE_ADV) {}
    bool Fill(SearchCriteria& c) override;
    void Reset() override;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnSizeOpChanged();
    DECLARE_MESSAGE_MAP()

private:
    CComboBox m_type;
    CComboBox m_sizeOp;
    CSpinButtonCtrl m_sizeSpin;
};

class CDupPage : public CSearchPage
{
public:
    CDupPage() : CSearchPage(IDD_PAGE_DUP) {}
    bool Fill(SearchCriteria& c) override;
    void Reset() override;
    bool IsDupEnabled() const;
    void SetDupEnabled(bool enable);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnDupsClicked();
    DECLARE_MESSAGE_MAP()

private:
    void EnableControls();
};
