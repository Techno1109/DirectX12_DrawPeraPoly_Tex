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

//DirectX12�̏��������̊e�v�f�����܂Ƃ߂邽�߂̃N���X
class Dx12Wrapper
{
public:
	Dx12Wrapper(HWND windowH);
	~Dx12Wrapper();

	bool Init();

	//��ʂ̃��Z�b�g
	bool ScreenCrear();
	//1stPath�`��
	void Draw();
	//��ʂ̍X�V
	void ScreenFlip();

private:

	struct  Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 uv;
	};


	//�E�B���h�E�n���h��
	HWND _WindowH;

	//�f�o�C�X-----------
	Microsoft::WRL::ComPtr<ID3D12Device> _Dev;


	//�R�}���h���X�g�쐬
	bool CreateCommandList();
	//�R�}���h���X�g�m�ۗp�I�u�W�F�N�g
	Microsoft::WRL::ComPtr <ID3D12CommandAllocator> _CmdAlloc;
	//�R�}���h���X�g�{��
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> _CmdList;
	//�R�}���h���X�g�̃L���[
	Microsoft::WRL::ComPtr <ID3D12CommandQueue> _CmdQueue;

	//DXGI-----------
	Microsoft::WRL::ComPtr<IDXGIFactory6> _DxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _DxgiSwapChain;


	//�R�}���h���s
	void ExecuteCommand();

	//�t�F���X�I�u�W�F�N�g-----------
	Microsoft::WRL::ComPtr <ID3D12Fence> _Fence;
	//�t�F���X�l
	UINT64 _FenceValue;
	//�|�[�����O�ҋ@
	void WaitWithFence();
	//�o���A�ǉ�
	void AddBarrier(Microsoft::WRL::ComPtr<ID3D12Resource>& Buffer, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);
	//�o���A�ǉ�
	void AddBarrier(D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter);

	//�����_�[�^�[�Q�b�gView�쐬
	bool CreateRenderTargetView();
	//�����_�[�^�[�Q�b�g�p�f�X�N���v�^�q�[�v
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _Rtv_DescriptorHeap;
	//�X���b�v�`�F�C���̃��\�[�X�|�C���^�Q
	std::vector< Microsoft::WRL::ComPtr<ID3D12Resource>> _BackBuffers;


	//���_�o�b�t�@�쐬
	bool CreateVertexBuffer();
	//���_�o�b�t�@
	Microsoft::WRL::ComPtr <ID3D12Resource> _VertexBuffer;
	//���_�o�b�t�@�r���[
	D3D12_VERTEX_BUFFER_VIEW _VertexBufferView;
	Vertex Vertices[4];

	//�C���f�N�X�o�b�t�@�쐬
	bool CreateIndexBuffer();
	//�C���f�b�N�X�o�b�t�@
	Microsoft::WRL::ComPtr <ID3D12Resource> _IndexBuffer;
	//�C���f�b�N�X�o�b�t�@�r���[
	D3D12_INDEX_BUFFER_VIEW _IndexBufferView;
	//���_�ԍ�
	std::vector<unsigned short> Indices;


	//�摜�ǂݍ���
	bool LoadPictureFromFile();
	//�e�N�X�`���I�u�W�F�N�g�쐬
	bool CreateTextureFromImageData_Discrete(DirectX::ScratchImage& scratchImage);
	bool CreateTextureFromImageData_NotDiscrete(DirectX::ScratchImage& scratchImage);
	bool CreateTexture();
	//�e�N�X�`���o�b�t�@
	Microsoft::WRL::ComPtr <ID3D12Resource> _TextureBuffer;
	//�e�N�X�`���q�[�v
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _TextureHeap;

	//���W�ϊ��p�萔�o�b�t�@�y�сA�o�b�t�@�r���[���쐬
	bool CreateTransformConstantBuffer();
	unsigned int AligmentedValue(unsigned int Size, unsigned int AlignmentSize);
	//���W�ϊ��萔�o�b�t�@
	Microsoft::WRL::ComPtr <ID3D12Resource> _TransformCB;
	//���W�ϊ��q�[�v
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> _TransfromHeap;

	//�r���[�|�[�g���쐬
	bool SetViewPort();
	//�r���[�|�[�g
	D3D12_VIEWPORT _ViewPort;
	//�V�U�[
	D3D12_RECT _ScissorRect;
	DirectX::XMMATRIX ViewMat;
	DirectX::XMMATRIX ProjMat;
	DirectX::XMMATRIX* MappedMatrix;


	//�V�F�[�_�ǂݍ���
	bool LoadShader();
	//���_�V�F�[�_
	Microsoft::WRL::ComPtr <ID3DBlob> _VertexShader;
	//�s�N�Z���V�F�[�_
	Microsoft::WRL::ComPtr <ID3DBlob> _PixelShader;

	//���[�g�V�O�l�`���쐬
	bool CreateRootSignature();
	//���[�g�V�O�l�`��
	Microsoft::WRL::ComPtr <ID3D12RootSignature> _RootSigunature;
	//�V�O�l�`��
	Microsoft::WRL::ComPtr <ID3DBlob> _Signature;
	//�G���[�Ώ��p
	Microsoft::WRL::ComPtr <ID3DBlob> _Error;

	//�p�C�v���C���X�e�[�g���쐬
	bool CreatePipeLineState();
	//�p�C�v���C���X�e�[�g
	Microsoft::WRL::ComPtr <ID3D12PipelineState> _PipelineState;

};
