// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/d3d12/ResourceAllocationD3D12.h"

#include "src/d3d12/HeapD3D12.h"
#include "src/d3d12/ResidencyManagerD3D12.h"
#include "src/d3d12/ResourceAllocatorD3D12.h"

#include <utility>

namespace gpgmm { namespace d3d12 {
    ResourceAllocation::ResourceAllocation(ResidencyManager* residencyManager,
                                           MemoryAllocator* memoryAllocator,
                                           const AllocationInfo& info,
                                           uint64_t offset,
                                           ComPtr<ID3D12Resource> resource,
                                           Heap* heap)
        : MemoryAllocation(memoryAllocator, info, offset, heap),
          mResidencyManager(residencyManager),
          mResource(std::move(resource)) {
    }

    void ResourceAllocation::ReleaseThis() {
        if (this == nullptr) {
            return;
        }

        if (GetAllocator() != nullptr) {
            GetAllocator()->DeallocateMemory(*this);
        }

        mResource.Reset();
        MemoryAllocation::Reset();
    }

    ID3D12Resource* ResourceAllocation::GetResource() const {
        return mResource.Get();
    }

    HRESULT ResourceAllocation::SetDebugName(const char* name) {
        return mResource->SetPrivateData(WKPDID_D3DDebugObjectName, std::strlen(name), name);
    }

    HRESULT ResourceAllocation::Map(uint32_t subresource,
                                    const D3D12_RANGE* pRange,
                                    void** ppMappedData) {
        Heap* heap = static_cast<Heap*>(GetMemory());
        if (heap == nullptr) {
            return E_INVALIDARG;
        }

        if (mResidencyManager != nullptr) {
            HRESULT hr = mResidencyManager->LockHeap(heap);
            if (FAILED(hr)) {
                return hr;
            }
        }

        return mResource->Map(subresource, pRange, ppMappedData);
    }

    void ResourceAllocation::Unmap(uint32_t subresource, const D3D12_RANGE* pRange) {
        Heap* heap = static_cast<Heap*>(GetMemory());
        if (heap == nullptr) {
            return;
        }
        if (mResidencyManager != nullptr) {
            mResidencyManager->UnlockHeap(heap);
        }

        mResource->Unmap(subresource, pRange);
    }

    void ResourceAllocation::UpdateResidency(ResidencySet* residencySet) {
        Heap* heap = static_cast<Heap*>(GetMemory());
        ASSERT(heap != nullptr);
        heap->UpdateResidency(residencySet);
    }
}}  // namespace gpgmm::d3d12
