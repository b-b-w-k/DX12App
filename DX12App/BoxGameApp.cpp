#include "PCH.h"
#include "BoxGameApp.h"		    
#include "InputManage.h"       // 输入管理
#include "ICameraController.h" // 摄像机控制器
#include "GeoGenerator.h"      // 模型导入
#include "D2DTextRenderer.h"   // 文字绘制
#include "Scene.h"             // 场景 
#include "DebugLineRenderer.h"
#include "DDSTextureLoader.h"
#include "ReflectionRTT.h"    
#include "Node.h"
#include "Component.h"
#include "RenderComponent.h"
#include "CollisionComponent.h"
#include "SkeletonComponent.h"
#include "ControlComponent.h"
#include "AnimationClip.h"
#include "Skeleton.h"
#include "RayCaster.h"
#include "ShadowMap.h"



using namespace DirectX;

const int numFrameResources = 3;

BoxGameApp::BoxGameApp(HINSTANCE hInstance)
	:GameApp(hInstance)
{
}

BoxGameApp::~BoxGameApp()
{
}

bool BoxGameApp::InitApp()
{
	if (!GameApp::InitApp())return false;

	ThrowIfFailed(m_commandList->Reset(
		m_commandAllocator.Get(),
		nullptr));

	// 获取SRV描述符的大小
	m_cbvSrvDescSize= m_d3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 调试线渲染器初始化
	m_debugRenderer = std::make_unique<DebugLineRenderer>(m_d3dDevice.Get());

	// ===== 输入管理初始化 =====
	m_inputManage = std::make_unique<InputManage>();
	m_inputManage->InputInit();

	


	// ===== 摄像机初始化 =====
	m_camControl = std::make_unique<FlyCameraController>();//默认自由摄像机
	m_camera.UpdateViewMatrix();//显示更新一次

	
	BuildGlobalDescriptorHeaps();//构建堆
	// ===== 场景对象初始化 =====
	m_scene = std::make_unique<Scene>(
		m_d3dDevice.Get(),
		m_commandList.Get(),
		m_cbvSrvDescSize);
	// ===== 射线检测器初始化 =====
	m_rayCaster = std::make_unique<RayCaster>(m_scene.get());

	// 构建：几何体/纹理/材质/对象/SRV堆
	BuildScene();         

	

	BuildReflection();           //建立反射资源
	BuildShadowMap();
	// ===== 管线资源初始化 =====
	BuildRootSignature();//获取纹理数量,构建描述符表
	BuildShadersAndInputLayout();
	BuildPSO();

	// ===== 帧资源 =====
	BuildFrameResource();

	//关闭命令列表
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待命令队列执行完毕
	FinishCmdQue();

	// ===== 文字渲染器初始化 =====
	m_textRenderer = std::make_unique<D2DTextRenderer>();
	if (!m_textRenderer->InitD2DRender(
		m_d3dDevice.Get(),
		m_commandQueue.Get(),
		m_swapChain.Get(),
		m_SCBCount))
		return false;

	return true;
}

LRESULT BoxGameApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int mouseX = 0;
	int mouseY = 0;
	switch (msg)
	{
	case WM_ACTIVATE: // 检测窗口是否激活, 如果没有激活就暂停游戏
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			m_isAppPause = true;
			m_timer.Pause();
		}
		else
		{
			m_isAppPause = false;
			m_timer.Start();
		}
		return 0;
	case WM_SIZE://大小调整
		m_width  = LOWORD(lParam);//更新宽高
		m_height = HIWORD(lParam);
		if (m_d3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				//暂停
				m_isAppPause = true;
				m_isMax = false;
				m_isMin = true;
				m_timer.Pause();
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				m_isAppPause = false;
				m_isMax = true;
				m_isMin = false;
				//重新设置视口
				OnResize();

			}
			else if (wParam == SIZE_RESTORED)
			{
				if (m_isMin)
				{
					m_isMin = false;
					m_isAppPause = false;
					//重新设置视口
					OnResize();
				}
				else if (m_isMax)
				{
					m_isMax = false;
					m_isAppPause = false;
					//重新设置视口
					OnResize();
				}
				else if (m_isResizing)
				{
					/*
					在WM_EXITSIZEMOVE处调整
					*/
				}
				else
				{
					//重新设置视口
					OnResize();
				}
			}

		}
		return 0;
	case WM_ENTERSIZEMOVE://拖动或拉伸边框开始
		m_isAppPause = true;
		m_isResizing = true;
		m_timer.Pause();
		return 0;
	case WM_EXITSIZEMOVE://拖动或拉伸边框结束
		m_isAppPause = false;
		m_isResizing = false;
		m_timer.Start();
		OnResize();
		return 0;
	case WM_GETMINMAXINFO://设置最小/最大窗口
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_CLOSE://关闭窗口
		
		DestroyWindow(m_hWnd);
		return 0;
	case WM_MOUSEMOVE://鼠标位置
		if (!m_isCaptureMouse)
		{
			mouseX = GET_X_LPARAM(lParam);
			mouseY = GET_Y_LPARAM(lParam);
			m_inputManage->SetMousePos(POINT{ mouseX,mouseY });
		}
		return 0;
	case WM_LBUTTONDOWN://左键按下(鼠标需传入虚拟键码)
		m_inputManage->KeyDown(VK_LBUTTON);
		
		return 0;
	case WM_RBUTTONDOWN://右键按下
		m_inputManage->KeyDown(VK_RBUTTON);
		
		return 0;
	case WM_LBUTTONUP://左键抬起
		m_inputManage->KeyUp(VK_LBUTTON);
		
		return 0;
	case WM_RBUTTONUP://右键抬起
		m_inputManage->KeyUp(VK_RBUTTON);
		
		return 0;
	case WM_KEYDOWN://键盘按下
		m_inputManage->KeyDown(wParam);
		if (wParam== VK_ESCAPE)
		{
			DestroyWindow(m_hWnd);
		}
		return 0;
	case WM_KEYUP://键盘抬起
		m_inputManage->KeyUp(wParam);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void BoxGameApp::OnResize()
{
	FinishCmdQue();//等待执行完成
	//释放文字渲染资源
	if (m_textRenderer)m_textRenderer->ReleaseResource();

	GameApp::OnResize();

	//重建文字渲染资源
	if (m_textRenderer)m_textRenderer->ReBuildResource(
		m_swapChain.Get(),
		m_SCBCount);
	m_camera.SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);

}

