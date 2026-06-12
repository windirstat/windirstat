// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "PageGpu.h"
#include "GpuHasher.h"

namespace
{
    // Worker-to-UI messages. WPARAM packs the algorithm index plus the
    // GPU flag; LPARAM carries the throughput in tenths of MB/s.
    constexpr UINT WM_BENCH_RESULT = WM_APP + 40;
    constexpr UINT WM_BENCH_DONE = WM_APP + 41;
    constexpr WPARAM BENCH_GPU_FLAG = 0x100;

    // Benchmark passes per algorithm/processor combination
    constexpr int kBenchPasses = 3;

    // The benchmark hashes an in-memory copy of the test file so it
    // measures hash throughput, not disk speed. Cap the copy so the
    // benchmark stays responsive and memory-friendly.
    constexpr size_t kBenchDataLimit = 64 * wds::Mi;

    void CloseBCryptAlgHandle(const BCRYPT_ALG_HANDLE h) noexcept { BCryptCloseAlgorithmProvider(h, 0); }

    // Hash the buffer once with BCrypt and return the elapsed seconds,
    // or a negative value on failure
    double CpuHashPass(const std::vector<BYTE>& data, const HashAlgorithm algo)
    {
        const auto& info = HashAlgorithms[algo];

        BCRYPT_ALG_HANDLE hAlgRaw = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlgRaw, info.id, MS_PRIMITIVE_PROVIDER, 0) != 0) return -1.0;
        SmartPointer hAlg(CloseBCryptAlgHandle, hAlgRaw);

        BCRYPT_HASH_HANDLE hHashRaw = nullptr;
        if (BCryptCreateHash(hAlg, &hHashRaw, nullptr, 0, nullptr, 0, 0) != 0) return -1.0;
        SmartPointer hHash(BCryptDestroyHash, hHashRaw);

        DWORD hashLength = 0;
        DWORD resultLength = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&hashLength),
            sizeof(hashLength), &resultLength, 0) != 0) return -1.0;

        const auto start = std::chrono::steady_clock::now();

        // Feed the data in 1 MiB slices to match the scan path's read size
        for (size_t offset = 0; offset < data.size(); offset += wds::Mi)
        {
            const auto sliceSize = static_cast<ULONG>(std::min<size_t>(wds::Mi, data.size() - offset));
            if (BCryptHashData(hHash, const_cast<PBYTE>(data.data() + offset), sliceSize, 0) != 0) return -1.0;
        }

        std::vector<BYTE> digest(hashLength);
        if (BCryptFinishHash(hHash, digest.data(), hashLength, 0) != 0) return -1.0;

        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    // Hash the buffer once on the GPU and return the elapsed seconds,
    // or a negative value on failure
    double GpuHashPass(const std::vector<BYTE>& data, const HashAlgorithm algo)
    {
        const auto start = std::chrono::steady_clock::now();
        if (GpuHasher::Hash(data.data(), data.size(), algo).empty()) return -1.0;
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    // Average multiple passes into MB/s; negative if any pass failed
    template <typename PassFunc>
    double MeasureThroughput(const std::vector<BYTE>& data, const HashAlgorithm algo,
        const std::stop_token& stopToken, PassFunc pass)
    {
        double totalSeconds = 0.0;
        for (int i = 0; i < kBenchPasses; ++i)
        {
            if (stopToken.stop_requested()) return -1.0;
            const double seconds = pass(data, algo);
            if (seconds <= 0.0) return -1.0;
            totalSeconds += seconds;
        }
        const double bytesPerSecond = static_cast<double>(data.size()) * kBenchPasses / totalSeconds;
        return bytesPerSecond / static_cast<double>(wds::Mi);
    }
}

IMPLEMENT_DYNAMIC(CPageGpu, CMFCPropertyPage)

CPageGpu::CPageGpu() : CMFCPropertyPage(IDD) {}

CPageGpu::~CPageGpu()
{
    // std::jthread requests stop and joins; the worker checks the stop
    // token between passes so this returns promptly
}

COptionsPropertySheet* CPageGpu::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageGpu::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_GPU_BENCHMARK_LIST, m_resultList);
    DDX_Text(pDX, IDC_GPU_TEST_FILE, m_testFilePath);
    DDX_CBIndex(pDX, IDC_GPU_ALGO_COMBO, m_selectedAlgorithm);
    DDX_Radio(pDX, IDC_GPU_RADIO_CPU, m_useGpu);
}

