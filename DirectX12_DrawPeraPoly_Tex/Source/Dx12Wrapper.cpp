#include "Dx12Wrapper.h"
#include "Application.h"

#include <assert.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib ")

using namespace DirectX;


Dx12Wrapper::Dx12Wrapper(HWND windowH) :_WindowH(windowH)
{
	//DXGI
	_DxgiFactory = nullptr;
	_DxgiSwapChain = nullptr;

	//デバイス
	_Dev = nullptr;

	//コマンド系
	//コマンドリスト確保用オブジェクト
	_CmdAlloc = nullptr;
	//コマンドリスト本体
	_CmdList = nullptr;
	//コマンドリストのキュー
	_CmdQueue = nullptr;

	//フェンスオブジェクト
	_Fence = nullptr;
	//フェンス値
	_FenceValue = 0;

	//ディスクリプタヒープ
	_Rtv_DescriptorHeap = nullptr;

	//頂点バッファ
	_VertexBuffer = nullptr;
	//インデックスバッファ
	_IndexBuffer = nullptr;

	//座標変換定数バッファ
	_TransformCB = nullptr;
	//座標変換ヒープ
	_TransfromHeap = nullptr;


	//ルートシグネチャ
	_RootSigunature = nullptr;
	//シグネチャ
	_Signature = nullptr;


	//テクスチャバッファ
	_TextureBuffer = nullptr;
	//テクスチャヒープ
	_TextureHeap = nullptr;

	//エラー対処用
	_Error = nullptr;

	//パイプラインステート
	_PipelineState = nullptr;

	//ビューポート
	SetViewPort();
}

Dx12Wrapper::~Dx12Wrapper()
{
}

bool Dx12Wrapper::Init()
{
#if _DEBUG
	ID3D12Debug* Debug;
	D3D12GetDebugInterface(IID_PPV_ARGS(&Debug));
	Debug->EnableDebugLayer();
	Debug->Release();
#endif

	HRESULT Result = S_OK;

	D3D_FEATURE_LEVEL Levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	for (auto level : Levels)
	{
		if (SUCCEEDED(D3D12CreateDevice(nullptr, level, IID_PPV_ARGS(_Dev.ReleaseAndGetAddressOf()))))
		{
			break;
		}
	}


	if (_Dev == nullptr)
	{
		return false;
	}

#if _DEBUG

	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_DxgiFactory.ReleaseAndGetAddressOf()))))
	{
		return false;
	}
#else

	if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(_DxgiFactory.ReleaseAndGetAddressOf()))))
	{
		return false;
	}
#endif

	//コマンドキューを作成

	D3D12_COMMAND_QUEUE_DESC CmdQueueDesc{};
	CmdQueueDesc.NodeMask = 0;
	CmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	CmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	CmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;


	if (FAILED(_Dev->CreateCommandQueue(&CmdQueueDesc, IID_PPV_ARGS(_CmdQueue.ReleaseAndGetAddressOf()))))
	{
		return false;
	}


	//スワップチェインを作成
	Size WindowSize = Application::Instance().GetWindowSize();

	DXGI_SWAP_CHAIN_DESC1 SwDesc = {};
	SwDesc.BufferCount = 2;
	SwDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	SwDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwDesc.Flags = 0;
	SwDesc.Width = WindowSize.Width;
	SwDesc.Height = WindowSize.Height;
	SwDesc.SampleDesc.Count = 1;
	SwDesc.SampleDesc.Quality = 0;
	SwDesc.Stereo = false;
	SwDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwDesc.Scaling = DXGI_SCALING_STRETCH;

	Result = _DxgiFactory->CreateSwapChainForHwnd(_CmdQueue.Get(), _WindowH, &SwDesc, nullptr, nullptr, (IDXGISwapChain1**)(_DxgiSwapChain.ReleaseAndGetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}


	//フェンスを作成
	Result = _Dev->CreateFence(_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_Fence.ReleaseAndGetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}


	if (!CreateCommandList())
	{
		return false;
	}

	if (!CreateRenderTargetView())
	{
		return false;
	}

	if (!CreateVertexBuffer())
	{
		return false;
	}

	if (!CreateIndexBuffer())
	{
		return false;
	}

	if (!LoadPictureFromFile())
	{
		return false;
	}

	if (!CreateTransformConstantBuffer())
	{
		return false;
	}

	if (!LoadShader())
	{
		return false;
	}

	if (!CreateRootSignature())
	{
		return false;
	}

	if (!CreatePipeLineState())
	{
		return false;
	}


	return true;
}