void BoxGameApp::DrawReflections()
{
	auto passCB = m_curFrameResource->passCBuffer->Resource();

	//保存剔除状态
	std::vector<char> saveCull;
	saveCull.reserve(m_scene->GetRenderNum());

	for (auto* obj : m_scene->GetRenderObjects())
	{
		auto* renderComp = obj->GetComponent<RenderComponent>();
		if (!renderComp)continue;

		saveCull.push_back(renderComp->isCull);
		renderComp->isCull = false;
	}
	// 每10帧绘制一次
	static int frameCounter = 0;
	bool isUpdate = false;
	if (++frameCounter % 10 == 0)isUpdate = true;
	//平面反射
	if (m_waterRT)
	{

		//先将纹理资源转为渲染目标绘制
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_waterRT->GetResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &barrier);

		//准备渲染
		auto rtv = m_waterRT->GetHCpuRtv(0);
		auto dsv = m_waterRT->GetHCpuDsv();
		m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		m_commandList->ClearRenderTargetView(
			rtv, DirectX::Colors::LightSteelBlue, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsv,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0, 0, nullptr);

		auto view = m_waterRT->GetViewport();
		auto screen = m_waterRT->GetScreenRect();
		m_commandList->RSSetViewports(1, &view);
		m_commandList->RSSetScissorRects(1, &screen);

		//设置对应常量缓冲区
		const UINT passCBSize = D3DTools::BufferByteSize(sizeof(PassConstants));
		m_commandList->SetGraphicsRootConstantBufferView(
			1, passCB->GetGPUVirtualAddress() + passCBSize * 1);//在主过程之后

		//绘制需要被反射的对象
		m_scene->Draw(m_PSOs["ReflectOpaque"].Get(),
			RenderLayer::Opaque, m_curFrameResource);
		m_scene->Draw(m_PSOs["SkyBox"].Get(),
			RenderLayer::SkyBox, m_curFrameResource);

		//切换回着色器资源视图
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_waterRT->GetResource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &barrier);

	}

	//立方体反射
	if (m_envRT && isUpdate)
	{
		//先将纹理资源转为渲染目标绘制
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_envRT->GetResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &barrier);
		//渲染6个视角
		for (UINT f = 0; f < m_envRT->GetFaceCount(); f++)
		{
			//设置渲染目标和清除状态
			auto rtv = m_envRT->GetHCpuRtv(f);
			auto dsv = m_envRT->GetHCpuDsv();
			m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
			m_commandList->ClearRenderTargetView(
				rtv, DirectX::Colors::LightSteelBlue, 0, nullptr);
			m_commandList->ClearDepthStencilView(dsv,
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
				1.0f, 0, 0, nullptr);

			//视口和裁切矩形
			auto view = m_envRT->GetViewport();
			auto screen = m_envRT->GetScreenRect();
			m_commandList->RSSetViewports(1, &view);
			m_commandList->RSSetScissorRects(1, &screen);

			//设置对应常量缓冲区
			const UINT passCBSize = D3DTools::BufferByteSize(sizeof(PassConstants));
			m_commandList->SetGraphicsRootConstantBufferView(
				1, passCB->GetGPUVirtualAddress() + passCBSize * (f + 2));//在主过程和平面反射之后

			//渲染需要被反射的对象
			m_scene->Draw(m_PSOs["ReflectOpaque"].Get(),
				RenderLayer::Opaque, m_curFrameResource);
			m_scene->Draw(m_PSOs["SkyBox"].Get(),
				RenderLayer::SkyBox, m_curFrameResource);
			m_scene->Draw(m_PSOs["Terrain"].Get(),
				RenderLayer::Terrain, m_curFrameResource);
			m_scene->Draw(m_PSOs["Wave"].Get(),
				RenderLayer::Wave, m_curFrameResource);
		}
		//切换回着色器资源视图
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_envRT->GetResource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &barrier);

	}

	//恢复剔除
	int i = 0;
	for (auto* obj : m_scene->GetRenderObjects())
	{
		auto* renderComp = obj->GetComponent<RenderComponent>();
		if (!renderComp)continue;
		renderComp->isCull = saveCull[i++];
	}
	
	//恢复主渲染目标,view,PassCB
	auto bbv = GetCurrentBackBufferView();
	auto dsv = GetDepthStencilView();
	m_commandList->OMSetRenderTargets(1, &bbv, FALSE, &dsv);
	m_commandList->RSSetViewports(1, &m_screenViewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->SetGraphicsRootConstantBufferView(
		1, passCB->GetGPUVirtualAddress());

}