BEGIN_MESSAGE_MAP(CPageGpu, CMFCPropertyPage)
    ON_BN_CLICKED(IDC_GPU_BROWSE, OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_GPU_START_BENCHMARK, OnBnClickedBenchmark)
    ON_BN_CLICKED(IDC_GPU_RADIO_CPU, OnSettingChanged)
    ON_BN_CLICKED(IDC_GPU_RADIO_GPU, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_GPU_ALGO_COMBO, OnSettingChanged)
    ON_MESSAGE(WM_BENCH_RESULT, OnBenchmarkResult)
    ON_MESSAGE(WM_BENCH_DONE, OnBenchmarkDone)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageGpu::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageGpu::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    // Apply dark mode to this property page
    DarkMode::AdjustControls(GetSafeHwnd());

    // Probing for the device also compiles the kernels, so the result
    // reflects true GPU hashing availability, not just device presence
    m_gpuAvailable = GpuHasher::IsAvailable();
    GetDlgItem(IDC_GPU_RADIO_GPU)->EnableWindow(m_gpuAvailable);

    m_resultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_resultList.InsertColumn(0, Localization::Lookup(IDS_PAGE_GPU_ALGORITHM).c_str(), LVCFMT_LEFT, 170);
    m_resultList.InsertColumn(1, Localization::Lookup(IDS_PAGE_GPU_CPU).c_str(), LVCFMT_RIGHT, 180);
    m_resultList.InsertColumn(2, Localization::Lookup(IDS_PAGE_GPU_GPU).c_str(), LVCFMT_RIGHT, 180);

    const std::wstring notAvailable = Localization::Lookup(IDS_PAGE_GPU_NOT_AVAILABLE);
    auto* combo = static_cast<CComboBox*>(GetDlgItem(IDC_GPU_ALGO_COMBO));
    for (int i = 0; i < static_cast<int>(HashAlgorithms.size()); ++i)
    {
        m_resultList.InsertItem(i, HashAlgorithms[i].name);
        if (!m_gpuAvailable) m_resultList.SetItemText(i, 2, notAvailable.c_str());
        combo->AddString(HashAlgorithms[i].name);
        m_cpuResults[i] = -1.0;
        m_gpuResults[i] = -1.0;
    }

    m_selectedAlgorithm = std::clamp(COptions::GpuHashAlgorithm.Obj(),
        static_cast<int>(HASH_MD5), static_cast<int>(HASH_SHA512));
    m_useGpu = (COptions::UseGpuHashing && m_gpuAvailable) ? 1 : 0;

    UpdateData(FALSE);
    return TRUE;
}

