// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "Item.h"
#include "Filtering.h"
#include "FinderBasic.h"
#include "FinderMtp.h"
#include "FinderRemote.h"
#include "FinderNtfs.h"

class CItem::ScanBatch final
{
    CItem* m_parent;
    BlockingQueue<CItem*>* m_queue;
    std::vector<CItem*> m_children;
    std::vector<CItem*> m_duplicates;
    std::unordered_map<std::wstring, std::pair<ULONGLONG, ULONGLONG>> m_extensions;
    ULONGLONG m_physical = 0, m_logical = 0, m_lastFlush = GetTickCount64();
    FILETIME m_lastChange{};
    ULONG m_files = 0, m_folders = 0;

public:
    ScanBatch(CItem* parent, BlockingQueue<CItem*>* queue) : m_parent(parent), m_queue(queue)
    {
        m_children.reserve(128);
        if (COptions::ScanForDuplicates) m_duplicates.reserve(128);
    }
    ~ScanBatch() { Flush(); }

    void Add(CItem* child)
    {
        if (m_parent->IsTypeOrFlag(ITF_MTP, ITF_REMOTE))
            child->SetFlag(m_parent->GetRawType() & (ITF_MTP | ITF_REMOTE));
        child->SetParent(m_parent);
        m_children.push_back(child);
        m_physical += child->GetSizePhysical();
        m_logical += child->GetSizeLogical();
        m_lastChange = std::max(m_lastChange, child->GetLastChange());
        if (!child->IsTypeOrFlag(IT_FILE)) { ++m_folders; return; }

        if (COptions::ScanForDuplicates && !child->IsTypeOrFlag(ITF_REMOTE)) m_duplicates.push_back(child);
        ++m_files;
        auto& [files, bytes] = m_extensions[child->GetExtension()];
        ++files;
        bytes += child->GetSizeLogical();
        child->SetFlag(ITF_EXTDATA);
    }

    void FlushIfNeeded(const bool force = false)
    {
        if (!force && m_children.size() < 128 && GetTickCount64() - m_lastFlush < 100) return;

        // Publish the batch before hashing; cancellation cleanup only publishes remaining children.
        Flush();
        for (CItem* child : m_duplicates) CFileDupeControl::Get()->ProcessDuplicate(child, m_queue);
        m_duplicates.clear();
    }

    void Flush()
    {
        if (m_children.empty()) return;
        m_parent->UpwardAddSizePhysical(std::exchange(m_physical, 0));
        m_parent->UpwardAddSizeLogical(std::exchange(m_logical, 0));
        m_parent->UpwardAddFiles(std::exchange(m_files, 0));
        m_parent->UpwardAddFolders(std::exchange(m_folders, 0));
        m_parent->UpwardUpdateLastChange(std::exchange(m_lastChange, FILETIME{}));
        for (const auto& [extension, totals] : m_extensions)
        {
            auto* record = CWinDirStatModel::Get()->GetExtensionDataRecord(extension);
            record->files.fetch_add(totals.first, std::memory_order_relaxed);
            record->bytes.fetch_add(totals.second, std::memory_order_relaxed);
        }
        m_extensions.clear();

        const auto append = [this] { m_parent->GetFolderInfo()->m_children.append_range(m_children); };
        if (!m_parent->MutateHiddenChildren(append)) CMainFrame::Get()->InvokeInMessageThread([&]
        {
            append();
            if (!m_parent->IsExpanded()) return;
            const std::vector<CTreeListItem*> rows(m_children.begin(), m_children.end());
            CFileTreeControl::Get()->OnChildrenAdded(m_parent, rows);
        });
        for (CItem* child : m_children)
        {
            if (child->IsTypeOrFlag(IT_FILE)) CFileTopControl::Get()->ProcessTop(child);
            else if (child->GetReadJobs() > 0) m_queue->Push(child);
        }
        m_children.clear();
        m_lastFlush = GetTickCount64();
        m_parent->UpwardDrivePacman();
    }
};

// --- Construction / Destruction ---

void* CItem::operator new(const size_t size, const ITEMTYPE type, const std::wstring_view name)
{
    const size_t extraBytes = ((type & IT_CONTAINER) != IT_NONE ? sizeof(CHILDINFO) : 0) +
        (name.size() + 1) * sizeof(wchar_t);
    if (name.size() > std::numeric_limits<USHORT>::max() ||
        size > std::numeric_limits<size_t>::max() - extraBytes)
        throw std::bad_array_new_length();
    return ::operator new(size + extraBytes);
}

CItem* CItem::Create(const ITEMTYPE type, const std::wstring_view name)
{
    return new (type, type & IT_DRIVE ? std::wstring_view{} : name) CItem(type, name);
}

CItem* CItem::Create(CItem* linkedItem)
{
    return new (IT_HLINKS_FILE, std::wstring_view{}) CItem(linkedItem);
}

CItem* CItem::Create(const ITEMTYPE type, const std::wstring_view name, const FILETIME lastChange,
    const ULONGLONG sizePhysical, const ULONGLONG sizeLogical, const ULONGLONG index,
    const DWORD attributes, const ULONG files, const ULONG subdirs)
{
    return new (type, type & IT_DRIVE ? std::wstring_view{} : name)
        CItem(type, name, lastChange, sizePhysical, sizeLogical, index, attributes, files, subdirs);
}

CItem::CItem(const ITEMTYPE type, const std::wstring_view name) : m_type(type)
{
    if (!IsLeaf())
    {
        std::construct_at(GetFolderInfo());

        // My computer node will never have these attributes set externally
        if (IsTypeOrFlag(IT_MYCOMPUTER))
        {
            m_attributes = 0;
        }
    }

    if (IsTypeOrFlag(IT_DRIVE))
    {
        // Store drive paths with a backslash
        std::wstring nameTmp(name);
        if (nameTmp.ends_with(L":")) nameTmp.append(L"\\");

        // The name string on the drive is two parts separated by a pipe. For example,
        // C:\|Local Disk (C:) is the true path followed by the name description
        SetName(std::format(L"{:.2}|{}", nameTmp, FormatVolumeNameOfRootPath(nameTmp)));
        m_attributes = LOWORD(GetFileAttributes(GetPathLong().c_str()));
    }
    else
    {
        SetName(name);
    }
    if (IsTypeOrFlag(ITF_REMOTE)) m_attributes = FILE_ATTRIBUTE_DIRECTORY;
}

CItem::CItem(CItem* linkedItem) : m_type(IT_HLINKS_FILE)
{
    assert(linkedItem != nullptr);
    SetName({});
    m_sizePhysical = linkedItem->GetSizePhysicalRaw();
    m_sizeLogical = linkedItem->GetSizeLogical();
    m_index = reinterpret_cast<std::uintptr_t>(linkedItem);
    SetLastChange(linkedItem->GetLastChange());
}