void BoxGameApp::DrawShadowMap()
{
	auto passCB = m_curFrameResource->passCBuffer->Resource();
	const UINT passCBSize = D3DTools::BufferByteSize(sizeof(PassConstants));

	//保存剔除状态
	std::vector<char> saveCull;
	saveCull.reserve(m_scene->GetRenderNum());
	for (auto* obj : m_scene->GetRenderObjects())
	{
		auto* renderComp = obj->GetComponent<RenderComponent>();
		if (!renderComp)continue;
		saveCull.push_back(renderComp->isCull);
		renderComp->isCull = false;
	}

	//状态转换: 采样->深度写入
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_shadowMap->GetResource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_commandList->ResourceBarrier(1, &barrier);

	//无需绑定渲染目标,只写入深度
	auto dsv = m_shadowMap->GetHCpuDsv();
	m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//视口和裁切矩形
	auto view = m_shadowMap->GetViewport();
	auto screen = m_shadowMap->GetScreenRect();
	m_commandList->RSSetViewports(1, &view);
	m_commandList->RSSetScissorRects(1, &screen);

	m_commandList->SetGraphicsRootConstantBufferView(
		1, passCB->GetGPUVirtualAddress() + passCBSize * 8);

	//有阴影的物体
	m_scene->Draw(m_PSOs["ShadowOpaque"].Get(),  RenderLayer::Opaque,  m_curFrameResource);
	m_scene->Draw(m_PSOs["ShadowSkinned"].Get(), RenderLayer::Skinned, m_curFrameResource);
	m_scene->Draw(m_PSOs["ShadowTerrain"].Get(), RenderLayer::Terrain, m_curFrameResource);

	//状态转换: 深度写入->采样
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_shadowMap->GetResource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_commandList->ResourceBarrier(1, &barrier);

	//恢复剔除
	int i = 0;
	for (auto* obj : m_scene->GetRenderObjects())
	{
		auto* renderComp = obj->GetComponent<RenderComponent>();
		if (!renderComp)continue;
		renderComp->isCull = saveCull[i++];
	}
	//恢复渲染设置
	m_commandList->RSSetViewports(1, &m_screenViewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

}

void BoxGameApp::DrawD2DText()
{
	if (!m_textRenderer || !m_textRenderer->IsInit())return;


	//调试信息
	static const std::wstring text =
		L"1  第一人称摄像机\n2  第三人称摄像机\n3  自由摄像机\n"
		L"F1 锁定鼠标\nF2 锁定摄像机\nF3 摄像机可视化\n"
		L"F4 AABB可视化\nF5 OBB可视化\nF6 渲染统计信息\n";

	// 渲染信息统计
	wchar_t buf[256];
	if (m_isShow)
	{
		const auto& stats = m_scene->GetDrawStats();
		swprintf_s(buf,
			L"物体: %u / %u    剔除: %u\n"
			L"绘制子网格: %u    DrawCall: %u\n"
			L"子网格: %u",
			stats.visibleObjects,
			stats.totalObjects,
			stats.culledObjects,
			stats.drawnSubmeshs,
			stats.drawCalls,
			m_scene->GetSubmeshNum());
	}

	//绘制开始
	m_textRenderer->Begin(m_currBackBuffer);

	//绘制调试信息
	m_textRenderer->DrawD2DText(
		text.c_str(),
		20, 20,
		D2D1::ColorF(1, 1, 1, 1),
		ETextType::DEBUG);

	//绘制渲染统计
	if (m_isShow)
	{
		m_textRenderer->DrawD2DText(
			buf,
			20, 250,
			D2D1::ColorF(1, 1, 1, 1),
			ETextType::DEBUG);
	}

	// 头顶名字
	/*if (auto* box = m_scene->GetSceneObject("box"))
	{
		XMFLOAT3 camPos = m_camera.GetPosition3f();
		XMFLOAT4X4 world = box->worldTransform;
		XMFLOAT3 pos = { world._41, world._42 + 1.0f, world._43 };


		float dist = XMVectorGetX(
			XMVector3Length(XMLoadFloat3(&pos) - XMLoadFloat3(&camPos)));
		XMFLOAT2 screenPos;
		if (dist <= 60.0f && WorldToScreen(pos, screenPos))
		{
			float alpha = 1.0f - max(0.0f, dist - 40.0f) / 20.0f;
			std::wstring name = WString::ToWstring("box");

			m_textRenderer->DrawD2DCenter(
				name.c_str(),
				screenPos.x, screenPos.y,
				D2D1::ColorF(1, 1, 1, alpha),
				ETextType::NPC_NAME);
		}
	}*/
	//绘制结束
	m_textRenderer->End();
}

void BoxGameApp::DrawDebugLine()
{
	m_debugRenderer->Begin();
	//包围盒统一从碰撞组件取
	auto AddBounds = [this](Node* obj)
	{
		if (!obj)return;
		auto* colComp = obj->GetComponent<CollisionComponent>();
		if (!colComp)return;
		if (!colComp->isUseBound)return;
		if (m_debugRenderer->isDebug_AABB)
			m_debugRenderer->AddAABB(colComp->bound_AABB, XMFLOAT4(0, 1, 0, 1));
		if (m_debugRenderer->isDebug_OBB)
			m_debugRenderer->AddOBB(colComp->bound_OBB, XMFLOAT4(1, 0, 0, 1));
	};

	//碰撞对象
	std::vector<Node*> drawnBounds;
	for (auto* obj : m_scene->GetCollisionNodes())
	{
		AddBounds(obj);
		drawnBounds.push_back(obj);
	}

	//视锥体
	if (m_debugRenderer->isDdebug_Frustum)
	{
		if (m_debugRenderer->isLock_Frustum)
		{
			if (m_debugRenderer->hasLockedCorners)
			{
				m_debugRenderer->AddFrustum(
					m_debugRenderer->lockedCorners, 
					XMFLOAT4(1, 1, 0, 1));
			}
			else
			{
				XMFLOAT3 corners[8];
				m_scene->GetFrustum().GetCorners(corners);
				m_debugRenderer->AddFrustum(corners, XMFLOAT4(1, 1, 0, 1));
			}
		}
	}
	//绘制
	m_debugRenderer->Draw(m_commandList.Get(), m_PSOs["Debug"].Get());
}

void BoxGameApp::UpdateController(float dt)
{
	//鼠标偏移量更新
	m_inputManage->UpdateMouse(m_hWnd, m_isCaptureMouse);

	//鼠标锁定切换
	if (m_inputManage->IsActionEnd(EAction::MOUSE_LOCK))
	{
		m_isCaptureMouse = !m_isCaptureMouse;
		m_inputManage->ResetKey();
		if (m_isCaptureMouse)
		{
			SetCapture(m_hWnd);
			ShowCursor(FALSE);
		}
		else
		{
			ReleaseCapture();
			ShowCursor(TRUE);
		}
		m_inputManage->ResetOffset();
	}

	//锁定视锥体
	if (m_inputManage->IsActionEnd(EAction::CONES_LOCK))
	{
		m_debugRenderer->isLock_Frustum = !m_debugRenderer->isLock_Frustum;
		if (m_debugRenderer->isLock_Frustum)
		{
			// 锁定,缓存当前视锥体
			XMFLOAT3 corners[8];
			m_scene->GetFrustum().GetCorners(corners);
			memcpy(m_debugRenderer->lockedCorners,
				corners,
				sizeof(XMFLOAT3) * 8);
			m_debugRenderer->hasLockedCorners = true;
		}
		else
		{
			// 解锁,清除缓存
			m_debugRenderer->hasLockedCorners = false;
		}
	}
	//视锥体可视化
	if (m_inputManage->IsActionEnd(EAction::CONES_VIEW))
	{
		m_debugRenderer->isDdebug_Frustum = !m_debugRenderer->isDdebug_Frustum;
	}
	//AABB可视化
	if (m_inputManage->IsActionEnd(EAction::AABB_VIEW))
	{
		m_debugRenderer->isDebug_AABB = !m_debugRenderer->isDebug_AABB;
	}
	//OBB可视化
	if (m_inputManage->IsActionEnd(EAction::OBB_VIEW))
	{
		m_debugRenderer->isDebug_OBB = !m_debugRenderer->isDebug_OBB;
	}
	if (m_inputManage->IsActionEnd(EAction::DEBUG_INFO))
	{
		m_isShow = !m_isShow;
	}

	//相机切换
	if (m_inputManage->IsActionEnd(EAction::CAMERA_FPS))
	{
		// 第一人称
		if(Node* player =m_scene->GetNode("player"))
			m_camControl = std::make_unique<FPSCameraController>(player);
		else
			m_camControl = std::make_unique<FlyCameraController>();
		
	}
	else if (m_inputManage->IsActionEnd(EAction::CAMERA_TPS))
	{
		// 第三人称
		Node* player = m_scene->GetNode("player");
		if (player||m_rayCaster.get())
			m_camControl = std::make_unique<TPSCameraController>(player, m_rayCaster.get());
		else
			m_camControl = std::make_unique<FlyCameraController>();
	}
	else if (m_inputManage->IsActionEnd(EAction::CAMERA_FLY))
	{
		// 自由相机
		m_camControl = std::make_unique<FlyCameraController>();
	
	}
		

	//更新摄像机
	if (m_camControl)m_camControl->UpdateCamControl(dt, m_inputManage.get(), &m_camera);
}

void BoxGameApp::UpdateMainPassCB(float dt)
{

	XMMATRIX view = m_camera.GetView();
	XMMATRIX proj = m_camera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMVECTOR det = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&det, view);
	det = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&det, proj);
	det = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&det, viewProj);

	XMStoreFloat4x4(&m_mainPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_mainPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_mainPassCB.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_mainPassCB.invView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_mainPassCB.invProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_mainPassCB.invViewProj, XMMatrixTranspose(invViewProj));

	m_mainPassCB.eyePosW = m_camera.GetPosition3f();
	m_mainPassCB.renderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
	m_mainPassCB.invRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
	m_mainPassCB.nearZ = 1.0f;
	m_mainPassCB.farZ = 1000.0f;
	m_mainPassCB.totalTime = m_timer.TotalTime();
	m_mainPassCB.detalTime = m_timer.DetalTime();
	//环境光
	m_mainPassCB.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	//方向光
	m_mainPassCB.lights[0].direction = XMFLOAT3{ -0.27735f, -0.57735f, -0.57735f };
	m_mainPassCB.lights[0].power = XMFLOAT3{ 0.8f, 0.8f, 0.8f };
	//点光
	/*m_mainPassCB.lights[1].position = XMFLOAT3{ -2.0f,1.0f,3.0f };
	m_mainPassCB.lights[1].decayStart = 1.0f;
	m_mainPassCB.lights[1].decayEnd = 10.0f;
	m_mainPassCB.lights[1].power = { 0.8f,0.5f,0.5f };*/

	m_mainPassCB.isClipPlane = 0;//默认不裁剪

	// 更新阴影矩阵
	// 正交盒只覆盖场景中需要投射阴影的物体范围
	// 需要自行把控
	const XMFLOAT3 sceneCenter = {  4.0f, 4.0f, 3.0f };
	const float    sceneRadius = 40.0f; //世界半径
	m_shadowMap->UpdateLightMat(
		m_mainPassCB.lights[0].direction, sceneCenter, sceneRadius, m_mainPassCB);

	const PassConstants& shadowCB = m_shadowMap->GetShadowPassCB();
	m_mainPassCB.shadowTransform = shadowCB.shadowTransform;


	auto curPassCB = m_curFrameResource->passCBuffer.get();
	curPassCB->CopyData(0, m_mainPassCB);
	curPassCB->CopyData(8, shadowCB);

	//上传更新反射过程常量缓冲区
	if (m_waterRT)
	{
		m_waterRT->UpdateFromMainPass(m_mainPassCB);
		curPassCB->CopyData(1, m_waterRT->GetRefPassCB(0));
	}
	if (m_envRT)
	{
		m_envRT->UpdateFromMainPass(m_mainPassCB);
		for (UINT i = 0; i < 6; i++)
			curPassCB->CopyData(2+i, m_envRT->GetRefPassCB(i));
	}

}