void CPageGpu::OnBnClickedBrowse()
{
    const std::wstring fileSelectString = std::format(L"{} (*.*)|*.*||",
        Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT |
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    m_testFilePath = dlg.GetPathName();
    UpdateData(FALSE);
}

void CPageGpu::OnBnClickedBenchmark()
{
    UpdateData(TRUE);

    if (m_testFilePath.IsEmpty())
    {
        WdsMessageBox(*this, Localization::Lookup(IDS_PAGE_GPU_FILE_ERROR),
            wds::strWinDirStat, MB_OK | MB_ICONWARNING);
        return;
    }

    // One benchmark at a time; joins a finished thread, if any
    if (m_benchThread.joinable()) m_benchThread.join();

    GetDlgItem(IDC_GPU_START_BENCHMARK)->EnableWindow(FALSE);
    GetDlgItem(IDC_GPU_STATUS)->SetWindowText(Localization::Lookup(IDS_PAGE_GPU_RUNNING).c_str());

    // File loading and hashing both happen in the worker so the UI stays
    // responsive and the thread can run at reduced priority throughout
    const std::wstring filePath = m_testFilePath.GetString();
    // Capture HWND on the UI thread — reading m_hWnd from a worker thread is a data race.
    const HWND hwnd = GetSafeHwnd();
    m_benchThread = std::jthread([this, hwnd, filePath](const std::stop_token& stopToken) mutable
    {
        // Run at below-normal priority so the benchmark does not starve
        // other processes on the machine
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

        std::vector<BYTE> testData;
        {
            const SmartPointer hFile(CloseHandle, CreateFile(filePath.c_str(),
                GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
            if (hFile != INVALID_HANDLE_VALUE)
            {
                LARGE_INTEGER fileSize = {};
                GetFileSizeEx(hFile, &fileSize);
                testData.resize(static_cast<size_t>(std::min<ULONGLONG>(fileSize.QuadPart, kBenchDataLimit)));
                DWORD readBytes = 0;
                if (!testData.empty() && ReadFile(hFile, testData.data(),
                    static_cast<DWORD>(testData.size()), &readBytes, nullptr) == 0)
                {
                    readBytes = 0;
                }
                testData.resize(readBytes);
            }
        }

        if (testData.empty())
        {
            ::PostMessage(hwnd, WM_BENCH_DONE, 1 /*error flag*/, 0);
            return;
        }

        RunBenchmark(stopToken, std::move(testData), hwnd);
    });
}

void CPageGpu::RunBenchmark(const std::stop_token& stopToken, const std::vector<BYTE> testData, const HWND hwnd)
{
    // The worker only posts messages; all control and member updates
    // happen on the UI thread in the message handlers.
    // hwnd is pre-captured on the UI thread before the worker starts.

    for (int algo = 0; algo < static_cast<int>(HashAlgorithms.size()); ++algo)
    {
        if (stopToken.stop_requested()) return;

        const double cpuMbps = MeasureThroughput(testData,
            static_cast<HashAlgorithm>(algo), stopToken, CpuHashPass);
        ::PostMessage(hwnd, WM_BENCH_RESULT, static_cast<WPARAM>(algo),
            static_cast<LPARAM>(std::lround(cpuMbps * 10.0)));

        // Yield between measurements so other threads stay responsive
        SleepEx(5, FALSE);

        if (m_gpuAvailable)
        {
            if (stopToken.stop_requested()) return;
            const double gpuMbps = MeasureThroughput(testData,
                static_cast<HashAlgorithm>(algo), stopToken, GpuHashPass);
            ::PostMessage(hwnd, WM_BENCH_RESULT, static_cast<WPARAM>(algo) | BENCH_GPU_FLAG,
                static_cast<LPARAM>(std::lround(gpuMbps * 10.0)));

            SleepEx(5, FALSE);
        }
    }

    ::PostMessage(hwnd, WM_BENCH_DONE, 0, 0);
}

LRESULT CPageGpu::OnBenchmarkResult(const WPARAM wParam, const LPARAM lParam)
{
    const int algo = static_cast<int>(wParam & 0xFF);
    const bool isGpu = (wParam & BENCH_GPU_FLAG) != 0;
    const double mbps = static_cast<double>(lParam) / 10.0;

    (isGpu ? m_gpuResults : m_cpuResults)[algo] = mbps;

    const std::wstring text = mbps >= 0.0
        ? std::format(L"{:.0f} {}", mbps, Localization::Lookup(IDS_PAGE_GPU_MB_PER_SEC))
        : Localization::Lookup(IDS_PAGE_GPU_NOT_AVAILABLE);
    m_resultList.SetItemText(algo, isGpu ? 2 : 1, text.c_str());

    return 0;
}

LRESULT CPageGpu::OnBenchmarkDone(const WPARAM wParam, LPARAM)
{
    GetDlgItem(IDC_GPU_START_BENCHMARK)->EnableWindow(TRUE);
    if (wParam != 0)
    {
        WdsMessageBox(*this, Localization::Lookup(IDS_PAGE_GPU_FILE_ERROR),
            wds::strWinDirStat, MB_OK | MB_ICONWARNING);
        return 0;
    }
    ApplyRecommendation();
    return 0;
}

void CPageGpu::ApplyRecommendation()
{
    // Find the fastest measured algorithm/processor combination
    int bestAlgo = -1;
    bool bestIsGpu = false;
    double bestMbps = 0.0;
    double bestCpuMbps = 0.0;
    double bestGpuMbps = 0.0;
    for (int algo = 0; algo < static_cast<int>(HashAlgorithms.size()); ++algo)
    {
        if (m_cpuResults[algo] > bestMbps) { bestMbps = m_cpuResults[algo]; bestAlgo = algo; bestIsGpu = false; }
        if (m_gpuResults[algo] > bestMbps) { bestMbps = m_gpuResults[algo]; bestAlgo = algo; bestIsGpu = true; }
        bestCpuMbps = std::max(bestCpuMbps, m_cpuResults[algo]);
        bestGpuMbps = std::max(bestGpuMbps, m_gpuResults[algo]);
    }
    if (bestAlgo < 0) return;

    // Mark the winner in the list and preselect it for the user
    const std::wstring winner = L"⭐ " + std::wstring(
        m_resultList.GetItemText(bestAlgo, bestIsGpu ? 2 : 1).GetString());
    m_resultList.SetItemText(bestAlgo, bestIsGpu ? 2 : 1, winner.c_str());

    m_selectedAlgorithm = bestAlgo;
    m_useGpu = bestIsGpu ? 1 : 0;
    UpdateData(FALSE);
    SetModified();

    // Tell the user explicitly when a present GPU loses against the CPU
    // (typical for virtual machines with an emulated GPU device)
    GetDlgItem(IDC_GPU_STATUS)->SetWindowText(
        m_gpuAvailable && bestGpuMbps < bestCpuMbps
        ? Localization::Lookup(IDS_PAGE_GPU_SLOWER).c_str() : L"");
}

void CPageGpu::OnOK()
{
    UpdateData();

    const bool refreshAll = COptions::ScanForDuplicates &&
        (COptions::FileHashAlgorithm != m_selectedAlgorithm ||
         COptions::UseGpuHashing != (m_useGpu != 0));

    COptions::GpuHashAlgorithm = m_selectedAlgorithm;
    COptions::UseGpuHashing = (m_useGpu != 0) && m_gpuAvailable;

    // The selection on this page is authoritative for duplicate
    // detection, so keep the algorithm used by the scan in sync
    COptions::FileHashAlgorithm = m_selectedAlgorithm;

    if (refreshAll)
    {
        CDirStatDoc::Get()->OnOpenDocument(
            CDirStatDoc::Get()->GetPathName().GetString());
    }

    CMFCPropertyPage::OnOK();
}

void CPageGpu::OnSettingChanged()
{
    SetModified();
}
