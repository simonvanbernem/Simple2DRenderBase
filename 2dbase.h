#include <stdint.h>
#include "Windows.h"
#include "d3d11.h"
#include <d3dcompiler.h>
#include "DirectXMath.h"
#include "assert.h"
#include <stdio.h>

static bool destroy_window = false;

struct Constant_Vertex_Buffer{
	float projection_matrix[4][4];
};


struct Vertex{
	float x;
	float y;
};

static HWND window_handle;
static ID3D11Device* device;
static ID3D11DeviceContext* device_context;
static IDXGISwapChain* swap_chain;
static ID3D11RenderTargetView* render_target_view;
static ID3D11Buffer* constant_vertex_buffer = nullptr;
static ID3D11InputLayout* input_layout;
static int window_width;
static int window_height;
static Vertex camera_position;
static float camera_view_units;
static float camera_speeed; //dont come and murder me please
static float zoom_speeed;

using Index = uint16_t;

static ID3D11Buffer* vertex_buffer = nullptr;
static int vertex_buffer_size = 0;

static ID3D11Buffer* index_buffer = nullptr;
static int index_buffer_size = 0;

//I know I am a bad person: I just cant be bothered to do this properly right now. We only need info about 6 keys, but I just use their ascii code to index this array. I am a horrible person
static bool is_key_down[255] = {};

LRESULT CALLBACK WindowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam){
	LRESULT result = 0;
	switch(message){
		case WM_DESTROY:{
			destroy_window = true;
			PostQuitMessage(0);
			return 0;
		} break;
		case WM_KEYDOWN:{
			if(wParam == 'W' || wParam == 'A' || wParam == 'S' || wParam == 'D' || wParam == 'Q' || wParam == 'E')
				is_key_down[wParam] = true;
		} break;
		case WM_KEYUP:{
			if(wParam == 'W' || wParam == 'A' || wParam == 'S' || wParam == 'D' || wParam == 'Q' || wParam == 'E')
				is_key_down[wParam] = false;
		} break;
		default:{
			result = DefWindowProc(handle, message, wParam, lParam);
		} break;
	}

	return result;
}

char* get_d3d11_error_names(HRESULT result) {
	switch (result) {
		case D3D11_ERROR_FILE_NOT_FOUND:
			return "D3D11_ERROR_FILE_NOT_FOUND";
		break;
		case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
			return "D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS";
		break;
		case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
			return "D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS";
		break;
		case D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD:
			return "D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD";
		break;
		case E_FAIL :
			return "E_FAIL ";
		break;
		case E_INVALIDARG :
			return "E_INVALIDARG ";
		break;
		case E_OUTOFMEMORY :
			return "E_OUTOFMEMORY ";
		break;
		case E_NOTIMPL :
			return "E_NOTIMPL ";
		break;
		case S_FALSE :
			return "S_FALSE ";
		break;
		case S_OK :
			return "S_OK ";
		break;
		default:
			return "how the hell did you get an unknown error code?";
	}
}

#define HANDLE_HRESULT(command)	{auto handle_hresult_result = command; if(handle_hresult_result!=S_OK){printf("HANDLE_HRESULT failed (line %d)! DirectX11 Error: %s", __LINE__, get_d3d11_error_names(handle_hresult_result)); assert(false);}}


ID3DBlob* compile_shader(const char* source, const char* target, const char* entry_point, const char* debug_name){
	ID3DBlob* compile_errors = nullptr;
	ID3DBlob* compiled_shader = nullptr;
	HRESULT error = D3DCompile(source, strlen(source), NULL, NULL, NULL, entry_point, target, 0, 0, &compiled_shader, &compile_errors);

	if(compile_errors){
		printf("%s failed to compile", debug_name);
		printf("errors:\n%s", static_cast<char*>(compile_errors->GetBufferPointer()));
		assert(false);
	}

	return compiled_shader;
}