void BoxGameApp::GameUpdate()
{
	float dt=m_timer.DetalTime();
	UpdateController(dt);//更新
	m_camera.UpdateViewMatrix();

	// 更新视锥体
	m_scene->SetFrustum(m_camera.GetView4x4f(), m_camera.GetProj4x4f());
	// 更新观察点
	m_scene->SetCameraEye(m_camera.GetPosition3f());

	//切换帧资源
	m_curFrameResourceIndex = (m_curFrameResourceIndex + 1) % numFrameResources;
	m_curFrameResource = m_frameResources[m_curFrameResourceIndex].get();

	if (m_curFrameResource->fence != 0 &&
		m_fence->GetCompletedValue() < m_curFrameResource->fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);;
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_curFrameResource->fence,
			eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	// 场景更新
	m_scene->Update(dt, m_curFrameResource);

	//更新过程常量缓冲区
	UpdateMainPassCB(dt);
	
	//输入管理更新
	m_inputManage->EndUpdate();

}

void BoxGameApp::GameRender()
{
	//======不考虑开启MSAA写法======

	//获取对应帧资源的命令分配器
	auto cmdAllocator = m_curFrameResource->cmdAllocator;
	//重置命令分配器
	ThrowIfFailed(cmdAllocator->Reset());
	//重置命令列表(传入PSO)
	ThrowIfFailed(m_commandList->Reset(cmdAllocator.Get(), m_PSOs["Opaque"].Get()));

	//设置视口和裁切矩形
	m_commandList->RSSetViewports(1, &m_screenViewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	//资源状态转换(呈现->绘制)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &barrier);

	//清空RTV
	auto bbv = GetCurrentBackBufferView();
	m_commandList->ClearRenderTargetView(
		bbv,
		DirectX::Colors::LightSteelBlue,
		0, nullptr);

	//清空DSV
	auto dsv = GetDepthStencilView();
	m_commandList->ClearDepthStencilView(
		dsv,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0,
		0, nullptr);

	//输出装配阶段
	bbv = GetCurrentBackBufferView();
	dsv = GetDepthStencilView();
	m_commandList->OMSetRenderTargets(1,&bbv,true,&dsv);	
	
	// 设置描述符堆
	ID3D12DescriptorHeap* heaps[] = { m_globalSrvHeap.Get()};
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	
	//设置根签名
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// ====== 设置根参数 ======

	//设置过程常量缓冲区资源
	auto passCB = m_curFrameResource->passCBuffer->Resource();
	m_commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// 设置地形常量缓冲区
	auto* terrainBuffer = m_curFrameResource->terrainBuffer->Resource();
	m_commandList->SetGraphicsRootConstantBufferView(2, terrainBuffer->GetGPUVirtualAddress());

	//设置材质缓冲区资源
	auto matBuffer = m_curFrameResource->matBuffer->Resource();
	m_commandList->SetGraphicsRootShaderResourceView(4, matBuffer->GetGPUVirtualAddress());
	
	//设置天空盒纹理
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDesc(
		m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart());
	//偏移到天空盒的描述符句柄处
	skyTexDesc.Offset(m_scene->GetSkyBoxIndex(), m_cbvSrvDescSize);
	m_commandList->SetGraphicsRootDescriptorTable(3, skyTexDesc);

	//2D纹理根描述符从堆首开始
	m_commandList->SetGraphicsRootDescriptorTable(
		7, m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart());

	//地形高度纹理
	UINT heightIndex = m_scene->GetTextureSrvIndex("terrainHeight");
	if (heightIndex != UINT_MAX)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE heightDesc(
			m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart(),
			heightIndex, m_cbvSrvDescSize);
		m_commandList->SetGraphicsRootDescriptorTable(8, heightDesc);
	}

	// 骨骼变换矩阵
	auto* boneBuffer = m_curFrameResource->boneBuffer->Resource();
	m_commandList->SetGraphicsRootShaderResourceView(6, boneBuffer->GetGPUVirtualAddress());

	//阴影贴图
	m_commandList->SetGraphicsRootDescriptorTable(9, m_shadowMap->GetHGpuSrv());

	
	//绘制渲染项 相同pso统一渲染

	//绘制阴影贴图
	DrawShadowMap();
	
	//绘制反射纹理
	DrawReflections();

	//不透明渲染
	//有曲面反射的话,切换采样立方体反射纹理
	m_commandList->SetGraphicsRootDescriptorTable(
		5, m_envRT->GetHGpuSrv());

	m_scene->Draw(m_PSOs["Opaque"].Get(),
		RenderLayer::Opaque, m_curFrameResource);

	m_scene->Draw(m_PSOs["Skinned"].Get(),
		RenderLayer::Skinned, m_curFrameResource);

	m_scene->Draw(m_PSOs["Terrain"].Get(),
		RenderLayer::Terrain, m_curFrameResource);

	//天空盒渲染
	m_scene->Draw(m_PSOs["SkyBox"].Get(),
		RenderLayer::SkyBox, m_curFrameResource);

	

	//AlphaTest物体渲染
	// m_scene->Draw(m_PSOs["AlphaTest"].Get(),
	// RenderLayer::AlphaTest, m_curFrameResource);
	
	//采样平面反射纹理
	m_commandList->SetGraphicsRootDescriptorTable(
		5, m_waterRT->GetHGpuSrv());

	m_scene->Draw(m_PSOs["Mirror"].Get(),
		RenderLayer::Mirror, m_curFrameResource);

	m_scene->Draw(m_PSOs["Wave"].Get(),
		RenderLayer::Wave, m_curFrameResource);
	

	//透明物体渲染(一定放最后)
	m_scene->Draw(m_PSOs["Transparent"].Get(),
		RenderLayer::Transparent, m_curFrameResource);

	//绘制调试线
	DrawDebugLine();

	//资源状态转换(绘制->呈现)
	//在D2D里渲染文字后改了
	/*barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &barrier);*/

	//关闭
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	//绘制文字
	DrawD2DText();

	//呈现
	ThrowIfFailed(m_swapChain->Present(0, 0));
	//交换前后台缓冲区
	m_currBackBuffer = (m_currBackBuffer + 1) % m_SCBCount;

	//设置当前帧资源围栏值
	m_curFrameResource->fence = ++m_currentFence;
	m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

void BoxGameApp::BuildRootSignature()
{
	//天空盒纹理t0,space0
	CD3DX12_DESCRIPTOR_RANGE skyTable;//描述符表
	skyTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 
		1, 0, 0);

	//反射纹理  t0,space2
	CD3DX12_DESCRIPTOR_RANGE refTable;
	refTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1, 0, 2);

	//其他纹理(法线,漫反射)t1,space0
	CD3DX12_DESCRIPTOR_RANGE texTable;//描述符表
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		m_scene->GetTextureNum(), 1, 0);

	//高度纹理t1,space1
	CD3DX12_DESCRIPTOR_RANGE heightTable;
	heightTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE shadowTable;
	shadowTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 
		1, 1, 2);

	// 填写根参数
	CD3DX12_ROOT_PARAMETER slotRootParameter[10];

	// 常量缓冲区根参数
	slotRootParameter[0].InitAsConstantBufferView(0); // objCB     b0
	slotRootParameter[1].InitAsConstantBufferView(1); // passCB    b1
	slotRootParameter[2].InitAsConstantBufferView(2); // terrainCB b2

	// t0寄存器更参数
	slotRootParameter[3].InitAsDescriptorTable(1, &skyTable, D3D12_SHADER_VISIBILITY_PIXEL); // skyBox t0 sapce0
	slotRootParameter[4].InitAsShaderResourceView(0, 1);                                     // matB   t0 sapce1
	slotRootParameter[5].InitAsDescriptorTable(1, &refTable, D3D12_SHADER_VISIBILITY_PIXEL); // ref    t0 sapce2
	slotRootParameter[6].InitAsShaderResourceView(0, 3);                                     // bone   t0 space3
	
	// t1寄存器根参数
	slotRootParameter[7].InitAsDescriptorTable(1,&texTable, D3D12_SHADER_VISIBILITY_PIXEL);     // tex    t1 sapce0
	slotRootParameter[8].InitAsDescriptorTable(1, &heightTable, D3D12_SHADER_VISIBILITY_DOMAIN);// height t1 space1
	slotRootParameter[9].InitAsDescriptorTable(1, &shadowTable, D3D12_SHADER_VISIBILITY_PIXEL);// height t1 space2

	auto staticSamplers = GetStaticSamplers();//静态采样器

	CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(
		_countof(slotRootParameter),//根参数数量
		slotRootParameter,			//根参数
		staticSamplers.size(),		//静态采样器数量
		staticSamplers.data(),		//静态采样器
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);//需要顶点输入布局

	Microsoft::WRL::ComPtr<ID3DBlob> seriRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorInfo = nullptr;

	//序列化根签名
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		seriRootSignature.GetAddressOf(),
		errorInfo.GetAddressOf());

	//先输出错误信息
	if (errorInfo)OutputDebugStringA((char*)errorInfo->GetBufferPointer());
	ThrowIfFailed(hr);

	

	//创建根签名
	ThrowIfFailed(m_d3dDevice->CreateRootSignature(
		0,
		seriRootSignature->GetBufferPointer(),
		seriRootSignature->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

void BoxGameApp::BuildFrameResource()
{
	//地形数据是运行时常量无需更新
	TerrainConstants terrainCB{};
	terrainCB.gTerrainSize = { m_scene->GetTerrainSize(), m_scene->GetTerrainSize() };
	terrainCB.gTerrainTexSize = m_scene->GetTextureSize("terrainHeight");
	terrainCB.gTerrainScale = m_scene->GetTerrainHeightScale();

	for (int i = 0; i < numFrameResources; i++)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(
			m_d3dDevice.Get(),
			9,//过程常量+平面反射常量+Cube反射CB+阴影CB
			m_scene->GetSubmeshNum(),   // 对象CB
			m_scene->GetMaterialNum()));// 材质缓冲区

		m_frameResources.back()->terrainBuffer->CopyData(0, terrainCB);
	}
}

