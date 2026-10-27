#pragma once
#include <d3d12.h>
#include <cassert>
#include <cstdint>
#include <vector>

class SrvAllocator {
public:
    SrvAllocator() = default;

    void Init(ID3D12Device* device, ID3D12DescriptorHeap* heap, UINT reserved = 1) {
        assert(device);
        assert(heap);

        device_ = device;
        heap_ = heap;

        const auto desc = heap_->GetDesc();
        capacity_ = desc.NumDescriptors;

        inc_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        reserved_ = reserved;
        next_ = reserved_;  // 0 は ImGui 予約などで使う想定
        freeList_.clear();
    }

    UINT Allocate() {
        assert(device_ && heap_);

        if (!freeList_.empty()) {
            const UINT index = freeList_.back();
            freeList_.pop_back();
            return index;
        }

        assert(next_ < capacity_ && "SRV heap is full. Increase NumDescriptors.");
        return next_++;
    }

    void Free(UINT index) {
        // 予約領域は解放対象にしない
        if (index < reserved_) {
            return;
        }
        // まだ Allocate されてない範囲の index は無効
        if (index >= next_) {
            return;
        }
        freeList_.push_back(index);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT index) const {
        assert(heap_);
        D3D12_CPU_DESCRIPTOR_HANDLE base = heap_->GetCPUDescriptorHandleForHeapStart();
        base.ptr += static_cast<SIZE_T>(index) * inc_;
        return base;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT index) const {
        assert(heap_);
        D3D12_GPU_DESCRIPTOR_HANDLE base = heap_->GetGPUDescriptorHandleForHeapStart();

        base.ptr += static_cast<SIZE_T>(index) * inc_;
        return base;
    }

    // いま使われている個数(予約領域と、返却済みの空きを除く)。ヒープが尽きないかの確認用(シーンの開始時に、ログへ出す)
    UINT UsedCount() const { return next_ - reserved_ - static_cast<UINT>(freeList_.size()); }

    UINT Capacity() const { return capacity_; }
    UINT Reserved() const { return reserved_; }

private:
    ID3D12Device* device_ = nullptr;
    ID3D12DescriptorHeap* heap_ = nullptr;
    UINT inc_ = 0;

    UINT capacity_ = 0;
    UINT reserved_ = 1;
    UINT next_ = 1;

    std::vector<UINT> freeList_;
};