CItem::CItem(const ITEMTYPE type, const std::wstring_view name, const FILETIME lastChange,
    const ULONGLONG sizePhysical, const ULONGLONG sizeLogical, const ULONGLONG index,
    const DWORD attributes, const ULONG files, const ULONG subdirs)
{
    m_type = type;
    if (!IsLeaf())
    {
        const auto folderInfo = std::construct_at(GetFolderInfo());
        folderInfo->m_subdirs = subdirs;
        folderInfo->m_files = files;
    }

    if (IsTypeOrFlag(IT_DRIVE))
    {
        SetName(std::format(L"{:.2}|{}", name, FormatVolumeNameOfRootPath(std::wstring(name))));
    }
    else
    {
        SetName(name);
    }
    SetLastChange(lastChange);
    m_sizePhysical = sizePhysical;
    m_sizeLogical = sizeLogical;
    m_index = index;
    m_attributes = LOWORD(attributes);
}

// Only failed enumerations carry text, kept apart from the per-directory state.
static std::mutex scanErrorsMutex;
static std::unordered_map<const CItem*, std::wstring> scanErrors;

static bool HasScanError(const ScanOutcome outcome) noexcept
{
    return outcome != ScanOutcome::NotStarted && outcome != ScanOutcome::Complete &&
        outcome != ScanOutcome::Cancelled;
}

CItem::~CItem()
{
    if (!IsLeaf())
    {
        if (HasScanError(GetFolderInfo()->m_scanOutcome)) SetScanResult({});
        std::vector<CItem*> queue = std::move(GetFolderInfo()->m_children);
        while (!queue.empty())
        {
            const CItem* current = queue.back();
            queue.pop_back();

            if (!current->IsLeaf())
            {
                if (auto& children = current->GetFolderInfo()->m_children; !children.empty())
                {
                    queue.append_range(children);
                    children.clear();
                }
            }
            delete current;
        }
        std::destroy_at(GetFolderInfo());
    }
    // Release any MTP shell metadata registered to this item
    if (IsTypeOrFlag(ITF_MTP)) FinderMtp::UnregisterPath(m_index);
}

// --- Hierarchy ---

const std::vector<CItem*>& CItem::GetChildren() const noexcept
{
    assert(!IsLeaf());
    return GetFolderInfo()->m_children;
}

CItem* CItem::GetEnumRoot() const noexcept
{
    // Resolve the selected folder, drive, or MTP branch without crossing a multi-root container.
    auto* item = const_cast<CItem*>(this);
    while (!item->IsTypeOrFlag(ITF_ROOTITEM) && item->GetParent() != nullptr &&
        !item->GetParent()->IsTypeOrFlag(IT_MYCOMPUTER)) item = item->GetParent();
    return item;
}

CItem* CItem::GetParentDrive() const noexcept
{
    CItem* enumRoot = GetEnumRoot();
    return enumRoot->IsTypeOrFlag(IT_DRIVE) ? enumRoot : nullptr;
}

CItem* CItem::GetVolumeRoot() const noexcept
{
    // Prefer a nearer mounted volume before falling back to the enumeration root.
    CItem* enumRoot = GetEnumRoot();
    for (auto* item = const_cast<CItem*>(this); item != enumRoot; item = item->GetParent())
    {
        if (item->IsTypeOrFlag(IT_DRIVE, ITRP_MOUNT)) return item;
    }
    return enumRoot;
}

bool CItem::IsScanRoot() const noexcept
{
    // True for the tree root and for every selection placed directly under a multi-root container
    return IsTypeOrFlag(ITF_ROOTITEM) || GetParent() == nullptr || GetParent()->IsTypeOrFlag(IT_MYCOMPUTER);
}

bool CItem::IsMtpRoot() const noexcept
{
    return IsTypeOrFlag(ITF_MTP) && IsScanRoot();
}

bool CItem::HasShellIdentity() const noexcept
{
    if (IsTypeOrFlag(ITF_REMOTE)) return false;
    return !IsTypeOrFlag(ITF_MTP) || FinderMtp::HasShellIdentity(m_index);
}

bool CItem::MutateHiddenChildren(const std::function<void()>& mutation) const
{
    // Progress reads drive children even when their rows are hidden.
    if (IsTypeOrFlag(IT_DRIVE, IT_MYCOMPUTER) && CMainFrame::Get() != nullptr) return false;
    std::shared_lock lock(CTreeListControl::s_viewStateMutex);
    if (CTreeListControl::s_viewStates.contains(this)) return false;
    mutation();
    return true;
}

void CItem::AddChild(CItem* child, const bool addOnly)
{
    assert(!child->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) || GetParentDrive() == nullptr ||
        GetParentDrive()->GetReadJobs() > 0 || FindHardlinksItem() == nullptr);

    if (IsTypeOrFlag(ITF_MTP, ITF_REMOTE)) child->SetFlag(GetRawType() & (ITF_MTP | ITF_REMOTE));

    if (!addOnly)
    {
        UpwardAddSizePhysical(child->GetSizePhysical());
        UpwardAddSizeLogical(child->GetSizeLogical());
        UpwardUpdateLastChange(child->GetLastChange());
        ExtensionDataAdd();
    }

    child->SetParent(this);
    const auto append = [this, child] { GetFolderInfo()->m_children.push_back(child); };
    if (!MutateHiddenChildren(append)) CMainFrame::Get()->InvokeInMessageThread([&]
    {
        // Add child in UI thread since UI thread immediately uses it
        append();
        if (IsExpanded()) CFileTreeControl::Get()->OnChildAdded(this, child);
    });
}

void CItem::RemoveChild(CItem* child) const
{
    assert(!child->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) || child->GetParentDrive() == nullptr ||
        child->FindHardlinksItem() == nullptr);

    const auto remove = [this, child]
    {
        if (IsVisible()) CFileTreeControl::Get()->OnChildRemoved(this, child);
        auto& children = GetFolderInfo()->m_children;
        if (const auto it = std::ranges::find(children, child); it != children.end()) children.erase(it);
        delete child;
    };
    if (CMainFrame::Get() != nullptr) CMainFrame::Get()->InvokeInMessageThread(remove);
    else remove();
}

void CItem::RemoveAllChildren() const
{
    assert(!IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) || GetParentDrive() == nullptr ||
        FindHardlinksItem() == nullptr);

    if (IsRootItem())
    {
        CWinDirStatModel::Get()->GetExtensionData()->clear();
    }

    if (IsLeaf()) return;
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CFileTreeControl::Get()->OnRemovingAllChildren(this);
    });

    for (const auto& child : GetFolderInfo()->m_children)
    {
        delete child;
    }
    GetFolderInfo()->m_children.clear();
}