static uint64_t last_frame_timestamp;
static uint64_t clock_frequency;
void create_window(int width, int height, char* title, Vertex initial_camera_position = {0, 0}, float initial_camera_view_units = 100, float camera_speed = 1.f, float zoom_speed = 1.f){


	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	last_frame_timestamp = temp.QuadPart;
	QueryPerformanceFrequency(&temp);
	clock_frequency = temp.QuadPart;

	HMODULE application_instance = GetModuleHandle(NULL);

	camera_speeed = camera_speed; //I already told you: I know I am a bad person.
	zoom_speeed = zoom_speed;

	camera_view_units = initial_camera_view_units;
	camera_position = initial_camera_position;

	window_width = width;
	window_height = height;

	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEXW);
	window_class.style = CS_HREDRAW|CS_VREDRAW;
	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = application_instance;
	window_class.lpszClassName = "2DBASE_WINDOW_CLASS";

	if(!RegisterClassExA(&window_class))
		assert(false);

	window_handle = CreateWindowExA(0, window_class.lpszClassName, title, WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, application_instance, 0);


	DXGI_SWAP_CHAIN_DESC swap_chain_description = {};
	
	swap_chain_description.BufferDesc.Width = width;
	swap_chain_description.BufferDesc.Height = height;
	swap_chain_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_description.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_description.BufferDesc.RefreshRate.Denominator = 1;

	swap_chain_description.BufferCount = 1;
	swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//no multisampling
	swap_chain_description.SampleDesc.Count = 1;
	swap_chain_description.SampleDesc.Quality = 0;

	swap_chain_description.OutputWindow = window_handle;

	swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	swap_chain_description.Windowed = true;

	D3D_FEATURE_LEVEL desired_feature_level = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL supported_feature_level;

	HRESULT error = D3D11CreateDeviceAndSwapChain(
		0, //pAdapter
		D3D_DRIVER_TYPE_HARDWARE, //DriverType
		0, //Software
		0, //Flags
		&desired_feature_level, //pFeatureLevels
		1, //FeatureLevels
		D3D11_SDK_VERSION, //SDKVersion
		&swap_chain_description, //pSwapChainDesc
		&swap_chain, //ppSwapChain
		&device, //ppDevice
		&supported_feature_level, //FeatureLevel
		&device_context //ppImmediateContext
	);

	assert(error == S_OK);


	ID3D11Texture2D* back_buffer = nullptr;
	HANDLE_HRESULT(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer)));
	HANDLE_HRESULT(device->CreateRenderTargetView(back_buffer, 0, &render_target_view));
	back_buffer->Release();

	device_context->OMSetRenderTargets(1, &render_target_view, nullptr);


	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);

	device_context->RSSetViewports(1, &viewport);


	const char* vertex_shader_source = R"DELIMITER(

	cbuffer vertexBuffer : register(b0) {
		float4x4 projection_matrix;
	};

	struct VS_INPUT{
		float2 position : POSITION;
	};

	struct VS_OUTPUT{
		float4 position : SV_POSITION;
	};

	VS_OUTPUT VS(VS_INPUT input) {
		VS_OUTPUT output;
		
		output.position = mul(projection_matrix, float4(input.position.xy, 0.f, 1.f));

		return output;
	}

	)DELIMITER";

	ID3D11VertexShader* vertex_shader;
	auto vertex_shader_bytecode = compile_shader(vertex_shader_source, "vs_4_0", "VS", "color alpha texture vertex shader");
	HANDLE_HRESULT(device->CreateVertexShader(vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), NULL, &vertex_shader));

	//input layout
	D3D11_INPUT_ELEMENT_DESC vertex_layout[] = {
		//per vertex
		{"POSITION", 0, DXGI_FORMAT_R32G32_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	HANDLE_HRESULT(device->CreateInputLayout(vertex_layout, 1, vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), &input_layout));


	//pixel shader
	//@TODO so it turns out my clipping down there doesn't work. I should fix this
	const char* pixel_shader_source = R"DELIMITER(

	struct VS_OUTPUT{
		float4 position : SV_POSITION;
	};

	float4 PS(VS_OUTPUT input) : SV_TARGET {
		float4 color;
		color.r = 0.2;
		color.g = 0.4;
		color.b = 0.8;
		color.a = 1;
		return color;
	}

	)DELIMITER";

	ID3D11PixelShader* pixel_shader;
	auto pixel_shader_bytecode = compile_shader(pixel_shader_source, "ps_4_0", "PS", "unified pixel shader");
	HANDLE_HRESULT(device->CreatePixelShader(pixel_shader_bytecode->GetBufferPointer(), pixel_shader_bytecode->GetBufferSize(), NULL, &pixel_shader));


	D3D11_BUFFER_DESC description;
	description.ByteWidth = sizeof(Constant_Vertex_Buffer);
	description.Usage = D3D11_USAGE_DYNAMIC;
	description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	description.MiscFlags = 0;
	HANDLE_HRESULT(device->CreateBuffer(&description, NULL, &constant_vertex_buffer));

	device_context->VSSetShader(vertex_shader, 0, 0);
	device_context->PSSetShader(pixel_shader, 0, 0);
	device_context->IASetInputLayout(input_layout);
	device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool render_and_handle_input(int vertex_count, Vertex* vertices, int index_count, Index* indices){
	{
		LARGE_INTEGER temp;
		QueryPerformanceCounter(&temp);
		uint64_t current_frame_timestamp = temp.QuadPart;

		float frame_time = float(current_frame_timestamp - last_frame_timestamp) / float(clock_frequency);

		camera_position.x += frame_time * camera_speeed * camera_view_units * 2 * (is_key_down['D'] - is_key_down['A']);
		camera_position.y += frame_time * camera_speeed * camera_view_units * 2 * (is_key_down['W'] - is_key_down['S']);
		camera_view_units += frame_time * camera_view_units * zoom_speeed * 2 * (is_key_down['Q'] - is_key_down['E']);

		last_frame_timestamp = current_frame_timestamp;

		D3D11_MAPPED_SUBRESOURCE resource;
		HANDLE_HRESULT(device_context->Map(constant_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
		auto buffer = (Constant_Vertex_Buffer*) resource.pData;

		float aspect_ratio = float(window_width) / float(window_height);

		float l = camera_position.x - camera_view_units * aspect_ratio;
		float r = camera_position.x + camera_view_units * aspect_ratio;
		float b = camera_position.y - camera_view_units;
		float t = camera_position.y + camera_view_units;

		//diagonal: we scale x by 2 over width to get a 0-2 range, same for y with height
		//bottom row: we translate x by -1 to get -1 to 1 range, same for y
		float projection_matrix[4][4] = {
			{2.f / (r - l)       ,  0.f                , 0.f, 0.f},
			{0.f                 , 2.f / (t - b)       , 0.f, 0.f},
			{0.f                 ,  0.f                , 1.f, 0.f},
			{-((r + l) / (r - l)), -((t + b) / (t - b)), 0.f, 1.f},
		};
		memcpy(buffer, projection_matrix, sizeof(projection_matrix));
		device_context->Unmap(constant_vertex_buffer, 0);
		device_context->VSSetConstantBuffers(0, 1, &constant_vertex_buffer);
	}

	//enlarge or create vertex buffer
	if(!vertex_buffer || vertex_buffer_size < vertex_count){
		if(vertex_buffer)
			vertex_buffer->Release();

		vertex_buffer_size = vertex_count + 5000;

		D3D11_BUFFER_DESC description = {};
		description.ByteWidth = sizeof(Vertex) * vertex_count;
		description.Usage = D3D11_USAGE_DYNAMIC;
		description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HANDLE_HRESULT(device->CreateBuffer(&description, NULL, &vertex_buffer));
	}

		//copy vertices into the vertex buffer
	{
		D3D11_MAPPED_SUBRESOURCE resource;
		HANDLE_HRESULT(device_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
		memcpy(resource.pData, vertices, vertex_count * sizeof(Vertex));
		device_context->Unmap(vertex_buffer, 0);
	}

	{
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;
		device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
	}

	//enlarge or create index buffer
	if(!index_buffer || index_buffer_size < index_count){
		if(index_buffer)
			index_buffer->Release();

		index_buffer_size = index_count + 5000;

		D3D11_BUFFER_DESC description = {};
		description.ByteWidth = sizeof(Index) * index_buffer_size;
		description.Usage = D3D11_USAGE_DYNAMIC;
		description.BindFlags = D3D11_BIND_INDEX_BUFFER;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HANDLE_HRESULT(device->CreateBuffer(&description, 0, &index_buffer));
	}

	//copy indices into the index buffer
	{
		D3D11_MAPPED_SUBRESOURCE resource;
		HANDLE_HRESULT(device_context->Map(index_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
		memcpy(resource.pData, indices, index_count * sizeof(Index));
		device_context->Unmap(index_buffer, 0);
	}

	{
		device_context->IASetIndexBuffer(index_buffer, sizeof(Index) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	}

	device_context->DrawIndexed(index_count, 0, 0);

	HANDLE_HRESULT(swap_chain->Present(0, 0));

	float color[4] = {0.2f, 0.2f, 0.2f, 1.f};
	device_context->ClearRenderTargetView(render_target_view, color);


	MSG msg = {};

	BOOL message_exisits = PeekMessageA(&msg, 0, 0, 0, PM_REMOVE);

	while(message_exisits) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
		message_exisits = PeekMessageA(&msg, 0, 0, 0, PM_REMOVE);
	}

	return destroy_window;
}