void BoxGameApp::BuildShadersAndInputLayout()
{
	//外部导入着色器宏定义
	const D3D_SHADER_MACRO alphaTestDefine[] =
	{
		"ALPHA_TEST","1",
		NULL,NULL
	};
	const D3D_SHADER_MACRO skinnedDefine[] =
	{
		"SKINNED","1",
		NULL,NULL
	};

	//hlsl着色器编译
	//标准着色器
	m_shaders["standardVS"] = D3DTools::CompileShader(L"..\\shaders\\standard.hlsl", nullptr, "VS", "vs_5_1");
	m_shaders["standardPS"] = D3DTools::CompileShader(L"..\\shaders\\standard.hlsl", nullptr, "PS", "ps_5_1");
	//alpha测试
	m_shaders["alphaTestPS"] = D3DTools::CompileShader(L"..\\shaders\\standard.hlsl", alphaTestDefine, "PS", "ps_5_1");
	//蒙皮着色器
	m_shaders["skinnedVS"] = D3DTools::CompileShader(L"..\\shaders\\standard.hlsl", skinnedDefine, "VS", "vs_5_1");
	//镜面/玻璃
	m_shaders["mirrorPS"] = D3DTools::CompileShader(L"..\\shaders\\mirror.hlsl", nullptr, "PS", "ps_5_1");
	//动态水面
	m_shaders["wavePS"] = D3DTools::CompileShader(L"..\\shaders\\wave.hlsl", nullptr, "PS", "ps_5_1");
	//地形
	m_shaders["terrainVS"] = D3DTools::CompileShader(L"..\\shaders\\terrain.hlsl", nullptr, "VS", "vs_5_1");
	m_shaders["terrainHS"] = D3DTools::CompileShader(L"..\\shaders\\terrain.hlsl", nullptr, "HS", "hs_5_1");
	m_shaders["terrainDS"] = D3DTools::CompileShader(L"..\\shaders\\terrain.hlsl", nullptr, "DS", "ds_5_1");
	m_shaders["terrainPS"] = D3DTools::CompileShader(L"..\\shaders\\terrain.hlsl", nullptr, "PS", "ps_5_1");
	
	//天空盒着色器
	m_shaders["skyBoxVS"] = D3DTools::CompileShader(L"..\\shaders\\skyBox.hlsl", nullptr, "VS", "vs_5_1");
	m_shaders["skyBoxPS"] = D3DTools::CompileShader(L"..\\shaders\\skyBox.hlsl", nullptr, "PS", "ps_5_1");
	//纯色着色器
	m_shaders["colorVS"] = D3DTools::CompileShader(L"..\\shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	m_shaders["colorPS"] = D3DTools::CompileShader(L"..\\shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	

	//输入布局描述
	m_inputLayout =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",  0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,   0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENT", 0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
	//纯色渲染输入布局描述
	m_colorInputLayout =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		
	};
	//蒙皮输入布局描述
	m_skinInputLayout =
	{
		{"POSITION",    0,DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",      0,DXGI_FORMAT_R32G32B32_FLOAT,   0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",    0,DXGI_FORMAT_R32G32_FLOAT,      0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENT",     0,DXGI_FORMAT_R32G32B32_FLOAT,   0,32,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"BONEWEIGHTS", 0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,44,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"BONEINDICES", 0,DXGI_FORMAT_R8G8B8A8_UINT,     0,60,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
	//地形输入布局描述
	m_terrainInputLayout =
	{
		// slot0 ,每个顶点
		{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},

		// slot1 ,每个patch实例,4个角点+1个细分因子
		{"PATCH_C",   0,DXGI_FORMAT_R32G32_FLOAT,		1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
		{"PATCH_C",   1,DXGI_FORMAT_R32G32_FLOAT,		1, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
		{"PATCH_C",   2,DXGI_FORMAT_R32G32_FLOAT,		1, 16,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
		{"PATCH_C",   3,DXGI_FORMAT_R32G32_FLOAT,		1, 24,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
		{"PATCH_TESS",0,DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},
	};

}

D3D12_SHADER_BYTECODE BoxGameApp::Shader(const char* name) const
{
	auto it = m_shaders.find(name);
	if (it == m_shaders.end())
	{
		OutputDebugStringA((std::string("[PSO] missing shader: ") + name + "\n").c_str());
		return { nullptr, 0 };
	}
	return { reinterpret_cast<BYTE*>(it->second->GetBufferPointer()),
             it->second->GetBufferSize() };
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC BoxGameApp::BasePsoDesc() const
{
	//不透明基座:其余 PSO 都以它为起点改状态
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.InputLayout = { m_inputLayout.data(),(UINT)m_inputLayout.size() };
	desc.pRootSignature = m_rootSignature.Get();
	desc.VS = Shader("standardVS");
	desc.PS = Shader("standardPS");
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = m_backBufferFormat;
	desc.SampleDesc.Count = m_is4xMsaaState ? 4 : 1;
	desc.SampleDesc.Quality = m_is4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	desc.DSVFormat = m_depthStencilFormat;
	return desc;
}

D3D12_RENDER_TARGET_BLEND_DESC BoxGameApp::AlphaBlendDesc() const
{
	//透明混合描述(10 个成员全写,避免残留未初始化)
	D3D12_RENDER_TARGET_BLEND_DESC d{};
	d.BlendEnable = true;//开启透明混合
	d.LogicOpEnable = false;//关闭逻辑运算符
	d.SrcBlend = D3D12_BLEND_SRC_ALPHA; //源RGB混合因子用源alpha
	d.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//目标RGB混合因子用源alpha取反
	d.BlendOp = D3D12_BLEND_OP_ADD;     //RGB混合运算符加法
	d.SrcBlendAlpha = D3D12_BLEND_ONE;  //源A混合因子为1
	d.DestBlendAlpha = D3D12_BLEND_ZERO;//目标A混合因子为0
	d.BlendOpAlpha = D3D12_BLEND_OP_ADD;//A混合运算符加法
	d.LogicOp = D3D12_LOGIC_OP_NOOP;    //逻辑运算符,没用
	d.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//全写入后台缓冲区
	return d;
}

void BoxGameApp::CreatePso(const char* name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
	HRESULT hr = m_d3dDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs[name]));
	if (FAILED(hr))
	{
		char buf[600];
		sprintf_s(buf,
			"[PSO] %s FAILED hr=0x%08X | VS=%p/%zu PS=%p/%zu HS=%p/%zu DS=%p/%zu | NumRT=%u RTV0=%d DSV=%d | Sample=%u/%u Topo=%d IL=%zu\n",
			name, (unsigned)hr,
			desc.VS.pShaderBytecode, desc.VS.BytecodeLength,
			desc.PS.pShaderBytecode, desc.PS.BytecodeLength,
			desc.HS.pShaderBytecode, desc.HS.BytecodeLength,
			desc.DS.pShaderBytecode, desc.DS.BytecodeLength,
			desc.NumRenderTargets, (int)desc.RTVFormats[0], (int)desc.DSVFormat,
			desc.SampleDesc.Count, desc.SampleDesc.Quality,
			(int)desc.PrimitiveTopologyType, desc.InputLayout.NumElements);
		OutputDebugStringA(buf);   //VS 输出窗口能看到
	}
	ThrowIfFailed(hr);
}

void BoxGameApp::BuildOpaquePso()
{
	auto desc = BasePsoDesc();
	CreatePso("Opaque", desc);
}

void BoxGameApp::BuildSkinnedPso()
{
	auto desc = BasePsoDesc();
	desc.InputLayout = { m_skinInputLayout.data(),(UINT)m_skinInputLayout.size() };
	desc.VS = Shader("skinnedVS");
	CreatePso("Skinned", desc);
}

void BoxGameApp::BuildTerrainPso()
{
	auto desc = BasePsoDesc();
	desc.InputLayout =
	{
		m_terrainInputLayout.data(),
		(UINT)m_terrainInputLayout.size()
	};
	desc.VS = Shader("terrainVS");
	desc.HS = Shader("terrainHS");
	desc.DS = Shader("terrainDS");
	desc.PS = Shader("terrainPS");
	//patch
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	//desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	CreatePso("Terrain", desc);
}

void BoxGameApp::BuildReflectPso()
{
	auto desc = BasePsoDesc();
	desc.RasterizerState.FrontCounterClockwise = TRUE;   //三角形绕序翻转
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	CreatePso("ReflectOpaque", desc);
}

void BoxGameApp::BuildShadowPso()
{
	// 阴影投射只需要VS,不需要PS;没有渲染目标,只写深度
	auto setDepthOnly = [](D3D12_GRAPHICS_PIPELINE_STATE_DESC& d)
		{
			d.PS = { nullptr, 0 };
			d.NumRenderTargets = 0;
			d.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
			d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			// 深度偏移:抑制自阴影痤疮
			d.RasterizerState.DepthBias = 1000;
			d.RasterizerState.DepthBiasClamp = 0.0f;
			d.RasterizerState.SlopeScaledDepthBias = 2.0f;
		};

	// 普通/镜面/水面物体,复用standardVS
	auto desc = BasePsoDesc();
	setDepthOnly(desc);
	CreatePso("ShadowOpaque", desc);

	//蒙皮物体,复用skinnedVS
	desc = BasePsoDesc();
	desc.InputLayout = { m_skinInputLayout.data(),(UINT)m_skinInputLayout.size() };
	desc.VS = Shader("skinnedVS");
	setDepthOnly(desc);
	CreatePso("ShadowSkinned", desc);

	//地形,复用地形VS/HS/DS
	desc = BasePsoDesc();
	desc.InputLayout = { m_terrainInputLayout.data(),(UINT)m_terrainInputLayout.size() };
	desc.VS = Shader("terrainVS");
	desc.HS = Shader("terrainHS");
	desc.DS = Shader("terrainDS");
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	setDepthOnly(desc);
	CreatePso("ShadowTerrain", desc);
}

void BoxGameApp::BuildTransparentPso()
{
	auto desc = BasePsoDesc();
	// 默认关闭,用于代替AlphaTest,但要开启MSAA
	desc.BlendState.AlphaToCoverageEnable = false;
	// 可以为最多8个渲染目标设置不同的混合参数
	desc.BlendState.RenderTarget[0] = AlphaBlendDesc();
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;//关闭深度写入
	desc.DepthStencilState.DepthEnable = true;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	CreatePso("Transparent", desc);
}

void BoxGameApp::BuildMirrorPso()
{
	
	auto desc = BasePsoDesc();
	desc.PS = Shader("mirrorPS");
	desc.BlendState.RenderTarget[0] = AlphaBlendDesc();//用透明混合的
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;//关闭深度写入
	desc.DepthStencilState.DepthEnable = true;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CreatePso("Mirror", desc);
}

void BoxGameApp::BuildWavePso()
{
	auto desc = BasePsoDesc();
	desc.PS = Shader("wavePS");
	desc.BlendState.RenderTarget[0] = AlphaBlendDesc();
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthEnable = true;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CreatePso("Wave", desc);
}

void BoxGameApp::BuildAlphaTestPso()
{
	auto desc = BasePsoDesc();
	desc.PS = Shader("alphaTestPS");
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	CreatePso("AlphaTest", desc);
}

void BoxGameApp::BuildWirePso()
{
	auto desc = BasePsoDesc();
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	CreatePso("Wire", desc);
}

void BoxGameApp::BuildSkyBoxPso()
{
	auto desc = BasePsoDesc();
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	//深度测试<=保留 防止清理深度缓冲为1时,NDC归一化为1的值被剔除
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.VS = Shader("skyBoxVS");
	desc.PS = Shader("skyBoxPS");
	CreatePso("SkyBox", desc);
}

void BoxGameApp::BuildDebugPso()
{
	auto desc = BasePsoDesc();
	desc.InputLayout = { m_colorInputLayout.data(),(UINT)m_colorInputLayout.size() };
	desc.VS = Shader("colorVS");
	desc.PS = Shader("colorPS");
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//不剔除
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;//线
	CreatePso("Debug", desc);
}

void BoxGameApp::BuildPSO()
{
	//创建渲染管线状态对象
	BuildOpaquePso();       //不透明
	BuildSkinnedPso();      //蒙皮
	BuildTerrainPso();      //地形
	BuildReflectPso();      //反射对象
	BuildShadowPso();
	BuildTransparentPso();  //透明混合物体
	BuildMirrorPso();       //镜面
	BuildWavePso();         //动态水面
	BuildAlphaTestPso();    //开启AlphaTest
	BuildWirePso();         //线框
	BuildSkyBoxPso();       //天空盒
	BuildDebugPso();        //调试层
}

void BoxGameApp::BuildScene()
{
#pragma region 纹理
	// ======== 构建纹理 ========
	m_scene->RegisterTexture("brick_diff", L"../textures/brick_diff.dds");             // 砖_diff
	m_scene->RegisterTexture("brick_nor", L"../textures/brick_nor.dds");              // 砖_nor

	m_scene->RegisterTexture("grass_diff", L"../textures/grass_diff.dds");             // 草地1_diff
	m_scene->RegisterTexture("grass_nor", L"../textures/grass_nor.dds");              // 草地1_nor

	m_scene->RegisterTexture("shazi_diff", L"../textures/shazi_diff.dds");             // 草地2_diff
	m_scene->RegisterTexture("shazi_nor", L"../textures/shazi_nor.dds");              // 草地2_nor

	m_scene->RegisterTexture("shashi_diff", L"../textures/shashi_diff.dds");            // 草地3_diff
	m_scene->RegisterTexture("shashi_nor", L"../textures/shashi_nor.dds");             // 草地3_nor

	m_scene->RegisterTexture("rock_diff", L"../textures/rock_diff.dds");              // 草地4_diff
	m_scene->RegisterTexture("rock_nor", L"../textures/rock_nor.dds");               // 草地4_nor

	m_scene->RegisterTexture("grass1_diff", L"../textures/grass1_diff.dds");            // 草地5_diff
	m_scene->RegisterTexture("grass1_nor", L"../textures/grass1_nor.dds");             // 草地5_nor

	m_scene->RegisterTexture("skyBox_tex", L"../textures/skyBox.dds", true);           // 天空_cube

	m_scene->RegisterTexture("terrainHeight", L"../textures/terrain_hei_U16.dds", false, true);// 地形高度

	m_scene->RegisterTexture("water_low_nor", L"../textures/water_low_nor.dds");		  // 水低波_nor
	m_scene->RegisterTexture("water_high_nor", L"../textures/water_high_nor.dds");		  // 水高波_nor
#pragma endregion
	
#pragma region 网格
	// ======== 构建网格 ======== 
	GeoGenerator gen;

	auto water = gen.CreatGrid(500, 500, 50, 50);             // 水面
	auto sphere = gen.CreatSphere(0.5f, 20, 20);              // 天空盒						           
	auto box = gen.CreatBox(1, 1, 1, 1);                      // 基本几何体
	auto mesh1 = gen.LoadMesh("../models/AK-H.fbx");          // 外部模型 
	auto skinnedData = gen.LoadSkinnedMesh("../models/Start Walking.fbx");

	std::string animateName;
	for (auto& pair : skinnedData.clips)
	{
		const std::string& name = pair.first;
		animateName = name;
		AnimationClip& clip = pair.second;
		//注册动画
		m_scene->RegisterAnimate(name, std::move(clip));
	}
	//注册骨架
	m_scene->RegisterSkeleton("player_skeleton", std::move(skinnedData.skeleton));

	//保存子网格名字,后面构造描述
	std::vector<std::string>submeshNames;
	submeshNames.reserve(skinnedData.meshes.size());
	for (auto& mesh : skinnedData.meshes)
	{
		submeshNames.push_back(mesh.first);
	}

	m_scene->RegisterGeometry<SkeletonVertex>("geo_player", std::move(skinnedData.meshes), EVertexLayout::Skeleton);

	m_scene->RegisterGeometry<Vertex>("geo_mesh1", std::move(mesh1), "sub_mesh1");   // 外部模型
	m_scene->RegisterGeometry<Vertex>("geo_box", std::move(box), "sub_box");	 // 基本几何体
	m_scene->RegisterGeometry<Vertex>("geo_sphere", std::move(sphere), "sub_sphere");	 // 天空盒
	m_scene->RegisterGeometry<Vertex>("geo_water", std::move(water), "sub_water");   // 水面
#pragma endregion

#pragma region 材质
	// ======== 构建材质 ======== 

	// 无贴图
	auto* defaultMat = m_scene->RegisterMaterial(
		"mat_default", { 1,1,1,1 }, { 0.08f,0.08f,0.08f }, 0.8f);             // 标准漫反射材质
	auto* TransparentMat = m_scene->RegisterMaterial(
		"mat_transparent", { 1,1,1,0.3 }, { 0.04f, 0.04f, 0.04f }, 0.5f);     // 透明材质
	auto* plasticMat = m_scene->RegisterMaterial(
		"mat_plastic", { 0.8f, 0.8f, 0.85f, 1.0f }, { 0.4f,0.4f,0.4f }, 0.1f);// 塑料材质
	auto* mirrorMat = m_scene->RegisterMaterial(
		"mat_mirror", { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, 0.0f);// 镜面材质

	// *测试*(模型有两个材质)
	auto* orangeColor = m_scene->RegisterMaterial(
		skinnedData.materialNames[0], { 1.0f, 0.647f, 0.0f, 1.0f }, { 0.08f,0.08f,0.08f }, 0.8f);// 标准漫反射材质
	auto* whiteColor = m_scene->RegisterMaterial(
		skinnedData.materialNames[1], { 1,1,1,1 }, { 0.08f,0.08f,0.08f }, 0.8f);             // 标准漫反射材质

	// 有贴图
	auto* brickMat = m_scene->RegisterMaterial(
		"mat_brick", { 1,1,1,1 }, { 0.04f,0.04f,0.04f }, 0.8f,
		{
			{ "brick_diff", "brick_nor" ,{1,1},{0,0}, 0.0f,1.0f }
		}
	); // 砖墙纹理材质,1层纹理

	auto* grassMat = m_scene->RegisterMaterial(
		"mat_grass", { 1,1,1,1 }, { 0.02f,0.02f,0.02f }, 1.0f,
		{
			{ "shashi_diff", "shashi_nor" ,{0.06f, 0.06f},{0,0},1.0f,  0.4f },
			{ "shazi_diff", "shazi_nor" ,  {0.05f, 0.05f},{0,0},1.0f,  0.2f },
			{ "grass_diff", "grass_nor" ,  {0.07f, 0.07f},{0,0},1.0f,  0.1f },
			{ "grass1_diff", "grass1_nor" ,{0.12f, 0.12f},{0,0},1.0f,  0.1f }
		}); // 地形纹理材质

	auto* skyMat = m_scene->RegisterMaterial(
		"mat_skyBox", { 1,1,1,1 }, { 0.04f,0.04f,0.04f }, 0.8f,
		{
			{ "skyBox_tex", "" ,{20,20},{0,0}, 0.0f,1.0f }
		}); // 天空纹理材质


	auto* waterMat = m_scene->RegisterMaterial(
		"water", { 0.08f, 0.22f, 0.28f, 0.40f }, { 0.02f,0.02f,0.02f }, 0.05f,
		{
			{ "", "water_low_nor" ,{7,10},{0.03f,  0.02f}, 0.0f,1.0f },
			{ "", "water_high_nor" ,{13,10},{-0.02f,  0.05f}, 0.0f,0.8f }
		});// 水面纹理材质
#pragma endregion

#pragma region 场景对象
	// ======== 构建场景对象 ======== 

	//地形
	m_scene->BuildTerrain(grassMat, { -50, -3, 3.0f }, 500, 50, 3);

	//水面
	m_scene->AddRenderObject("water", "geo_water", "sub_water", waterMat, RenderLayer::Wave)
		->SetLocalTransform({ -50, -1, 3.0f });

	//天空盒对象
	m_scene->AddRenderObject("skyBox", "geo_sphere", "sub_sphere", skyMat, RenderLayer::SkyBox)
		->GetComponent<RenderComponent>()->isNeverCull = true; //设置为不剔除


	//5x5立方体阵列
	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 5; j++)
		{
			Node* obj_box = m_scene->AddRenderObject("box" + std::to_string(i) + std::to_string(j),
				"geo_box", "sub_box", brickMat, RenderLayer::Opaque);
			obj_box->SetLocalTransform(
				{ float(i * 2), 0.5f + float(j * 2), 3.0f },
				{ 0.0f,XM_PI * 0.25f + i * 0.2f,j * 0.2f });
			m_scene->AddCollisionFromNode(obj_box, ECollisionType::OBB);

		}
	}
	//外部模型对象
	Node* obj_Ak = m_scene->AddRenderObject("mesh1", "geo_mesh1", "sub_mesh1", plasticMat, RenderLayer::Opaque);
	obj_Ak->SetLocalTransform(
		{ 0.0f, 1.0f, 0.0f },
		{ XM_PI * 0.5f, 0, 0 },
		{ 0.005f,  0.005f,  0.005f });
	m_scene->AddCollisionFromNode(obj_Ak, ECollisionType::OBB);

	//构造子网格描述
	std::vector<SubmeshDesc> desc;
	desc.reserve(submeshNames.size());
	for (auto& name : submeshNames)
	{
		SubmeshDesc d = {};
		d.submeshName = name;
		d.layer = RenderLayer::Skinned;
		d.mat = m_scene->GetMaterial(name);
		desc.push_back(d);
	}


	Node* obj_player = m_scene->AddRenderObject("player", "geo_player", desc);
	obj_player->SetLocalTransform(
		{ -3.0f, 0.0f, 7.0f },
		{ 0, 0, 0 },
		{ 0.02f,  0.02f,  0.02f });
	m_scene->AddCollisionFromNode(obj_player, ECollisionType::Triangle);
	auto* skComp = m_scene->AddSkeletonFromNode(obj_player, "player_skeleton");
	//skComp->Play(m_scene->GetAnimate(animateName.c_str()));

	//obj_player->AddComponent<ControlComponent>(&m_camera,m_rayCaster.get(),m_inputManage.get());
#pragma endregion

	

	// 构建SRV堆
	m_scene->SaveSRVToHeap(
		m_d3dDevice.Get(),
		m_globalSrvHeap.Get(),
		m_srvSlotCount,
		m_cbvSrvDescSize);
	m_srvSlotCount += m_scene->GetSRVCount();
}

void BoxGameApp::BuildReflection()
{
	// 建立平面和立方体的反射资源
	
	//======== 建立平面反射资源 ========
	m_waterRT = std::make_unique<ReflectionRTT>(
		m_d3dDevice.Get(),
		EReflectType::PLANE,
		512, 512,
		m_backBufferFormat,
		m_depthStencilFormat);

	//分配堆
	auto cpuSrv =     AllocSrvCpu();
	auto gpuSrv =     AllocSrvGpu();
	auto cpuRtvBase = AllocRtvCpu();
	auto cpuDsv =     AllocDsvCpu();

	m_waterRT->BuildDesccriptors(
		cpuSrv, gpuSrv, cpuRtvBase, m_rtvDescSize, cpuDsv);

	m_waterRT->SetPlane({ 0.0f,1.0f,0.0f,1.0f });


	// ======== 建立立体反射资源 ========
	m_envRT = std::make_unique<ReflectionRTT>(
		m_d3dDevice.Get(),
		EReflectType::CUBE,
		512, 512,
		m_backBufferFormat,
		m_depthStencilFormat);
	// 分配堆
	cpuSrv =     AllocSrvCpu();
	gpuSrv =     AllocSrvGpu();
	cpuRtvBase = AllocRtvCpu();
	m_rtvSlotCount += 5;// 分配Cube加6
	cpuDsv =     AllocDsvCpu();
	

	m_envRT->BuildDesccriptors(
		cpuSrv, gpuSrv, cpuRtvBase, m_rtvDescSize, cpuDsv);

	m_envRT->SetCapturePos({ 0.0f, 1.0f, 0.0f });

}

void BoxGameApp::BuildShadowMap()
{
	m_shadowMap = std::make_unique<ShadowMap>(m_d3dDevice.Get(), 2048);

	auto cpuSrv = AllocSrvCpu();
	auto gpuSrv = AllocSrvGpu();
	auto cpuDsv = AllocDsvCpu();

	m_shadowMap->BuildDescriptors(cpuSrv, gpuSrv, cpuDsv);
}

void BoxGameApp::BuildGlobalDescriptorHeaps()
{
	m_rtvDescSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescSize = m_d3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//创建srv堆
	D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
	srvDesc.NumDescriptors = 64;
	srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&srvDesc, IID_PPV_ARGS(&m_globalSrvHeap)));

	//创建rtv堆
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
	rtvDesc.NumDescriptors = 16;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&rtvDesc, IID_PPV_ARGS(&m_globalRtvHeap)));

	//创建dsc堆
	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
	dsvDesc.NumDescriptors = 4;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvDesc, IID_PPV_ARGS(&m_globalDsvHeap)));

}