CItem* CItem::AddDirectory(const Finder& finder, ScanBatch& batch)
{
    // Bypass filesystem reparse restrictions when traversing MTP directories
    const bool follow = IsTypeOrFlag(ITF_MTP) || !finder.IsProtectedReparsePoint() &&
        CDirStatApp::Get()->IsFollowingAllowed(finder.GetReparseTag());

    auto* const child = CItem::Create(IT_DIRECTORY, IsTypeOrFlag(ITF_REMOTE) ?
        StorageSource::EscapeName(finder.GetFileName()) : finder.GetFileName());
    child->SetIndex(finder.GetIndex());
    // Preserve MTP shell metadata under the child index for later access
    if (IsTypeOrFlag(ITF_MTP))
        child->SetIndex(FinderMtp::RegisterPath({}, finder.GetShellPath(), finder.GetShellPidl()));
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    child->SetReparseTag(finder.GetReparseTag());
    if (finder.IsReserved() || this->IsTypeOrFlag(ITF_RESERVED)) child->SetFlag(ITF_RESERVED);
    if ((finder.RequiresBasicEnumeration() || IsTypeOrFlag(ITF_BASIC)) && follow)
        child->SetFlag(ITF_BASIC);
    batch.Add(child);
    child->UpwardAddReadJobs(follow ? 1 : 0);
    if (!follow) child->SetScanResult({ ScanOutcome::Complete, {} });

    return child;
}

CItem* CItem::AddFile(const Finder& finder, ScanBatch& batch)
{
    auto* const child = CItem::Create(IT_FILE, IsTypeOrFlag(ITF_REMOTE) ?
        StorageSource::EscapeName(finder.GetFileName()) : finder.GetFileName());
    child->SetIndex(finder.GetIndex());
    // Preserve MTP shell metadata under the child index for later access
    if (IsTypeOrFlag(ITF_MTP))
        child->SetIndex(FinderMtp::RegisterPath({}, finder.GetShellPath(), finder.GetShellPidl()));
    child->SetSizePhysical(finder.GetFileSizePhysical());
    child->SetSizeLogical(finder.GetFileSizeLogical());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    child->SetReparseTag(finder.GetReparseTag());
    if (finder.IsReserved() || this->IsTypeOrFlag(ITF_RESERVED)) child->SetFlag(ITF_RESERVED);
    batch.Add(child);
    child->SetDone();
    return child;
}

std::shared_ptr<StorageSource> CItem::GetStorageSource(const bool renew) const
{
    return IsTypeOrFlag(ITF_REMOTE) ? StorageSource::Open(GetEnumRoot()->GetNameView(), renew) : nullptr;
}

ScanResult CItem::GetScanResult() const
{
    if (!IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY)) return { ScanOutcome::Complete, {} };
    ScanResult result{ GetFolderInfo()->m_scanOutcome, {} };
    if (!HasScanError(result.outcome)) return result;
    std::lock_guard guard(scanErrorsMutex);
    if (const auto found = scanErrors.find(this); found != scanErrors.end()) result.error = found->second;
    return result;
}

void CItem::SetScanResult(ScanResult result)
{
    if (IsLeaf()) return;
    const bool failed = HasScanError(result.outcome);
    if (!HasScanError(GetFolderInfo()->m_scanOutcome.exchange(result.outcome)) && !failed) return;
    std::lock_guard guard(scanErrorsMutex);
    if (failed && !result.error.empty()) scanErrors[this] = std::move(result.error);
    else scanErrors.erase(this);
}

void CItem::ReconcileScanResults(CItem* root, const bool cancelled)
{
    if (root == nullptr) return;
    std::vector<CItem*> items{ root };
    for (size_t i = 0; i < items.size(); ++i)
        if (!items[i]->IsLeaf()) for (auto* child : items[i]->GetChildren())
            if (!child->IsLeaf()) items.push_back(child);
    for (auto* item : items | std::views::reverse)
    {
        if (item->IsLeaf()) continue;
        auto& outcome = item->GetFolderInfo()->m_scanOutcome;
        const bool enumerated = item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY);
        if (cancelled && enumerated && outcome == ScanOutcome::NotStarted) outcome = ScanOutcome::Cancelled;
        const bool incomplete = enumerated && outcome != ScanOutcome::Complete ||
            std::ranges::any_of(item->GetChildren(),
            [](const CItem* child) { return child->IsTypeOrFlag(ITF_SCANERROR); });
        item->SetFlag(ITF_SCANERROR, !incomplete);
    }
}

// --- Size & Stats ---

ULONGLONG CItem::GetSizePhysical() const noexcept
{
    return (IsTypeOrFlag(ITF_HARDLINK) && COptions::ProcessHardlinks) ? 0 : m_sizePhysical.load();
}

bool CItem::HasSizePhysical() const noexcept
{
    // Remote storage reports no allocation size, which also leaves a mixed root's total unknown.
    return !IsTypeOrFlag(ITF_REMOTE) && !(IsTypeOrFlag(IT_MYCOMPUTER) && std::ranges::any_of(GetChildren(),
        [](const CItem* child) { return child->IsTypeOrFlag(ITF_REMOTE); }));
}

void CItem::UpwardAddSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_sizePhysical += bytes;
        if (p->IsTypeOrFlag(ITF_HARDLINK)) break;
    }
}

void CItem::UpwardSubtractSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        assert(bytes <= p->m_sizePhysical);
        p->m_sizePhysical -= bytes;
        if (p->IsTypeOrFlag(ITF_HARDLINK)) break;
    }
}

void CItem::UpwardAddSizeLogical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_sizeLogical += bytes;
    }
}

void CItem::UpwardSubtractSizeLogical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        assert(bytes <= p->m_sizeLogical);
        p->m_sizeLogical -= bytes;
    }
}

void CItem::UpwardAddFolders(const ULONG dirCount) const noexcept
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->GetFolderInfo()->m_subdirs += dirCount;
    }
}

void CItem::UpwardSubtractFolders(const ULONG dirCount) const noexcept
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        assert(p->GetFolderInfo()->m_subdirs >= dirCount);
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->GetFolderInfo()->m_subdirs -= dirCount;
    }
}

void CItem::UpwardAddFiles(const ULONG fileCount) const noexcept
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->GetFolderInfo()->m_files += fileCount;
    }
}

void CItem::UpwardSubtractFiles(const ULONG fileCount) const noexcept
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        assert(p->GetFolderInfo()->m_files >= fileCount);
        p->GetFolderInfo()->m_files -= fileCount;
    }
}

double CItem::GetFraction() const noexcept
{
    if (!GetParent())
    {
        return 1.0;
    }
    const ULONGLONG parentSize = GetParent()->TmiGetSize();
    if (parentSize == 0)
    {
        return 1.0;
    }
    const ULONGLONG size = TmiGetSize();
    return static_cast<double>(size) / static_cast<double>(parentSize);
}

