#include "pch.h"
#include "SearchEngine.h"

namespace
{
    const DWORD     kReadChunk = 1 << 20;     // 1 MB reads
    const ULONGLONG kPrefixBytes = 64 * 1024;   // quick pre-hash before full hash
    const size_t    kBatchSize = 256;
    const ULONGLONG kFlushMs = 200;
    const ULONGLONG kStatusMs = 150;

    // Files whose data isn't on the disk: OneDrive-style cloud placeholders
    // and HSM-archived files. Reading them triggers a download or recall.
    const DWORD kNotLocalAttrs = FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_OFFLINE;

    ULONGLONG ToU64(const FILETIME& ft)
    {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    BYTE FoldAscii(BYTE b)
    {
        return (b >= 'A' && b <= 'Z') ? static_cast<BYTE>(b + ('a' - 'A')) : b;
    }

    bool ContainsBytes(const BYTE* p, size_t n, const std::vector<BYTE>& needle)
    {
        if (needle.empty() || n < needle.size())
        {
            return false;
        }

        return std::search(p, p + n, needle.begin(), needle.end()) != p + n;
    }

    HANDLE OpenForRead(const CString& path)
    {
        return CreateFileW(ExtendedPath(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    }
}

CString ExtendedPath(const CString& path)
{
    if (path.GetLength() < MAX_PATH - 12 || path.Left(4) == L"\\\\?\\")
    {
        return path;
    }

    if (path.Left(2) == L"\\\\")
    {
        return L"\\\\?\\UNC\\" + path.Mid(2);
    }

    return L"\\\\?\\" + path;
}

CString FormatCount(ULONGLONG n)
{
    CString digits;
    digits.Format(L"%llu", n);
    CString out;
    int len = digits.GetLength();
    for (int i = 0; i < len; i++)
    {
        if (i > 0 && (len - i) % 3 == 0)
        {
            out += L',';
        }

        out += digits[i];
    }

    return out;
}

CString FileEntry::FullPath() const
{
    CString p = folder;
    if (!p.IsEmpty() && p[p.GetLength() - 1] != L'\\')
    {
        p += L'\\';
    }

    return p + name;
}

CString FileEntry::HashHex() const
{
    if (!hasHash)
    {
        return CString();
    }

    CString s;
    for (BYTE b : hash)
    {
        s.AppendFormat(L"%02x", b);
    }

    return s;
}

CSearchEngine::~CSearchEngine()
{
    Stop();
    Wait();
}

bool CSearchEngine::Start(HWND hwndNotify, const SearchCriteria& criteria)
{
    Wait();

    m_hwnd = hwndNotify;
    m_c = criteria;
    m_stop = false;
    m_pending.clear();
    m_all.clear();
    m_found = m_scanned = 0;
    m_hashDone = m_hashTotal = 0;
    m_lastFlush = m_lastStatus = GetTickCount64();

    // Prepare the byte patterns used for "Containing text".
    m_needleA.clear();
    m_needleW.clear();
    if (!m_c.containing.IsEmpty())
    {
        CStringA utf8(CW2A(m_c.containing, CP_UTF8));
        m_needleA.assign(reinterpret_cast<const BYTE*>((LPCSTR)utf8),
            reinterpret_cast<const BYTE*>((LPCSTR)utf8) + utf8.GetLength());
        const BYTE* w = reinterpret_cast<const BYTE*>((LPCWSTR)m_c.containing);
        m_needleW.assign(w, w + m_c.containing.GetLength() * sizeof(WCHAR));
        if (!m_c.matchCase)
        {
            for (BYTE& b : m_needleA)
            {
                b = FoldAscii(b);
            }

            for (BYTE& b : m_needleW)
            {
                b = FoldAscii(b);
            }
        }
    }

    m_running = true;
    m_thread = AfxBeginThread(ThreadProc, this, THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED);
    if (!m_thread)
    {
        m_running = false;
        return false;
    }

    m_thread->m_bAutoDelete = FALSE;
    m_thread->ResumeThread();
    return true;
}

void CSearchEngine::Wait()
{
    if (m_thread)
    {
        WaitForSingleObject(m_thread->m_hThread, INFINITE);
        delete m_thread;
        m_thread = nullptr;
    }
}

UINT CSearchEngine::ThreadProc(LPVOID param)
{
    static_cast<CSearchEngine*>(param)->Run();
    return 0;
}

void CSearchEngine::Run()
{
    for (const CString& root : m_c.roots)
    {
        if (m_stop)
        {
            break;
        }

        Walk(root);
    }

    int groups = 0;
    if (m_c.findDups && !m_stop)
    {
        groups = FindDuplicates();
    }

    if (!m_c.findDups || !m_stop)
    {
        Flush(true);
    }
    else
    {
        m_pending.clear();
    }

    bool stopped = m_stop;
    m_running = false;
    ::PostMessage(m_hwnd, WM_APP_DONE, stopped, groups);
}

void CSearchEngine::Walk(const CString& dir)
{
    CString display = dir;
    if (display.GetLength() > 3 && display[display.GetLength() - 1] == L'\\')
    {
        display.Truncate(display.GetLength() - 1);
    }

    CString status;
    status.Format(L"Searching %s   (%s found, %s files scanned)",
        (LPCWSTR)display, (LPCWSTR)FormatCount(m_found), (LPCWSTR)FormatCount(m_scanned));
    PostStatus(status, false);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(ExtendedPath(dir + L"*"), FindExInfoBasic, &fd, FindExSearchNameMatch,
        nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE)
    {
        return;
    }

    std::vector<CString> subdirs;
    do
    {
        if (m_stop)
        {
            break;
        }

        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
        {
            continue;
        }

        bool hiddenOrSystem = (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
        if (hiddenOrSystem && !m_c.includeHidden)
        {
            continue;
        }

        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDir)
        {
            // Skip symbolic links and junctions (name surrogates) so we never
            // loop, but follow other reparse points such as DFS links and
            // cloud-sync folders, which hold real content.
            bool isLink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                IsReparseTagNameSurrogate(fd.dwReserved0);
            if (m_c.subfolders && !isLink)
            {
                subdirs.push_back(dir + fd.cFileName + L"\\");
            }
        }
        else
        {
            ++m_scanned;
        }

        if (Matches(fd, dir))
        {
            FileEntry e;
            e.name = fd.cFileName;
            e.folder = display;
            e.size = isDir ? 0 : (static_cast<ULONGLONG>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            e.modified = fd.ftLastWriteTime;
            e.attrs = fd.dwFileAttributes;
            Emit(std::move(e));
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);

    for (const CString& sub : subdirs)
    {
        if (m_stop)
        {
            break;
        }

        Walk(sub);
    }
}

bool CSearchEngine::Matches(const WIN32_FIND_DATAW& fd, const CString& dir)
{
    bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (isDir && !m_c.includeFolders)
    {
        return false;
    }

    if (!m_c.patterns.empty())
    {
        bool ok = false;
        for (const CString& p : m_c.patterns)
        {
            if (PathMatchSpecW(fd.cFileName, p))
            {
                ok = true;
                break;
            }
        }

        if (!ok)
        {
            return false;
        }
    }

    if (!isDir && !m_c.extensions.empty())
    {
        LPCWSTR ext = PathFindExtensionW(fd.cFileName);
        if (*ext == L'.')
        {
            ++ext;
        }

        CString e(ext);
        e.MakeLower();
        if (std::find(m_c.extensions.begin(), m_c.extensions.end(), e) == m_c.extensions.end())
        {
            return false;
        }
    }

    ULONGLONG size = (static_cast<ULONGLONG>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
    if (m_c.sizeOp == SIZE_ATLEAST && size < m_c.sizeBytes)
    {
        return false;
    }

    if (m_c.sizeOp == SIZE_ATMOST && size > m_c.sizeBytes)
    {
        return false;
    }

    if (m_c.useDate)
    {
        ULONGLONG t = ToU64(fd.ftLastWriteTime);
        if (t < m_c.dateFrom || t > m_c.dateTo)
        {
            return false;
        }
    }

    // Most expensive test last.
    if (!isDir && !m_c.containing.IsEmpty())
    {
        // Reading a cloud-only or archived file would download/recall it.
        if (fd.dwFileAttributes & kNotLocalAttrs)
        {
            return false;
        }

        return FileContains(dir + fd.cFileName);
    }

    return true;
}

bool CSearchEngine::FileContains(const CString& path)
{
    HANDLE h = OpenForRead(path);
    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // Keep the tail of each chunk so matches spanning a chunk boundary are found.
    const size_t keep = (std::max)(m_needleA.size(), m_needleW.size()) - 1;
    std::vector<BYTE> buf(kReadChunk + keep);
    size_t have = 0;
    bool found = false;

    while (!m_stop)
    {
        DWORD read = 0;
        if (!ReadFile(h, buf.data() + have, kReadChunk, &read, nullptr) || read == 0)
        {
            break;
        }

        size_t len = have + read;
        if (!m_c.matchCase)
        {
            for (size_t i = have; i < len; i++)
            {
                buf[i] = FoldAscii(buf[i]);
            }
        }

        if (ContainsBytes(buf.data(), len, m_needleA) || ContainsBytes(buf.data(), len, m_needleW))
        {
            found = true;
            break;
        }

        have = (std::min)(keep, len);
        memmove(buf.data(), buf.data() + len - have, have);
    }

    CloseHandle(h);
    return found;
}

bool CSearchEngine::HashFile(const CString& path, ULONGLONG limit, BYTE out[32])
{
    HANDLE h = OpenForRead(path);
    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCRYPT_SUCCESS(BCryptCreateHash(m_alg, &hash, nullptr, 0, nullptr, 0, 0));
    ULONGLONG total = 0;
    while (ok && total < limit)
    {
        if (m_stop)
        {
            ok = false;
            break;
        }

        DWORD want = static_cast<DWORD>((std::min<ULONGLONG>)(m_buf.size(), limit - total));
        DWORD read = 0;
        if (!ReadFile(h, m_buf.data(), want, &read, nullptr))
        {
            ok = false;
            break;
        }

        if (read == 0)
        {
            break;
        }

        ok = BCRYPT_SUCCESS(BCryptHashData(hash, m_buf.data(), read, 0));
        total += read;
    }

    if (ok)
    {
        ok = BCRYPT_SUCCESS(BCryptFinishHash(hash, out, 32, 0));
    }

    if (hash)
    {
        BCryptDestroyHash(hash);
    }

    CloseHandle(h);
    return ok;
}

std::vector<CSearchEngine::Bucket> CSearchEngine::SplitByHash(std::vector<Bucket>& in, ULONGLONG limit, bool fullPass)
{
    std::vector<Bucket> out;
    for (Bucket& b : in)
    {
        if (m_stop)
        {
            break;
        }

        ULONGLONG size = m_all[b[0]].size;
        // Small files were already hashed completely during the prefix pass.
        if (fullPass && size <= kPrefixBytes)
        {
            out.push_back(std::move(b));
            continue;
        }

        std::map<std::string, Bucket> byHash;
        for (size_t idx : b)
        {
            if (m_stop)
            {
                break;
            }

            FileEntry& e = m_all[idx];
            BYTE digest[32];
            if (HashFile(e.FullPath(), limit, digest))
            {
                if (fullPass || size <= limit)
                {
                    memcpy(e.hash, digest, sizeof(digest));
                    e.hasHash = true;
                }
                byHash[std::string(reinterpret_cast<const char*>(digest), sizeof(digest))].push_back(idx);
            }

            ++m_hashDone;
            CString status;
            status.Format(L"Comparing file contents%s... %s of %s   %s",
                fullPass ? L" (full)" : L"",
                (LPCWSTR)FormatCount(m_hashDone), (LPCWSTR)FormatCount(m_hashTotal),
                (LPCWSTR)e.FullPath());
            PostStatus(status, false);
        }

        for (auto& kv : byHash)
        {
            if (kv.second.size() > 1)
            {
                out.push_back(std::move(kv.second));
            }
        }
    }

    return out;
}

int CSearchEngine::FindDuplicates()
{
    PostStatus(L"Grouping " + FormatCount(m_all.size()) + L" files...", true);

    // Pass 1: bucket by size (and name, if requested).
    std::unordered_map<std::wstring, Bucket> first;
    for (size_t i = 0; i < m_all.size(); i++)
    {
        const FileEntry& e = m_all[i];
        if (e.size == 0 && m_c.skipEmpty)
        {
            continue;
        }

        if (e.size < m_c.dupMinSize)
        {
            continue;
        }

        // Hashing a cloud-only or archived file would download/recall it.
        if (m_c.dupMode != DUP_NAME_SIZE && (e.attrs & kNotLocalAttrs))
        {
            continue;
        }

        std::wstring key = std::to_wstring(e.size);
        if (m_c.dupMode != DUP_CONTENT)
        {
            CString lower = e.name;
            lower.MakeLower();
            key += L'|';
            key += (LPCWSTR)lower;
        }

        first[key].push_back(i);
    }

    std::vector<Bucket> buckets;
    for (auto& kv : first)
    {
        if (kv.second.size() > 1)
        {
            buckets.push_back(std::move(kv.second));
        }
    }

    first.clear();

    // Pass 2/3: confirm by content (64 KB prefix hash, then full SHA-256).
    if (m_c.dupMode != DUP_NAME_SIZE && !buckets.empty())
    {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&m_alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        {
            return 0;
        }

        m_buf.resize(kReadChunk);

        m_hashTotal = 0;
        for (const Bucket& b : buckets)
        {
            m_hashTotal += b.size();
        }

        m_hashDone = 0;
        buckets = SplitByHash(buckets, kPrefixBytes, false);

        m_hashTotal = 0;
        for (const Bucket& b : buckets)
        {
            if (m_all[b[0]].size > kPrefixBytes)
            {
                m_hashTotal += b.size();
            }
        }

        m_hashDone = 0;
        if (!m_stop)
        {
            buckets = SplitByHash(buckets, ULLONG_MAX, true);
        }

        BCryptCloseAlgorithmProvider(m_alg, 0);
        m_alg = nullptr;
    }

    if (m_stop)
    {
        return 0;
    }

    // Biggest reclaimable space first; inside a set, sort by path.
    std::sort(buckets.begin(), buckets.end(), [this](const Bucket& a, const Bucket& b)
    {
        return m_all[a[0]].size * (a.size() - 1) > m_all[b[0]].size * (b.size() - 1);
    });

    int group = 0;
    for (Bucket& b : buckets)
    {
        std::sort(b.begin(), b.end(), [this](size_t x, size_t y)
        {
            return StrCmpLogicalW(m_all[x].FullPath(), m_all[y].FullPath()) < 0;
        });

        for (size_t idx : b)
        {
            FileEntry e = m_all[idx];
            e.group = group;
            m_pending.push_back(std::move(e));
        }

        ++group;
    }

    return group;
}

void CSearchEngine::Emit(FileEntry&& e)
{
    ++m_found;
    if (m_c.findDups)
    {
        if (!e.IsFolder())
        {
            m_all.push_back(std::move(e));
        }

        return;
    }

    m_pending.push_back(std::move(e));
    Flush(false);
}

void CSearchEngine::Flush(bool force)
{
    if (m_pending.empty())
    {
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (!force && m_pending.size() < kBatchSize && now - m_lastFlush < kFlushMs)
    {
        return;
    }

    m_lastFlush = now;

    auto* batch = new std::vector<FileEntry>(std::move(m_pending));
    m_pending = std::vector<FileEntry>();
    if (!::PostMessage(m_hwnd, WM_APP_RESULTS, 0, reinterpret_cast<LPARAM>(batch)))
    {
        delete batch;
    }
}

void CSearchEngine::PostStatus(const CString& text, bool force)
{
    ULONGLONG now = GetTickCount64();
    if (!force && now - m_lastStatus < kStatusMs)
    {
        return;
    }

    m_lastStatus = now;

    auto* s = new CString(text);
    if (!::PostMessage(m_hwnd, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(s)))
    {
        delete s;
    }
}
