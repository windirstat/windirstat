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

#pragma once

#include "pch.h"

class COptionsPropertySheet;

//
// CPageGpu. "Settings" property page "Hashing".
//
// Benchmarks all five hash algorithms on the CPU (BCrypt) and, when a
// Direct3D 11 hardware device is present, on the GPU. The user's choice
// of algorithm and processor is stored in COptions::GpuHashAlgorithm,
// COptions::UseGpuHashing and COptions::FileHashAlgorithm.
//
class CPageGpu final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageGpu)

    enum : std::uint8_t { IDD = IDD_PAGE_GPU };

public:
    CPageGpu();
    ~CPageGpu() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    // Run one benchmark pass on the worker thread and post the results
    void RunBenchmark(const std::stop_token& stopToken, std::vector<BYTE> testData, HWND hwnd);

    // Apply the fastest measured combination to the selection controls
    void ApplyRecommendation();

    CListCtrl m_resultList;
    CStringW m_testFilePath;
    int m_selectedAlgorithm = HASH_SHA256; // combo index, matches HashAlgorithm
    int m_useGpu = 0;                      // radio index: 0 = CPU, 1 = GPU
    bool m_gpuAvailable = false;

    // Results in MB/s, indexed [algorithm]; < 0 means not measured
    std::array<double, 5> m_cpuResults{};
    std::array<double, 5> m_gpuResults{};

    std::jthread m_benchThread;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedBrowse();
    afx_msg void OnBnClickedBenchmark();
    afx_msg void OnSettingChanged();
    afx_msg LRESULT OnBenchmarkResult(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnBenchmarkDone(WPARAM wParam, LPARAM lParam);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
