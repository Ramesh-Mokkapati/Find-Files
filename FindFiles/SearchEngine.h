#pragma once

// Messages posted from the search thread to the dialog.
#define WM_APP_RESULTS  (WM_APP + 1)    // lParam = std::vector<FileEntry>* (receiver deletes)
#define WM_APP_STATUS   (WM_APP + 2)    // lParam = CString* (receiver deletes)
#define WM_APP_DONE     (WM_APP + 3)    // wParam = stopped, lParam = duplicate set count

enum SizeOp { SIZE_ANY, SIZE_ATLEAST, SIZE_ATMOST };
enum DupMode { DUP_NAME_SIZE, DUP_CONTENT, DUP_NAME_SIZE_CONTENT };

struct FileEntry
{
    CString   name;
    CString   folder;           // no trailing backslash except drive roots
    CString   type;             // filled in by the UI thread
    ULONGLONG size = 0;
    FILETIME  modified{};
    DWORD     attrs = 0;
    int       group = -1;       // duplicate set index, -1 when not in duplicate mode
    bool      hasHash = false;
    BYTE      hash[32]{};

    bool IsFolder() const { return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    CString FullPath() const;
    CString HashHex() const;
};

struct SearchCriteria
{
    std::vector<CString> patterns;      // PathMatchSpec patterns; empty = everything
    std::vector<CString> roots;         // each with a trailing backslash
    bool subfolders = true;

    bool useDate = false;
    ULONGLONG dateFrom = 0;             // UTC FILETIME as 64-bit
    ULONGLONG dateTo = ULLONG_MAX;

    std::vector<CString> extensions;    // lower case, no dot; empty = all
    bool includeFolders = true;
    CString containing;
    bool matchCase = false;
    int sizeOp = SIZE_ANY;
    ULONGLONG sizeBytes = 0;
    bool includeHidden = false;

    bool findDups = false;
    int dupMode = DUP_CONTENT;
    ULONGLONG dupMinSize = 0;
    bool skipEmpty = true;
};

class CSearchEngine
{
public:
    ~CSearchEngine();

    bool Start(HWND hwndNotify, const SearchCriteria& criteria);
    void Stop() { m_stop = true; }
    void Wait();
    bool IsRunning() const { return m_running; }

private:
    using Bucket = std::vector<size_t>;

    static UINT ThreadProc(LPVOID param);
    void Run();
    void Walk(const CString& dir);
    bool Matches(const WIN32_FIND_DATAW& fd, const CString& dir);
    bool FileContains(const CString& path);
    bool HashFile(const CString& path, ULONGLONG limit, BYTE out[32]);
    int  FindDuplicates();
    std::vector<Bucket> SplitByHash(std::vector<Bucket>& in, ULONGLONG limit, bool fullPass);
    void Emit(FileEntry&& e);
    void Flush(bool force);
    void PostStatus(const CString& text, bool force);

    HWND m_hwnd = nullptr;
    SearchCriteria m_c;
    CWinThread* m_thread = nullptr;
    std::atomic<bool> m_stop{ false };
    std::atomic<bool> m_running{ false };

    std::vector<FileEntry> m_pending;   // batch waiting to be posted
    std::vector<FileEntry> m_all;       // everything found (duplicate mode)
    ULONGLONG m_lastFlush = 0, m_lastStatus = 0;
    ULONGLONG m_found = 0, m_scanned = 0;
    size_t m_hashDone = 0, m_hashTotal = 0;

    std::vector<BYTE> m_needleA, m_needleW;   // UTF-8 and UTF-16LE forms of "containing"
    std::vector<BYTE> m_buf;
    BCRYPT_ALG_HANDLE m_alg = nullptr;
};

CString FormatCount(ULONGLONG n);

// Adds the \\?\ prefix to full paths that are too long for the classic
// MAX_PATH limit, so file APIs still accept them.
CString ExtendedPath(const CString& path);