double CItem::GetAbsoluteFraction() const noexcept
{
    if (!GetParent())
    {
        return 1.0;
    }

    const CItem* root = this;
    while (root->GetParent() != nullptr)
    {
        root = root->GetParent();
    }

    const ULONGLONG rootSize = root->TmiGetSize();
    if (rootSize == 0)
    {
        return 0.0;
    }

    const ULONGLONG size = TmiGetSize();
    return static_cast<double>(size) / static_cast<double>(rootSize);
}

ULONGLONG CItem::GetItemsCount() const noexcept
{
    if (IsLeaf()) return 0;
    return static_cast<ULONGLONG>(GetFolderInfo()->m_files) + static_cast<ULONGLONG>(GetFolderInfo()->m_subdirs);
}

void CItem::ExtensionDataAdd()
{
    if (!IsTypeOrFlag(IT_FILE) || IsTypeOrFlag(ITF_EXTDATA)) return;
    const auto record = CWinDirStatModel::Get()->GetExtensionDataRecord(GetExtension());
    record->AddFile(GetSizeLogical());
    SetFlag(ITF_EXTDATA);
}

void CItem::ExtensionDataRemove()
{
    if (!IsTypeOrFlag(IT_FILE) || !IsTypeOrFlag(ITF_EXTDATA)) return;
    const auto record = CWinDirStatModel::Get()->GetExtensionDataRecord(GetExtension());
    record->RemoveFile(GetSizeLogical());
    if (record->GetFiles() == 0) CWinDirStatModel::Get()->GetExtensionData()->erase(GetExtension());
    SetFlag(ITF_EXTDATA, true);
}

void CItem::ExtensionDataProcessChildren(const bool remove)
{
    std::vector childStack({ this });
    while (!childStack.empty()) [[msvc::forceinline_calls]]
    {
        const auto& item = childStack.back();
        childStack.pop_back();

        if (item->IsTypeOrFlag(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE))
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push_back(child);
            }
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            remove ? item->ExtensionDataRemove() : item->ExtensionDataAdd();
        }
    }
}

// --- Attributes & Properties ---

DWORD CItem::GetAttributes() const noexcept
{
    return m_attributes == LOWORD(INVALID_FILE_ATTRIBUTES)
        ? INVALID_FILE_ATTRIBUTES : m_attributes;
}

USHORT CItem::GetSortAttributes() const noexcept
{
    if (GetAttributes() == INVALID_FILE_ATTRIBUTES) return 0;

    USHORT ret = 0;

    // We want to enforce the order RHSACE with R being the highest priority
    // attribute and E being the lowest priority attribute.
    ret |= m_attributes & FILE_ATTRIBUTE_READONLY    ? 1 << 6 : 0; // R
    ret |= m_attributes & FILE_ATTRIBUTE_HIDDEN      ? 1 << 5 : 0; // H
    ret |= m_attributes & FILE_ATTRIBUTE_SYSTEM      ? 1 << 4 : 0; // S
    ret |= m_attributes & FILE_ATTRIBUTE_ARCHIVE     ? 1 << 3 : 0; // A
    ret |= m_attributes & FILE_ATTRIBUTE_COMPRESSED  ? 1 << 2 : 0; // C
    ret |= m_attributes & FILE_ATTRIBUTE_ENCRYPTED   ? 1 << 1 : 0; // E
    ret |= m_attributes & FILE_ATTRIBUTE_SPARSE_FILE ? 1 << 0 : 0; // Z

    return ret;
}

DWORD CItem::GetReparseTag() const noexcept
{
    if (IsTypeOrFlag(ITRP_SYMLINK)) return IO_REPARSE_TAG_SYMLINK;
    if (IsTypeOrFlag(ITRP_MOUNT)) return IO_REPARSE_TAG_MOUNT_POINT;
    if (IsTypeOrFlag(ITRP_JUNCTION)) return IO_REPARSE_TAG_JUNCTION_POINT;
    if (IsTypeOrFlag(ITRP_CLOUD)) return IO_REPARSE_TAG_CLOUD_MASK;
    return 0;
}

void CItem::SetReparseTag(const DWORD reparseType) noexcept
{
    if (reparseType == IO_REPARSE_TAG_SYMLINK) SetReparseType(ITRP_SYMLINK);
    else if (reparseType == IO_REPARSE_TAG_MOUNT_POINT) SetReparseType(ITRP_MOUNT);
    else if (reparseType == IO_REPARSE_TAG_JUNCTION_POINT) SetReparseType(ITRP_JUNCTION);
    else if ((reparseType & ~IO_REPARSE_TAG_CLOUD_MASK) == IO_REPARSE_TAG_CLOUD) SetReparseType(ITRP_CLOUD);
}

std::wstring CItem::GetOwner(const bool force) const
{
    // Remote objects have no Windows security descriptor.
    if (!SupportsFilesystemApis()) return {};
    auto* viewState = force ? nullptr : GetViewState();
    if (!force && viewState == nullptr) return {};

    // If visible, use cached variable
    std::wstring tmp;
    std::wstring & ret = viewState != nullptr ? viewState->owner : tmp;
    if (!ret.empty()) return ret;

    // Fetch owner information from drive
    SmartPointer ps(LocalFree, static_cast<PSECURITY_DESCRIPTOR>(nullptr));
    PSID sid = nullptr;
    GetNamedSecurityInfo(GetPathLong().c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
        &sid, nullptr, nullptr, nullptr, &ps);
    ret = GetNameFromSid(sid);
    return ret;
}

void CItem::UpwardUpdateLastChange(const FILETIME& t) noexcept
{
    const auto value = std::bit_cast<ULONGLONG>(t);
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        auto previous = p->m_lastChange.load(std::memory_order_relaxed);
        while (previous < value && !p->m_lastChange.compare_exchange_weak(previous, value,
            std::memory_order_relaxed)) {}
    }
}

void CItem::UpwardRecalcLastChange()
{
    // ignore current object in recalculation
    for (auto p = GetParent(); p != nullptr; p = p->GetParent())
    {
        const auto newMax = (std::ranges::max)(
            p->GetChildren() | std::views::transform(&CItem::GetLastChange));

        if (p->GetLastChange() == newMax) break;
        p->SetLastChange(newMax);
    }
}

// --- Paths & Names ---

void CItem::SetName(const std::wstring_view name)
{
    if (name.size() > std::numeric_limits<USHORT>::max()) throw std::bad_array_new_length();
    auto nameLen = static_cast<USHORT>(name.size());
    while (nameLen > 0 && name[nameLen - 1] == L'\\') nameLen--;

    auto driveName = IsTypeOrFlag(IT_DRIVE) ? std::make_unique_for_overwrite<wchar_t[]>(nameLen + 1) : nullptr;
    wchar_t* buffer = driveName != nullptr ? driveName.get() : GetNameBuffer();
    if (nameLen) std::wmemcpy(buffer, name.data(), nameLen);
    buffer[nameLen] = L'\0';
    if (driveName != nullptr) GetFolderInfo()->m_driveName = std::move(driveName);
    m_nameLen = nameLen;
}