void Dx12Wrapper::ExecuteCommand()
{
	//ルートシグネチャをセット
	_CmdList->SetGraphicsRootSignature(_RootSigunature.Get());


	//テクスチャ
	_CmdList->SetDescriptorHeaps(1, _TextureHeap.GetAddressOf());
	_CmdList->SetGraphicsRootDescriptorTable(0, _TextureHeap->GetGPUDescriptorHandleForHeapStart());

	//座標
	_CmdList->SetDescriptorHeaps(1, _TransfromHeap.GetAddressOf());
	_CmdList->SetGraphicsRootDescriptorTable(1, _TransfromHeap->GetGPUDescriptorHandleForHeapStart());

	//ビューポートをセット
	_CmdList->RSSetViewports(1, &_ViewPort);

	//シザーをセット
	_CmdList->RSSetScissorRects(1, &_ScissorRect);

	//パイプラインをセット
	_CmdList->SetPipelineState(_PipelineState.Get());

	//描画
	Draw();

	//リソースバリア
	AddBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_CmdList->Close();
	ID3D12CommandList* Cmds[] = { _CmdList.Get() };
	_CmdQueue->ExecuteCommandLists(1, Cmds);
	++_FenceValue;

	_CmdQueue->Signal(_Fence.Get(), _FenceValue);
	WaitWithFence();

	_CmdAlloc->Reset();
	_CmdList->Reset(_CmdAlloc.Get(), _PipelineState.Get());
}

