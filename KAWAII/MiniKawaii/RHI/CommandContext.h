#pragma once

#include "DescriptorHeap.h"
#include "GpuBuffer.h"
#include "DynamicResource.h"
#include "PipelineState.h"

#define ContextPoolSize 4
#define AvailableContextSize 4

namespace RHI
{
	class CommandContext;

	// Compute Command List only supports the transition of these states
#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
	    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
	    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
		| D3D12_RESOURCE_STATE_COPY_DEST \
		| D3D12_RESOURCE_STATE_COPY_SOURCE )

	class ContextManager : public Singleton<ContextManager>
	{
	public:
		CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE type);
		void FreeContext(CommandContext* usedContext);

	private:
		std::vector<std::unique_ptr<CommandContext>> m_ContextPool[ContextPoolSize];
		std::queue<CommandContext*> m_AvailableContexts[AvailableContextSize];
	};

	struct NonCopyable
	{
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	};

	/*
	* Collection of command list and command allocator
	* Calling the function "Begin" will request a Command Context and then you can start recording the command,
	* and call function "End" you will push the command into a Command Queue.
	* A new Command Allocator will be requeted at begin and this allocator will recycled at End
	* Each thread will use his own CommandContext
	*/
	class CommandContext : public NonCopyable
	{
		friend class ContextManager;
	private:
		CommandContext(D3D12_COMMAND_LIST_TYPE type);

		// Called when the CommandContext is created, this function will create the CommandList and request and Allocator
		void Initialize();
		// Called when the CommandContext is reused to reset the rendering state, this function will request an Allocator and call CL::Reset
		void Reset();

	public:
		~CommandContext();

		// Start recording commands
		static CommandContext& Begin(const std::wstring ID = L"");
		// Flush existing commands to the GPU but keep the context alive
		uint64_t Flush(bool WaitForCompletion = false);
		// Finish the command record
		uint64_t Finish(bool waitForCompletion = false, bool releaseDynamic = false);

		static void InitializeBuffer(GpuBuffer& dest, const void* data, size_t numBytes, size_t destOffset = 0);
		static void InitializeBuffer(GpuBuffer& dest, const GpuUploadBuffer Src, size_t srcOffset, size_t numBytes = -1, size_t destOffset = 0);
		static void InitializeTexture(GpuResource& dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA subData[]);

		// Allocate memory from Dynamic Resource
		D3D12DynamicAllocation AllocateDynamicSpace(size_t numByte, size_t alignment);
		// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
		// GpuResource has two members : 
		// m_UsageState : Represent the current state of the resource. When TransitionResource is called, the current state of the resource
		// will be check with m_UsageState. If it not equal, submit the resource barrier.
		// m_TransitionState : Indicates the state of the resource in transition, used the split barrier to mark that resource has begin the Barrier
		// So try to cache to 16 and then execute them together (FlushResourceBarriers) ?
		void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool FlushImmediate = true);
		void FlushResourceBarriers(void);

	protected:
		void SetID(const std::wstring& ID) { m_ID = ID; }
		// Command list type
		D3D12_COMMAND_LIST_TYPE m_type;
		// Command List (Command list is held by Command Contex, Command Allocator is manage by object pool)
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		ID3D12CommandAllocator* m_CurrentAllocator;

		// Resource Barrier(Flush at 16)
		D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
		UINT m_numBarriersToFlush;

		// Dynamic Descriptor
		DynamicSuballocationsManager m_DynamicGPUDescriptorAllocator;

		// Dynamic Resource
		DynamicResourceHeap m_DynamicResourceHeap;

		// Resource Binding
		PipelineState* m_curPSO = nullptr;

		std::wstring  m_ID;
	};
}