std::wstring CItem::GetName(const bool stripDrivePrefix) const noexcept
{
    if (IsTypeOrFlag(ITF_REMOTE) && !IsScanRoot()) return StorageSource::DisplayName(GetNameView());
    return std::wstring(GetNameView(stripDrivePrefix));
}

std::wstring_view CItem::GetNameView(const bool stripDrivePrefix) const noexcept
{
    if (IsTypeOrFlag(IT_HLINKS_FILE)) return GetLinkedItem()->GetNameView(stripDrivePrefix);
    if (stripDrivePrefix && IsTypeOrFlag(IT_DRIVE))
    {
        return std::wstring_view(GetNameBuffer(), m_nameLen).substr(std::size(L"?:"));
    }
    return { GetNameBuffer(), m_nameLen };
}

bool CItem::HasExtension(const std::wstring_view extension) const noexcept
{
    if (!IsTypeOrFlag(IT_FILE)) return false;

    const auto name = GetNameView();
    if (extension.empty() || extension.size() > name.size()) return false;

    const auto suffix = name.substr(name.size() - extension.size());
    return _wcsnicmp(suffix.data(), extension.data(), extension.size()) == 0;
}

std::wstring CItem::GetExtension() const
{
    if (!IsTypeOrFlag(IT_FILE)) return GetName();
    const auto extName = GetNameView();
    const auto pos = extName.rfind('.');
    if (pos == std::wstring_view::npos) return {};
    std::wstring extLower(extName.substr(pos));
    _wcslwr_s(extLower.data(), extLower.size() + 1);
    return extLower;
}

std::wstring CItem::GetPath() const
{
    if (IsTypeOrFlag(ITF_REMOTE)) return StorageSource::ItemPath(this);
    if (IsTypeOrFlag(IT_HLINKS_FILE)) return GetLinkedItem()->GetPath();

    if (IsTypeOrFlag(IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        return {};
    }

    // Reconstruct MTP paths from the registered root and item ancestry
    if (IsTypeOrFlag(ITF_MTP))
    {
        thread_local std::vector<const CItem*> pathParts;
        pathParts.clear();
        const CItem* root = GetEnumRoot();
        for (const CItem* part = this; part != root; part = part->GetParent())
        {
            pathParts.push_back(part);
        }

        std::wstring path = root->IsTypeOrFlag(ITF_MTP) ? FinderMtp::GetPath(root->m_index) : std::wstring{};
        if (!path.empty())
        {
            for (const CItem* part : pathParts | std::views::reverse)
            {
                if (!path.ends_with(L'\\')) path += L'\\';
                path += part->GetNameView();
            }
            return path;
        }
    }

    std::wstring path = GetPathWithoutSlash();
    if (IsTypeOrFlag(IT_DRIVE))
    {
        path += L"\\";
    }
    return path;
}

int CItem::ComparePath(const CItem* other) const
{
    if (!other) return 1;

    auto getValid = [](const CItem* item) -> const CItem*
    {
        while (item && item->IsTypeOrFlag(IT_MYCOMPUTER, IT_HLINKS_SET, IT_HLINKS_IDX))
        {
            item = item->GetParent();
        }

        return item;
    };

    const CItem* leftRoot = getValid(this);
    const CItem* rightRoot = getValid(other);

    if (!leftRoot || !rightRoot) return (leftRoot > rightRoot) - (leftRoot < rightRoot);

    thread_local std::vector<const CItem*> leftPath;
    thread_local std::vector<const CItem*> rightPath;

    leftPath.clear();
    rightPath.clear();

    for (const CItem* p = leftRoot; p; p = getValid(p->GetParent())) leftPath.push_back(p);
    for (const CItem* p = rightRoot; p; p = getValid(p->GetParent())) rightPath.push_back(p);

    size_t leftIdx = leftPath.size();
    size_t rightIdx = rightPath.size();

    // Walk from root toward leaves by indexing backwards.
    while (leftIdx > 0 && rightIdx > 0 && leftPath[leftIdx - 1] == rightPath[rightIdx - 1])
    {
        --leftIdx;
        --rightIdx;
    }

    // One path is an ancestor of the other.
    if (leftIdx == 0 || rightIdx == 0)
    {
        if (leftIdx == 0 && rightIdx == 0) return 0;
        return leftIdx == 0 ? -1 : 1;
    }

    const CItem* left = leftPath[leftIdx - 1];
    const CItem* right = rightPath[rightIdx - 1];
    auto getSlice = [](const CItem* item)
    {
        const auto view = item->GetNameView();
        if (item->IsTypeOrFlag(IT_DRIVE)) return view.substr(0, 2);
        return view;
    };

    const auto leftView = getSlice(left);
    const auto rightView = getSlice(right);

    const size_t minLen = (std::min)(leftView.size(), rightView.size());
    if (const int cmp = left->IsTypeOrFlag(ITF_REMOTE) && right->IsTypeOrFlag(ITF_REMOTE) ?
        leftView.compare(rightView) : _wcsnicmp(leftView.data(), rightView.data(), minLen))
    {
        return (cmp > 0) - (cmp < 0);
    }

    return (leftView.size() > rightView.size()) - (leftView.size() < rightView.size());
}

std::wstring CItem::GetPathLong() const
{
    if (!SupportsFilesystemApis()) return GetPath();
    return FinderBasic::MakeLongPathCompatible(GetPath());
}

std::wstring CItem::GetFolderPath() const
{
    if (!SupportsFilesystemApis())
        return IsTypeOrFlag(IT_FILE) && GetParent() != nullptr ? GetParent()->GetPath() : GetPath();

    std::wstring path = GetPath();
    if (IsTypeOrFlag(IT_FILE))
    {
        const auto i = path.find_last_of(wds::chrBackslash);
        assert(i != std::wstring::npos);
        path = path.substr(0, i + 1);
    }

    return path;
}

std::wstring CItem::GetIconPath() const
{
    // Items without a filesystem path resolve their icon by file type through a synthetic path.
    return SupportsFilesystemApis() || IsMtpRoot() ? GetPath() :
        L"C:\\~" + (IsTypeOrFlag(IT_FILE) ? GetExtension() : std::wstring{});
}

bool CItem::HasUncPath() const
{
    if (!SupportsFilesystemApis()) return false;
    return GetPath().starts_with(L"\\\\");
}

CItem* CItem::FindItemByPath(const std::wstring& path, const bool findAncestor, const ITEMTYPE kind) const
{
    CItem* root = GetEnumRoot();
    const auto roots = root->IsTypeOrFlag(IT_MYCOMPUTER) ?
        std::span(root->GetChildren()) : std::span<CItem* const>(&root, 1);
    if (StorageSource::IsPath(path))
    {
        // Descend by exact names; a directory and its marker object share one URI and differ only in kind.
        const auto [address, options] = StorageSource::SplitOptions(path);
        for (CItem* item : roots)
        {
            const auto [rootAddress, rootOptions] = StorageSource::SplitOptions(item->GetNameView());
            if (!item->IsTypeOrFlag(ITF_REMOTE) || rootOptions != options || !address.starts_with(rootAddress))
                continue;
            const auto child = [&item](const std::wstring_view name, const bool leaf) -> CItem*
            {
                const auto& children = item->GetChildren();
                const auto found = std::ranges::find_if(children,
                    [&](const CItem* entry) { return entry->IsLeaf() == leaf && entry->GetNameView() == name; });
                return found == children.end() ? nullptr : *found;
            };
            auto names = address.substr(rootAddress.size());
            for (auto slash = names.find(L'/'); slash != names.npos; slash = names.find(L'/'))
            {
                CItem* directory = child(names.substr(0, slash), false);
                if (directory == nullptr) return findAncestor ? item : nullptr;
                item = directory;
                names.remove_prefix(slash + 1);
            }
            if (names.empty() && kind != IT_FILE) return item;
            CItem* file = kind == IT_DIRECTORY ? nullptr : child(names, true);
            return file != nullptr || !findAncestor ? file : item;
        }
        return nullptr;
    }

    // Find the most specific enumeration root containing the requested path.
    CItem* current = nullptr;
    size_t rootLength = 0;
    for (CItem* candidate : roots)
    {
        std::wstring rootPath = candidate->GetPath();
        while (!rootPath.empty() && rootPath.back() == L'\\') rootPath.pop_back();
        if (rootPath.size() <= rootLength || path.size() < rootPath.size() ||
            _wcsnicmp(path.c_str(), rootPath.c_str(), rootPath.size()) != 0 ||
            (path.size() > rootPath.size() && path[rootPath.size()] != L'\\')) continue;
        current = candidate;
        rootLength = rootPath.size();
    }
    if (current == nullptr) return nullptr;

    // Split the path into components, filtering out empty strings
    for (const auto part : std::views::split(std::wstring_view(path).substr(rootLength), wds::chrBackslash))
    {
        const std::wstring_view component(part.begin(), part.end());
        if (component.empty()) continue;
        if (current->IsLeaf()) return findAncestor ? current : nullptr;

        // Find the matching child using GetNameView for comparison
        auto it = std::ranges::find_if(current->GetChildren(),
            [&](const CItem* child) { return child->GetNameView() == component; });
        if (it == current->GetChildren().end()) return findAncestor ? current : nullptr;
        current = *it;
    }

    return current;
}

std::wstring CItem::GetPathWithoutSlash() const
{
    // create vector of the path structure in thread scope so we can reverse it
    thread_local std::vector<const CItem*> pathParts;
    thread_local std::wstring path;

    // make sure the vector is cleared before use
    pathParts.clear();
    path.clear();

    // walk backwards to get a list of pointers to each part of the path
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        pathParts.emplace_back(p);
    }

    // append the strings in reverse order
    for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it) [[msvc::forceinline_calls]]
    {
        if (const auto & pathPart = *it; pathPart->IsTypeOrFlag(IT_DIRECTORY))
        {
            path.append(pathPart->GetNameBuffer(), pathPart->m_nameLen).append(L"\\");
        }
        else if (pathPart->IsTypeOrFlag(IT_DRIVE))
        {
            path.append(pathPart->GetNameBuffer(), 2).append(L"\\");
        }
        else if (!pathPart->IsTypeOrFlag(IT_MYCOMPUTER))
        {
            path.append(pathPart->GetNameBuffer(), pathPart->m_nameLen);
        }
    }

    // Remove trailing backslashes
    if (const auto pos = path.find_last_not_of(L'\\'); pos != std::wstring::npos)
    {
        path.erase(pos + 1);
    }
    return path;
}