bool Dx12Wrapper::ScreenCrear()
{
	auto BackBufferIdx = _DxgiSwapChain->GetCurrentBackBufferIndex();
	auto HeapPointer = _Rtv_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	HeapPointer.ptr += BackBufferIdx * _Dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//バリア追加
	AddBarrier(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//レンダリング
	_CmdList->OMSetRenderTargets(1, &HeapPointer, false, nullptr);

	//RGBA指定　最大1
	float ClsColor[4] = { 0.8f,0.8f, 0.5f,0.0 };
	_CmdList->ClearRenderTargetView(HeapPointer, ClsColor, 0, nullptr);

	return true;
}


void Dx12Wrapper::Draw()
{
	//インデックスバッファをセット
	_CmdList->IASetIndexBuffer(&_IndexBufferView);
	_CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//頂点バッファをセット
	_CmdList->IASetVertexBuffers(0, 1, &_VertexBufferView);

	//描画
	_CmdList->DrawIndexedInstanced(Indices.size(), 1, 0, 0, 0);
}

void Dx12Wrapper::ScreenFlip()
{

	ExecuteCommand();

	HRESULT Result = S_OK;
	Result = _DxgiSwapChain->Present(1, 0);
	assert(SUCCEEDED(Result));
}

void Dx12Wrapper::WaitWithFence()
{
	//ポーリング待機
	while (_Fence->GetCompletedValue() != _FenceValue)
	{
		auto Event = CreateEvent(nullptr, false, false, nullptr);
		_Fence->SetEventOnCompletion(_FenceValue, Event);
		WaitForSingleObject(Event, INFINITE);	//ここで待機。
		CloseHandle(Event);
	}
}

void Dx12Wrapper::AddBarrier(Microsoft::WRL::ComPtr<ID3D12Resource>& Buffer, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
{
	//リソースバリアを設定
	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.StateBefore = StateBefore;
	BarrierDesc.Transition.StateAfter = StateAfter;
	BarrierDesc.Transition.pResource = Buffer.Get();
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//レンダリング前にバリアを追加
	_CmdList->ResourceBarrier(1, &BarrierDesc);
}

void Dx12Wrapper::AddBarrier(D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
{
	//リソースバリアを設定
	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.StateBefore = StateBefore;
	BarrierDesc.Transition.StateAfter = StateAfter;
	BarrierDesc.Transition.pResource = _BackBuffers[_DxgiSwapChain->GetCurrentBackBufferIndex()].Get();
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//レンダリング前にバリアを追加
	_CmdList->ResourceBarrier(1, &BarrierDesc);
}

bool Dx12Wrapper::CreateCommandList()
{
	HRESULT Result = S_OK;

	//コマンドアロケータ作成
	Result = _Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_CmdAlloc.ReleaseAndGetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}

	//コマンドリスト作成
	Result = _Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _CmdAlloc.Get(), nullptr, IID_PPV_ARGS(_CmdList.ReleaseAndGetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateRenderTargetView()
{
	HRESULT Result = S_OK;

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc;

	Result = _DxgiSwapChain->GetDesc1(&SwapChainDesc);

	if (FAILED(Result))
	{
		return false;
	}

	//表示画面メモリ確保(デスクリプタヒープ作成)
	D3D12_DESCRIPTOR_HEAP_DESC DescripterHeapDesc{};
	DescripterHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	DescripterHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DescripterHeapDesc.NodeMask = 0;
	DescripterHeapDesc.NumDescriptors = SwapChainDesc.BufferCount;

	Result = _Dev->CreateDescriptorHeap(&DescripterHeapDesc, IID_PPV_ARGS(_Rtv_DescriptorHeap.ReleaseAndGetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}

	_BackBuffers.resize(SwapChainDesc.BufferCount);

	int DescripterSize = _Dev->GetDescriptorHandleIncrementSize(DescripterHeapDesc.Type);

	D3D12_CPU_DESCRIPTOR_HANDLE DescripterHandle = _Rtv_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC RtvDesc = {};
	RtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	//レンダーターゲットビューをヒープに紐づけ
	for (int i = 0; i < _BackBuffers.size(); i++)
	{
		Result = _DxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&_BackBuffers[i]));

		if (FAILED(Result))
		{
			break;
		}

		_Dev->CreateRenderTargetView(_BackBuffers[i].Get(), nullptr, DescripterHandle);
		DescripterHandle.ptr += DescripterSize;
	}

	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateVertexBuffer()
{
	HRESULT Result = S_OK;

	Vertices[0] = { { -1,-1	,0.0f } , {0,1,0} };
	Vertices[1] = { { -1,1	,0.0f } , {0,0,0} };
	Vertices[2] = { { 1	,-1	,0.0f } , {1,1,0} };
	Vertices[3] = { { 1	,1	,0.0f } , {1,0,0} };


	D3D12_HEAP_PROPERTIES HeapProp = {};

	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = sizeof(Vertices);
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Result = _Dev->CreateCommittedResource(&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&_VertexBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	//頂点Map作成
	Vertex* _VertexMap = nullptr;

	Result = _VertexBuffer->Map(0, nullptr, (void**)&_VertexMap);
	std::copy(std::begin(Vertices), std::end(Vertices), _VertexMap);
	_VertexBuffer->Unmap(0, nullptr);

	//頂点バッファビュー作成
	_VertexBufferView.BufferLocation = _VertexBuffer->GetGPUVirtualAddress();

	_VertexBufferView.StrideInBytes = sizeof(Vertices[0]);
	_VertexBufferView.SizeInBytes = sizeof(Vertices);

	return true;
}

bool Dx12Wrapper::CreateIndexBuffer()
{
	Indices = { 0,1,2,1,3,2 };
	HRESULT Result = S_OK;

	D3D12_HEAP_PROPERTIES HeapProp = {};

	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProp.CreationNodeMask = 1;
	HeapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Alignment = 0;
	ResourceDesc.Width = sizeof(Indices[0]) * Indices.size();
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Result = _Dev->CreateCommittedResource(&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&_IndexBuffer));

	if (FAILED(Result))
	{
		return false;
	}

	//IndexMap作成
	unsigned short* _IndexMap = nullptr;

	Result = _IndexBuffer->Map(0, nullptr, (void**)&_IndexMap);

	if (FAILED(Result))
	{
		return false;
	}

	std::copy(std::begin(Indices), std::end(Indices), _IndexMap);
	_IndexBuffer->Unmap(0, nullptr);

	//インデクスバッファビュー設定
	_IndexBufferView.BufferLocation = _IndexBuffer->GetGPUVirtualAddress();
	_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	_IndexBufferView.SizeInBytes = sizeof(Indices[0]) * Indices.size();

	return true;
}

bool Dx12Wrapper::LoadShader()
{
	HRESULT Result = S_OK;

	Result = D3DCompileFromFile(L"Shader/VertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BasicVS", "vs_5_0", D3DCOMPILE_DEBUG
		| D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_VertexShader, nullptr);
	if (FAILED(Result))
	{
		return false;
	}

	Result = D3DCompileFromFile(L"Shader/PixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BasicPS", "ps_5_0", D3DCOMPILE_DEBUG
		| D3DCOMPILE_SKIP_OPTIMIZATION, 0, &_PixelShader, nullptr);

	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateRootSignature()
{
	HRESULT Result = S_OK;

	D3D12_DESCRIPTOR_RANGE Range[2] = {};
	Range[0].BaseShaderRegister = 0;//0
	Range[0].NumDescriptors = 1;
	Range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	Range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVはテクスチャ t
	Range[0].RegisterSpace = 0;

	//座標変換
	Range[1].BaseShaderRegister = 0;//0
	Range[1].NumDescriptors = 1;
	Range[1].OffsetInDescriptorsFromTableStart = 0;
	Range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	Range[1].RegisterSpace = 0;

	D3D12_ROOT_PARAMETER Rp[2] = {};
	Rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Rp[0].DescriptorTable.NumDescriptorRanges = 1;
	Rp[0].DescriptorTable.pDescriptorRanges = &Range[0];

	//座標変換
	Rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	Rp[1].DescriptorTable.NumDescriptorRanges = 1;
	Rp[1].DescriptorTable.pDescriptorRanges = &Range[1];

	D3D12_STATIC_SAMPLER_DESC Sampler[1];
	Sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	Sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	Sampler[0].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	Sampler[0].MaxAnisotropy = 0;
	Sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
	Sampler[0].MinLOD = 0;
	Sampler[0].ShaderRegister = 0;
	Sampler[0].RegisterSpace = 0;
	Sampler[0].MipLODBias = 0.0f;

	D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
	RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootSignatureDesc.NumParameters = 2;
	RootSignatureDesc.pParameters = Rp;
	RootSignatureDesc.NumStaticSamplers = 1;
	RootSignatureDesc.pStaticSamplers = Sampler;

	Result = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &_Signature, &_Error);

	if (FAILED(Result))
	{
		return false;
	}

	Result = _Dev->CreateRootSignature(0, _Signature->GetBufferPointer(), _Signature->GetBufferSize(), IID_PPV_ARGS(&_RootSigunature));

	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreatePipeLineState()
{
	HRESULT Result = S_OK;


	D3D12_INPUT_ELEMENT_DESC InputLayoutDescs[] = { { "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
													{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 } };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc = {};
	//シェーダ系
	PipelineStateDesc.VS.pShaderBytecode = _VertexShader->GetBufferPointer();
	PipelineStateDesc.VS.BytecodeLength = _VertexShader->GetBufferSize();

	PipelineStateDesc.PS.pShaderBytecode = _PixelShader->GetBufferPointer();
	PipelineStateDesc.PS.BytecodeLength = _PixelShader->GetBufferSize();
	//ルートシグネチャ、頂点レイアウト
	PipelineStateDesc.pRootSignature = _RootSigunature.Get();
	PipelineStateDesc.InputLayout.pInputElementDescs = InputLayoutDescs;
	PipelineStateDesc.InputLayout.NumElements = _countof(InputLayoutDescs);

	//レンダーターゲット
	PipelineStateDesc.NumRenderTargets = 1;
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//震度ステンシル
	PipelineStateDesc.DepthStencilState.DepthEnable = false;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.DSVFormat;

	//ラスタライザ
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	PipelineStateDesc.RasterizerState.FrontCounterClockwise = false;
	PipelineStateDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	PipelineStateDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	PipelineStateDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	PipelineStateDesc.RasterizerState.DepthClipEnable = true;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.AntialiasedLineEnable = false;
	PipelineStateDesc.RasterizerState.ForcedSampleCount = 0;
	PipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//BlendState
	D3D12_RENDER_TARGET_BLEND_DESC RenderTargetBlendDesc{};
	RenderTargetBlendDesc.BlendEnable = true;
	RenderTargetBlendDesc.LogicOpEnable = false;
	RenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	RenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
	RenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	RenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	RenderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	RenderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	RenderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	RenderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		PipelineStateDesc.BlendState.RenderTarget[i] = RenderTargetBlendDesc;
	}
	PipelineStateDesc.BlendState.AlphaToCoverageEnable = false;
	PipelineStateDesc.BlendState.IndependentBlendEnable = false;

	//その他
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = 0xffffffff;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	Result = _Dev->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&_PipelineState));

	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Dx12Wrapper::SetViewPort()
{
	Size WindowSize = Application::Instance().GetWindowSize();

	_ViewPort.TopLeftX = 0;
	_ViewPort.TopLeftY = 0;
	_ViewPort.Width = WindowSize.Width;
	_ViewPort.Height = WindowSize.Height;
	_ViewPort.MaxDepth = 1.0f;
	_ViewPort.MinDepth = 0.0f;

	_ScissorRect.left = 0;
	_ScissorRect.top = 0;
	_ScissorRect.right = WindowSize.Width;
	_ScissorRect.bottom = WindowSize.Height;

	return true;
}


bool Dx12Wrapper::CreateTextureFromImageData_Discrete(DirectX::ScratchImage& scratchImage)
{
	HRESULT Result = S_OK;

	auto MetaData = scratchImage.GetMetadata();
	auto Img = scratchImage.GetImage(0, 0, 0);
	//中間バッファとしてUploadeヒープ設定
	D3D12_HEAP_PROPERTIES UploadeHeapProp = {};
	UploadeHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	UploadeHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	UploadeHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	UploadeHeapProp.CreationNodeMask = 0;
	UploadeHeapProp.VisibleNodeMask = 0;

	//中間バッファリソース
	D3D12_RESOURCE_DESC UpResDesc = {};
	UpResDesc.Format = DXGI_FORMAT_UNKNOWN;
	UpResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	UpResDesc.Width = AligmentedValue(Img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * Img->height;
	UpResDesc.Height = 1;
	UpResDesc.DepthOrArraySize = 1;
	UpResDesc.MipLevels = 1;
	UpResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	UpResDesc.SampleDesc.Count = 1;
	UpResDesc.SampleDesc.Quality = 0;

	ID3D12Resource* UploadBuff = nullptr;

	Result = _Dev->CreateCommittedResource(&UploadeHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&UpResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&UploadBuff));

	if (FAILED(Result))
	{
		return false;
	}


	D3D12_HEAP_PROPERTIES HeapProp = {};

	//グラボがDiscreteの場合はDEFAULTで作っておく
	//UPLOADバッファを中間バッファに
	//CopyTextureReginで転送

	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProp.CreationNodeMask = 0;
	HeapProp.VisibleNodeMask = 0;



	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(scratchImage.GetMetadata().dimension);
	ResourceDesc.Width = MetaData.width;
	ResourceDesc.Height = MetaData.height;
	ResourceDesc.DepthOrArraySize = MetaData.arraySize;
	ResourceDesc.MipLevels = MetaData.mipLevels;
	ResourceDesc.Format = MetaData.format;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	ResourceDesc.Alignment = 0;

	Result = _Dev->CreateCommittedResource(&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&_TextureBuffer));

	if (FAILED(Result))
	{
		return false;
	}

	uint8_t* MapForImg = nullptr;
	Result = UploadBuff->Map(0, nullptr, (void**)&MapForImg);

	auto Address = Img->pixels;
	for (int i = 0; i < Img->height; i++)
	{
		std::copy_n(Address, Img->rowPitch, MapForImg);
		Address += Img->rowPitch;
		MapForImg += AligmentedValue(Img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	}

	UploadBuff->Unmap(0, nullptr);

	D3D12_TEXTURE_COPY_LOCATION Src = {};
	D3D12_TEXTURE_COPY_LOCATION Dst = {};

	Src.pResource = UploadBuff;
	Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Src.PlacedFootprint.Offset = 0;
	Src.PlacedFootprint.Footprint.Width = MetaData.width;
	Src.PlacedFootprint.Footprint.Height = MetaData.height;
	Src.PlacedFootprint.Footprint.Depth = MetaData.depth;
	Src.PlacedFootprint.Footprint.RowPitch = AligmentedValue(Img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	Src.PlacedFootprint.Footprint.Format = Img->format;

	Dst.pResource = _TextureBuffer.Get();
	Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Dst.SubresourceIndex = 0;

	_CmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);

	//リソースバリアを設定
	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	BarrierDesc.Transition.pResource = _TextureBuffer.Get();
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//バリアを追加
	_CmdList->ResourceBarrier(1, &BarrierDesc);
	_CmdList->Close();
	ID3D12CommandList* Cmds[] = { _CmdList.Get() };
	_CmdQueue->ExecuteCommandLists(1, Cmds);
	++_FenceValue;

	_CmdQueue->Signal(_Fence.Get(), _FenceValue);
	WaitWithFence();

	_CmdAlloc->Reset();
	_CmdList->Reset(_CmdAlloc.Get(), _PipelineState.Get());


	D3D12_DESCRIPTOR_HEAP_DESC TexHeapDesc = {};
	TexHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TexHeapDesc.NodeMask = 0;
	TexHeapDesc.NumDescriptors = 1;
	TexHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_Dev->CreateDescriptorHeap(&TexHeapDesc, IID_PPV_ARGS(&_TextureHeap));


	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Format = MetaData.format;
	SrvDesc.Texture2D.MipLevels = 1;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	_Dev->CreateShaderResourceView(_TextureBuffer.Get(), &SrvDesc, _TextureHeap->GetCPUDescriptorHandleForHeapStart());

	UploadBuff->Release();
	return true;
}

bool Dx12Wrapper::CreateTextureFromImageData_NotDiscrete(DirectX::ScratchImage& scratchImage)
{
	HRESULT Result = S_OK;

	auto MetaData = scratchImage.GetMetadata();

	//WriteToSubresource方式
	D3D12_HEAP_PROPERTIES HeapProp = {};

	HeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	HeapProp.CreationNodeMask = 1;
	HeapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(scratchImage.GetMetadata().dimension);
	ResourceDesc.Width = MetaData.width;
	ResourceDesc.Height = MetaData.height;
	ResourceDesc.DepthOrArraySize = MetaData.arraySize;
	ResourceDesc.MipLevels = MetaData.mipLevels;
	ResourceDesc.Format = MetaData.format;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	ResourceDesc.Alignment = 0;

	Result = _Dev->CreateCommittedResource(&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&_TextureBuffer));

	if (FAILED(Result))
	{
		return false;
	}

	auto Img = scratchImage.GetImage(0, 0, 0);

	_TextureBuffer->WriteToSubresource(0, nullptr, Img->pixels, Img->rowPitch, Img->slicePitch);

	D3D12_DESCRIPTOR_HEAP_DESC TexHeapDesc = {};
	TexHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TexHeapDesc.NodeMask = 0;
	TexHeapDesc.NumDescriptors = 1;
	TexHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_Dev->CreateDescriptorHeap(&TexHeapDesc, IID_PPV_ARGS(&_TextureHeap));


	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Format = MetaData.format;
	SrvDesc.Texture2D.MipLevels = 1;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	_Dev->CreateShaderResourceView(_TextureBuffer.Get(), &SrvDesc, _TextureHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
}

bool Dx12Wrapper::CreateTexture()
{
	HRESULT Result = S_OK;

	//WriteToSubresource方式
	D3D12_HEAP_PROPERTIES HeapProp = {};
	HeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	HeapProp.CreationNodeMask = 1;
	HeapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	ResourceDesc.Width = 600;
	ResourceDesc.Height = 600;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	ResourceDesc.Alignment = 0;

	Result = _Dev->CreateCommittedResource(&HeapProp,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&_TextureBuffer));

	if (FAILED(Result))
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC TexHeapDesc = {};
	TexHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TexHeapDesc.NodeMask = 0;
	TexHeapDesc.NumDescriptors = 1;
	TexHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_Dev->CreateDescriptorHeap(&TexHeapDesc, IID_PPV_ARGS(&_TextureHeap));


	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SrvDesc.Texture2D.MipLevels = 1;
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	_Dev->CreateShaderResourceView(_TextureBuffer.Get(), &SrvDesc, _TextureHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
}

bool Dx12Wrapper::CreateTransformConstantBuffer()
{
	HRESULT Result = S_OK;

	Size WinSize = Application::Instance().GetWindowSize();

	D3D12_HEAP_PROPERTIES HeapProp = {};
	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	HeapProp.CreationNodeMask = 1;
	HeapProp.VisibleNodeMask = 1;
	HeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC ResDesc = {};
	ResDesc.Alignment = 0;
	ResDesc.Width = AligmentedValue(sizeof(XMMATRIX), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResDesc.SampleDesc.Count = 1;
	ResDesc.SampleDesc.Quality = 0;
	ResDesc.MipLevels = 1;
	ResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ResDesc.Height = 1;
	ResDesc.DepthOrArraySize = 1;


	Result = _Dev->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_TransformCB));


	if (FAILED(Result))
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC TransHeapDesc = {};
	TransHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TransHeapDesc.NodeMask = 0;
	TransHeapDesc.NumDescriptors = 2;
	TransHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	Result = _Dev->CreateDescriptorHeap(&TransHeapDesc, IID_PPV_ARGS(&_TransfromHeap));

	if (FAILED(Result))
	{
		return false;
	}


	D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc = {};
	ViewDesc.BufferLocation = _TransformCB->GetGPUVirtualAddress();
	ViewDesc.SizeInBytes = _TransformCB->GetDesc().Width;

	_Dev->CreateConstantBufferView(&ViewDesc, _TransfromHeap->GetCPUDescriptorHandleForHeapStart());


	//XMMATRIXは行列。
	//初期化はXMMatrixIdentityで初期化する

	XMMATRIX Trans2D = XMMatrixIdentity();
	Trans2D.r[0].m128_f32[0] = 1.0f / (static_cast<float>(WinSize.Width) / 2);
	Trans2D.r[1].m128_f32[1] = -1.0f / (static_cast<float>(WinSize.Height) / 2);
	Trans2D.r[3].m128_f32[0] = -1;
	Trans2D.r[3].m128_f32[1] = 1;

	XMMATRIX WorldMat = XMMatrixIdentity();

	WorldMat = XMMatrixRotationY(XM_PIDIV4);

	//視点
	XMFLOAT3 Eye(0, 0, -5);
	//注視点
	XMFLOAT3 Target(0, 0, 0);
	//上ベクトルが必要
	XMFLOAT3 UpperVec(0, 1, 0);

	//XMVECTORはSIMD対象であり、XMFLOAT等はSIMD対象外
	ViewMat = XMMatrixLookAtLH(XMLoadFloat3(&Eye), XMLoadFloat3(&Target), XMLoadFloat3(&UpperVec));

	ProjMat = XMMatrixPerspectiveFovLH(XM_PIDIV4, static_cast<float>(WinSize.Width) / static_cast<float>(WinSize.Height), 1.0f, 1000);

	_TransformCB->Map(0, nullptr, (void**)&MappedMatrix);

	*MappedMatrix = WorldMat * ViewMat * ProjMat;

	return true;
}

unsigned int Dx12Wrapper::AligmentedValue(unsigned int Size, unsigned int AlignmentSize)
{
	return (Size + AlignmentSize - (Size % AlignmentSize));
}

bool Dx12Wrapper::LoadPictureFromFile()
{
	HRESULT Result = S_OK;

	TexMetadata MetaData = {};
	ScratchImage ScratchImg = {};

	Result = CoInitializeEx(0, COINIT_MULTITHREADED);

	if (FAILED(Result))
	{
		return false;
	}

	Result = LoadFromWICFile(L"Texture/Texture.jpg", WIC_FLAGS_NONE, &MetaData, ScratchImg);

	if (FAILED(Result))
	{
		return false;
	}


	if (!CreateTextureFromImageData_Discrete(ScratchImg))
	{
		return false;
	}

	return true;
}