CD3DX12_CPU_DESCRIPTOR_HANDLE BoxGameApp::AllocSrvCpu()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_globalSrvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_srvSlotCount++, m_cbvSrvDescSize);//自动后推
}

CD3DX12_GPU_DESCRIPTOR_HANDLE BoxGameApp::AllocSrvGpu()
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart(),
		m_srvSlotCount - 1, m_cbvSrvDescSize);//与上面cpu对应,所以减1
}

CD3DX12_CPU_DESCRIPTOR_HANDLE BoxGameApp::AllocRtvCpu()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_globalRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_rtvSlotCount++, m_rtvDescSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE BoxGameApp::AllocDsvCpu()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_globalDsvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_dsvSlotCount++, m_dsvDescSize);
}

bool  BoxGameApp::WorldToScreen(const XMFLOAT3& world, XMFLOAT2& outScreen)
{

	XMVECTOR v = XMLoadFloat3(&world);
	XMMATRIX view = m_camera.GetView();
	XMMATRIX proj = m_camera.GetProj();

	// 变换到裁剪空间
	XMVECTOR clip4 = XMVector4Transform(XMVectorSetW(v, 1.0f), view * proj);
	float w = XMVectorGetW(clip4);

	if (w <= 0.0001f) return false; // 摄像机背后

	// 透视除法，得到NDC [-1,1]
	XMVECTOR ndc = XMVectorScale(clip4, 1.0f / w);
	float ndcX = XMVectorGetX(ndc);
	float ndcY = XMVectorGetY(ndc);
	float ndcZ = XMVectorGetZ(ndc);

	if (ndcX < -1.0f || ndcX > 1.0f) return false;
	if (ndcY < -1.0f || ndcY > 1.0f) return false;
	if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

	outScreen.x = (ndcX * 0.5f + 0.5f) * m_width;
	outScreen.y = (-ndcY * 0.5f + 0.5f) * m_height;   // Y 翻转

	return true;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> BoxGameApp::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWarp //点过滤+重复寻址
	{
		0,                               //寄存器编号
		D3D12_FILTER_MIN_MAG_MIP_POINT,  //过滤方式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, //u三轴寻址方式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, //v
		D3D12_TEXTURE_ADDRESS_MODE_WRAP  //w
	};

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp //点过滤+钳位寻址
	{
		1,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearWarp //双线性过滤+重复寻址
	{
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp //双线性过滤+钳位寻址
	{
		3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	};

	const CD3DX12_STATIC_SAMPLER_DESC anisoWarp //各项异性过滤+重复寻址
	{
		4,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0.0f,
		8 //最大各项异性值
	};

	const CD3DX12_STATIC_SAMPLER_DESC anisoClamp //各项异性过滤+钳位寻址
	{
		5,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f,
		8
	};

	const CD3DX12_STATIC_SAMPLER_DESC shadow //阴影图采样
	{
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, //比较过滤模式：缩小/放大用线性，MIP 用点采样
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,                //UVW 超出(0,1)时使用边框颜色
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,                //比较为小于等于
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK           //边框颜色为不透明黑,深度值0
	};

	return 
	{ 
		pointWarp, pointClamp, linearWarp, 
		linearClamp, anisoWarp, anisoClamp ,
		shadow
	};
}