// --- Scanning & Done State ---

void CItem::SetDone()
{
    if (IsDone())
    {
        return;
    }

    if (SupportsSpaceItems())
    {
        UpdateFreeSpaceItem();
        UpdateUnknownItem();
    }

    // Sort and set finish time
    if (!IsLeaf())
    {
        COptions::TreeMapUseLogical ? SortItemsBySizeLogical() : SortItemsBySizePhysical();
        GetFolderInfo()->m_tfinish = GetScanTickCount();
    }

    // Mark as done just so other functions do not sort at the same time
    SetFlag(ITF_DONE);
}

void CItem::UpwardSetDone() noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->SetDone();
    }
}

void CItem::UpwardSetUndone() noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->SupportsSpaceItems() && p->IsDone())
        {
            if (CItem* unknown = p->FindUnknownItem(); unknown != nullptr)
            {
                p->UpwardSubtractSizePhysical(unknown->GetSizePhysical());
                unknown->SetSizePhysical(0);
            }
        }

        p->SetFlag(ITF_DONE, true);
    }
}

void CItem::UpwardAddReadJobs(const ULONG count) const noexcept
{
    if (IsLeaf() || count == 0) return;
    if (GetFolderInfo()->m_jobs == 0) GetFolderInfo()->m_tstart = GetScanTickCount();
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->GetFolderInfo()->m_jobs += count;
    }
}

void CItem::UpwardSubtractReadJobs(const ULONG count) noexcept
{
    if (count == 0 || IsTypeOrFlag(IT_FILE)) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->GetFolderInfo()->m_jobs.fetch_sub(count) == count)
        {
            p->SetDone();
        }
    }
}

ULONGLONG CItem::GetTicksWorked() const noexcept
{
    if (IsLeaf()) return 0;
    return GetFolderInfo()->m_tfinish > 0 ? (GetFolderInfo()->m_tfinish - GetFolderInfo()->m_tstart) :
        (GetFolderInfo()->m_tstart > 0) ? (GetScanTickCount() - GetFolderInfo()->m_tstart) : 0;
}

void CItem::ResetScanStartTime() const noexcept
{
    if (IsLeaf()) return;
    GetFolderInfo()->m_tfinish = 0;
    GetFolderInfo()->m_tstart = GetScanTickCount();
}

ULONG CItem::GetScanTickCount() noexcept
{
    const ULONGLONG state = scanClockState.load(std::memory_order_relaxed);
    const ULONGLONG activeMilliseconds = (state & SCAN_CLOCK_SUSPENDED) != 0 ?
        state & ~SCAN_CLOCK_SUSPENDED : GetTickCount64() - state;
    return static_cast<ULONG>(activeMilliseconds / 1000ull);
}

