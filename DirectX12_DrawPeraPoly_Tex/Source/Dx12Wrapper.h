#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <map>
#include <memory>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#pragma comment(lib,"DirectXTex.lib")

//DirectX12の初期化等の各要素等をまとめるためのクラス
class Dx12Wrapper
{
public:
	Dx12Wrapper(HWND windowH);
	~Dx12Wrapper();

	bool Init();

	//画面のリセット
	bool ScreenCrear();
	//1stPath描画
	void Draw();
	//画面の更新
	void ScreenFlip();

private:

	struct  Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 uv;
	};


	//ウィンドウハンドル
	HWND _WindowH;

	//デバイス-----------
	Microsoft::WRL::ComPtr<ID3D12Device> _Dev;


	//コマンドリスト作成
	bool CreateCommandList();
	//コマンドリスト確保用オブジェクト
	Microsoft::WRL::ComPtr <ID3D12CommandAllocator> _CmdAlloc;
	//コマンドリスト本体
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> _CmdList;
	//コマンドリストのキュー
	Microsoft::WRL::ComPtr <ID3D12CommandQueue> _CmdQueue;

	//DXGI-----------
	Microsoft::WRL::ComPtr<IDXGIFactory6> _DxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _DxgiSwapChain;


	//コマンド実行
	void ExecuteCommand();

	//フェンスオブジェクト-----------
	Microsoft::WRL::ComPtr <ID3D12Fence> _Fence;
	//フェンス値
	UINT64 _FenceValue;
	//ポーリング待機
	void WaitWithFence();
	//バリア追加
	void AddBarrier(Microsoft::WRL::ComPtr<ID3D12Resource>& Buffer, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);
	//バリア追加
	void AddBarrier(D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);

	//レンダーターゲットView作成
	bool CreateRenderTargetView();
	//レンダーターゲット用デスクリプタヒープ
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _Rtv_DescriptorHeap;
	//スワップチェインのリソースポインタ群
	std::vector< Microsoft::WRL::ComPtr<ID3D12Resource>> _BackBuffers;


	//頂点バッファ作成
	bool CreateVertexBuffer();
	//頂点バッファ
	Microsoft::WRL::ComPtr <ID3D12Resource> _VertexBuffer;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW _VertexBufferView;
	Vertex Vertices[4];

	//インデクスバッファ作成
	bool CreateIndexBuffer();
	//インデックスバッファ
	Microsoft::WRL::ComPtr <ID3D12Resource> _IndexBuffer;
	//インデックスバッファビュー
	D3D12_INDEX_BUFFER_VIEW _IndexBufferView;
	//頂点番号
	std::vector<unsigned short> Indices;


	//画像読み込み
	bool LoadPictureFromFile();
	//テクスチャオブジェクト作成
	bool CreateTextureFromImageData_Discrete(DirectX::ScratchImage& scratchImage);
	bool CreateTextureFromImageData_NotDiscrete(DirectX::ScratchImage& scratchImage);
	bool CreateTexture();
	//テクスチャバッファ
	Microsoft::WRL::ComPtr <ID3D12Resource> _TextureBuffer;
	//テクスチャヒープ
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _TextureHeap;

	//座標変換用定数バッファ及び、バッファビューを作成
	bool CreateTransformConstantBuffer();
	unsigned int AligmentedValue(unsigned int Size, unsigned int AlignmentSize);
	//座標変換定数バッファ
	Microsoft::WRL::ComPtr <ID3D12Resource> _TransformCB;
	//座標変換ヒープ
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _TransfromHeap;

	//ビューポートを作成
	bool SetViewPort();
	//ビューポート
	D3D12_VIEWPORT _ViewPort;
	//シザー
	D3D12_RECT _ScissorRect;
	DirectX::XMMATRIX ViewMat;
	DirectX::XMMATRIX ProjMat;
	DirectX::XMMATRIX* MappedMatrix;


	//シェーダ読み込み
	bool LoadShader();
	//頂点シェーダ
	Microsoft::WRL::ComPtr <ID3DBlob> _VertexShader;
	//ピクセルシェーダ
	Microsoft::WRL::ComPtr <ID3DBlob> _PixelShader;

	//ルートシグネチャ作成
	bool CreateRootSignature();
	//ルートシグネチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> _RootSigunature;
	//シグネチャ
	Microsoft::WRL::ComPtr <ID3DBlob> _Signature;
	//エラー対処用
	Microsoft::WRL::ComPtr <ID3DBlob> _Error;

	//パイプラインステートを作成
	bool CreatePipeLineState();
	//パイプラインステート
	Microsoft::WRL::ComPtr <ID3D12PipelineState> _PipelineState;

};