void CItem::SuspendScanClock() noexcept
{
    const ULONGLONG state = scanClockState.load(std::memory_order_relaxed);
    if ((state & SCAN_CLOCK_SUSPENDED) != 0) return;

    const ULONGLONG activeMilliseconds = GetTickCount64() - state;
    assert((activeMilliseconds & SCAN_CLOCK_SUSPENDED) == 0);
    scanClockState.store(SCAN_CLOCK_SUSPENDED | activeMilliseconds, std::memory_order_relaxed);
}

void CItem::ResumeScanClock() noexcept
{
    const ULONGLONG state = scanClockState.load(std::memory_order_relaxed);
    if ((state & SCAN_CLOCK_SUSPENDED) == 0) return;

    const ULONGLONG activeMilliseconds = state & ~SCAN_CLOCK_SUSPENDED;
    scanClockState.store(GetTickCount64() - activeMilliseconds, std::memory_order_relaxed);
}

void CItem::SortItemsBySizePhysical() const
{
    if (IsLeaf()) return;

    // sort by size for proper treemap rendering
    const auto sort = [this]
    {
        GetFolderInfo()->m_children.shrink_to_fit();
        std::ranges::sort(GetFolderInfo()->m_children, std::ranges::greater{}, &CItem::GetSizePhysical);
    };
    if (!MutateHiddenChildren(sort)) CMainFrame::Get()->InvokeInMessageThread(sort);
}

void CItem::SortItemsBySizeLogical() const
{
    if (IsLeaf()) return;

    // sort by size for proper treemap rendering
    const auto sort = [this]
    {
        GetFolderInfo()->m_children.shrink_to_fit();
        std::ranges::sort(GetFolderInfo()->m_children, std::ranges::greater{}, &CItem::GetSizeLogical);
    };
    if (!MutateHiddenChildren(sort)) CMainFrame::Get()->InvokeInMessageThread(sort);
}

void CItem::UpdateStatsFromDisk()
{
    assert(!IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) || GetParentDrive() == nullptr ||
        FindHardlinksItem() == nullptr);

    // Remote metadata is supplied by its enumeration backend.
    if (!SupportsFilesystemApis()) return;

    if (IsTypeOrFlag(IT_DIRECTORY, IT_FILE))
    {
        FinderBasic finder(true);
        // A scan root stores its full path as its name, so it must be looked up without a name filter
        if (finder.FindFile(GetFolderPath(), IsScanRoot() ? std::wstring() : GetName(), GetAttributes()))
        {
            SetLastChange(finder.GetLastWriteTime());
            SetAttributes(finder.GetAttributes());

            if (IsTypeOrFlag(IT_FILE))
            {
                ExtensionDataRemove();
                UpwardSubtractSizePhysical(GetSizePhysicalRaw());
                UpwardSubtractSizeLogical(GetSizeLogical());
                UpwardAddSizePhysical(finder.GetFileSizePhysical());
                UpwardAddSizeLogical(finder.GetFileSizeLogical());
                SetIndex(finder.GetIndex());
                ExtensionDataAdd();
            }
        }
        else if (IsScanRoot() && GetAttributes() == INVALID_FILE_ATTRIBUTES)
        {
            // Correct potentially invalid attributes on root items
            if (const DWORD attr = GetFileAttributes(GetPathLong().c_str()); attr != INVALID_FILE_ATTRIBUTES)
            {
                SetAttributes(attr);
            }
        }
    }
    else if (IsTypeOrFlag(IT_DRIVE))
    {
        if (const SmartPointer handle(CloseHandle, CreateFile(GetPathLong().c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr)); handle != INVALID_HANDLE_VALUE)
        {
            FILETIME lastChange{};
            if (GetFileTime(handle, nullptr, nullptr, &lastChange)) SetLastChange(lastChange);
        }
    }
}

void CItem::ScanRemoteFlat(FinderRemote& finder, BlockingQueue<CItem*>* queue)
{
    // Build the hierarchy directly from flat pages without scheduling synthetic prefixes for network reads.
    std::unordered_map<std::wstring, CItem*> directories{ { L"", this } };
    std::unique_ptr<ScanBatch> batch;
    CItem* batchParent = nullptr;
    finder.UseFlatListing(true);
    for (bool found = finder.FindFile(this); found; found = finder.FindNext())
    {
        queue->WaitIfSuspended();
        const std::wstring relative = finder.GetRelativeKey();
        size_t start = 0;
        CItem* parent = this;
        for (size_t slash = relative.find(L'/'); slash != relative.npos; slash = relative.find(L'/', start))
        {
            const auto prefix = relative.substr(0, slash + 1);
            if (const auto foundDirectory = directories.find(prefix); foundDirectory != directories.end())
                parent = foundDirectory->second;
            else
            {
                auto* child = CItem::Create(IT_DIRECTORY,
                    StorageSource::EscapeName(relative.substr(start, slash - start)));
                child->SetAttributes(FILE_ATTRIBUTE_DIRECTORY);
                ScanBatch directoryBatch(parent, queue);
                directoryBatch.Add(child);
                directoryBatch.Flush();
                directories.emplace(prefix, child);
                parent = child;
            }
            start = slash + 1;
        }
        finder.SetEntryName(relative.substr(start));
        if (finder.IsDirectory())
        {
            if (start == relative.size()) parent->UpwardUpdateLastChange(finder.GetLastWriteTime());
            else
            {
                const auto prefix = relative + L"/";
                auto [entry, inserted] = directories.try_emplace(prefix, nullptr);
                if (inserted)
                {
                    auto* child = CItem::Create(IT_DIRECTORY, StorageSource::EscapeName(finder.GetFileName()));
                    child->SetAttributes(FILE_ATTRIBUTE_DIRECTORY);
                    ScanBatch directoryBatch(parent, queue);
                    directoryBatch.Add(child);
                    directoryBatch.Flush();
                    entry->second = child;
                }
                entry->second->UpwardUpdateLastChange(finder.GetLastWriteTime());
            }
            continue;
        }
        if (CFiltering::IsFilterActive() && CFiltering::IsFilteredOut(finder.GetFileName(), {},
            finder.GetFileSizeLogical(), finder.GetLastWriteTime())) continue;
        if (batchParent != parent)
        {
            batch = std::make_unique<ScanBatch>(parent, queue);
            batchParent = parent;
        }
        parent->AddFile(finder, *batch);
        batch->FlushIfNeeded();
    }
    batch.reset();

    // A flat page failure leaves the listed subtree incomplete; successful pages remain visible.
    for (auto* directory : directories | std::views::values)
    {
        directory->SetScanResult({ finder.GetResult().outcome, directory == this ? finder.GetResult().error : L"" });
        if (directory != this) directory->SetDone();
    }
}

void CItem::ScanItems(BlockingQueue<CItem*> * queue, FinderNtfsContext& contextNtfs, FinderBasicContext& contextBasic,
    std::unordered_map<const CItem*, FinderBasicContext>* folderContexts,
    const std::shared_ptr<StorageSource>& remoteSource)
{
    // Reuse storage finders throughout this worker.
    FinderNtfs finderNtfs(&contextNtfs);
    FinderMtp finderMtp;
    FinderRemote finderRemote(remoteSource, queue);

    for (auto itemOpt = queue->Pop(); itemOpt.has_value(); itemOpt = queue->Pop())
    {
        // Fetch item from queue
        CItem* const item = itemOpt.value();

        // Mark the time we started evaluating this node
        item->ResetScanStartTime();

        if (item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY) && CFiltering::IsFilterActive() &&
            CFiltering::IsFilteredOut(item->GetPath(), item->IsTypeOrFlag(ITF_REMOTE) && item->IsScanRoot()))
        {
            item->SetScanResult({ ScanOutcome::Complete, {} });
            item->UpwardSubtractReadJobs(1);
            item->UpwardDrivePacman();
            continue;
        }

        // Try to load NTFS MFT
        if (item->IsTypeOrFlag(IT_DRIVE) && COptions::UseFastScanEngine)
        {
            contextNtfs.LoadRoot(item, queue);
        }

        if (item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY))
        {
            auto* basicContext = &contextBasic;
            if (folderContexts != nullptr)
            {
                // Descendants keep the context of the root initially assigned to this queue.
                const CItem* root = item->GetVolumeRoot();
                auto context = folderContexts->find(root);
                while (context == folderContexts->end()) context = folderContexts->find(root = root->GetParent());
                basicContext = &context->second;
            }
            FinderBasic finderBasic(basicContext);

            // Select the enumeration backend for the queued item
            Finder* finder = item->IsTypeOrFlag(ITF_REMOTE) ? static_cast<Finder*>(&finderRemote) :
                item->IsTypeOrFlag(ITF_MTP) ? static_cast<Finder*>(&finderMtp) :
                contextNtfs.IsLoaded() && !item->IsTypeOrFlag(ITF_BASIC) ?
                static_cast<Finder*>(&finderNtfs) : static_cast<Finder*>(&finderBasic);

            const bool flat = item->IsTypeOrFlag(ITF_REMOTE) && remoteSource != nullptr &&
                remoteSource->capabilities.flatListing && CFiltering::IncludeDirsRegex.empty() &&
                CFiltering::ExcludeDirsRegex.empty();
            if (flat)
            {
                item->ScanRemoteFlat(finderRemote, queue);
                item->UpwardSubtractReadJobs(1);
                item->UpwardDrivePacman();
                continue;
            }
            finderRemote.UseFlatListing(false);

            // Filter filesystem children by the same paths their items report.
            const std::wstring childPrefix = CFiltering::IsFilterActive() && item->SupportsFilesystemApis() ?
                item->GetPathWithoutSlash() + L'\\' : std::wstring();
            const auto childPath = [&]
            {
                return childPrefix.empty() ? finder->GetFilePath() :
                    std::wstring(childPrefix).append(finder->GetFileName());
            };
            const bool directoryIncluded = CFiltering::CanReuseDirectoryMatch(childPrefix);
            ScanBatch batch(item, queue);
            for (bool b = finder->FindFile(item); b; b = finder->FindNext()) [[msvc::forceinline_calls]]
            {
                if (queue->IsPauseOrCancelRequested())
                {
                    batch.FlushIfNeeded(true);
                    queue->WaitIfSuspended();
                }
                if (finder->IsDirectory())
                {
                    if (COptions::ExcludeHiddenDirectory && finder->IsHidden() ||
                        COptions::ExcludeProtectedDirectory && finder->IsHiddenSystem() ||
                        COptions::ExcludeDropboxIgnored && finder->HasIgnoredStream() ||
                        CFiltering::IsFilterActive() && CFiltering::IsFilteredOut(childPath()))
                    {
                        continue;
                    }

                    item->AddDirectory(*finder, batch);
                }
                else
                {
                    // The descendant regex suffix does not match line terminators in filenames.
                    const bool reuseDirectoryMatch = directoryIncluded &&
                        finder->GetFileName().find_first_of(L"\r\n\u2028\u2029") == std::wstring_view::npos;
                    if (COptions::ExcludeHiddenFile && finder->IsHidden() ||
                        COptions::ExcludeProtectedFile && finder->IsHiddenSystem() ||
                        COptions::ExcludeSymbolicLinksFile && finder->GetReparseTag() == IO_REPARSE_TAG_SYMLINK ||
                        COptions::ExcludeDropboxIgnored && finder->HasIgnoredStream() ||
                        CFiltering::IsFilterActive() && CFiltering::IsFilteredOut(finder->GetFileName(),
                            reuseDirectoryMatch || CFiltering::IncludeDirsRegex.empty() ? std::wstring() : childPath(),
                            finder->GetFileSizeLogical(), finder->GetLastWriteTime(), reuseDirectoryMatch))
                    {
                        continue;
                    }

                    item->AddFile(*finder, batch);
                }

                // Update pacman position
                batch.FlushIfNeeded(queue->IsPauseOrCancelRequested());
                queue->WaitIfSuspended();
            }
            batch.FlushIfNeeded(true);
            if (item->IsTypeOrFlag(ITF_REMOTE))
                item->UpwardUpdateLastChange(finderRemote.GetDirectoryTime());

            // A directory removed after its parent listed it has nothing left to enumerate.
            const auto& result = finder->GetResult();
            item->SetScanResult(result.outcome == ScanOutcome::Missing && !item->IsScanRoot() ?
                ScanResult{ ScanOutcome::Complete, {} } : result);
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            // Only used for refreshes
            item->UpdateStatsFromDisk();
            CFileDupeControl::Get()->ProcessDuplicate(item, queue);
            CFileTopControl::Get()->ProcessTop(item);
            item->SetDone();
        }
        else if (item->IsTypeOrFlag(IT_MYCOMPUTER))
        {
            for (const auto & child : item->GetChildren())
            {
                child->UpwardAddReadJobs(1);
                queue->Push(child);
            }
        }
        item->UpwardSubtractReadJobs(1);
        item->UpwardDrivePacman();
    }
}

void CItem::ScanItemsFinalize(CItem* item)
{
    if (item == nullptr) return;
    std::vector queue({item});
    while (!queue.empty()) [[msvc::forceinline_calls]]
    {
        const auto & qitem = queue.back();
        queue.pop_back();
        qitem->SetDone();
        if (qitem->IsLeaf()) continue;
        for (const auto& child : qitem->GetChildren())
        {
            if (!child->IsDone()) queue.push_back(child);
        }
    }
}
