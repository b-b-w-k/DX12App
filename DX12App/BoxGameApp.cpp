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
#include "AnimationClip.h"
#include "Skeleton.h"




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
	BuildScene();         // 构建：几何体/纹理/材质/对象/SRV堆

	BuildReflection();           //建立反射资源
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
	for (auto* obj : m_scene->GetRenderObjects())
	{
		auto* renderComp = obj->GetComponent<RenderComponent>();
		if (!renderComp)continue;

		if (m_debugRenderer->isDebug_AABB)
			m_debugRenderer->AddAABB(renderComp->bound_AABB, XMFLOAT4(0, 1, 0, 1));
		if (m_debugRenderer->isDebug_OBB)
			m_debugRenderer->AddOBB(renderComp->bound_OBB, XMFLOAT4(1, 0, 0, 1));
	}
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
		m_camControl = std::make_unique<FPSCameraController>();
	else if (m_inputManage->IsActionEnd(EAction::CAMERA_TPS))
		m_camControl = std::make_unique<TPSCameraController>();
	else if (m_inputManage->IsActionEnd(EAction::CAMERA_FLY))
		m_camControl = std::make_unique<FlyCameraController>();

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

	auto curPassCB = m_curFrameResource->passCBuffer.get();
	curPassCB->CopyData(0, m_mainPassCB);

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

	//======着色器参数设置====
	
	ID3D12DescriptorHeap* heaps[] = { m_globalSrvHeap.Get()};
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	
	//设置根签名
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	//设置过程常量缓冲区资源
	auto passCB = m_curFrameResource->passCBuffer->Resource();
	m_commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
	
	//设置材质缓冲区资源
	auto matBuffer = m_curFrameResource->matBuffer->Resource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	
	//设置纹理资源
	//天空盒纹理
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDesc(
		m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart());
	//偏移到天空盒的描述符句柄处
	skyTexDesc.Offset(m_scene->GetSkyBoxIndex(), m_cbvSrvDescSize);
	m_commandList->SetGraphicsRootDescriptorTable(3, skyTexDesc);

	//2D纹理根描述符从堆首开始
	m_commandList->SetGraphicsRootDescriptorTable(
		4, m_globalSrvHeap->GetGPUDescriptorHandleForHeapStart());

	// 骨骼变换矩阵
	auto* boneBuffer = m_curFrameResource->boneBuffer->Resource();
	m_commandList->SetGraphicsRootShaderResourceView(6, boneBuffer->GetGPUVirtualAddress());

	//绘制渲染项 相同pso统一渲染
	
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
	CD3DX12_DESCRIPTOR_RANGE texTable0;//描述符表
	texTable0.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 
		1, 0, 0);//天空盒t0,space0

	CD3DX12_DESCRIPTOR_RANGE texTable1;//描述符表
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		m_scene->GetTextureNum(), 1, 0);//其他纹理t1

	
	CD3DX12_DESCRIPTOR_RANGE refTable;
	refTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1, 0, 2);//反射纹理t0,space2

	CD3DX12_ROOT_PARAMETER slotRootParameter[7];

	//常量缓冲区视图形式
	slotRootParameter[0].InitAsConstantBufferView(0);//objCB  b0
	slotRootParameter[1].InitAsConstantBufferView(1);//passCB b1
	slotRootParameter[2].InitAsShaderResourceView(0,1);//matB//作为着色器资源视图 t0 sapce1
	slotRootParameter[3].InitAsDescriptorTable(1,&texTable0, D3D12_SHADER_VISIBILITY_PIXEL);//skyBox t0 sapce0
	slotRootParameter[4].InitAsDescriptorTable(1,&texTable1, D3D12_SHADER_VISIBILITY_PIXEL);//tex t1 sapce0
	slotRootParameter[5].InitAsDescriptorTable(1,&refTable, D3D12_SHADER_VISIBILITY_PIXEL);//ref t0 sapce2
	slotRootParameter[6].InitAsShaderResourceView(0, 3);//bone t0 space3
	
	auto staticSamplers = GetStaticSamplers();//静态采样器

	CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(
		7,                    //根参数数量
		slotRootParameter,    //根参数
		staticSamplers.size(),//静态采样器数量
		staticSamplers.data(),//静态采样器
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
	// 支持12个角色*每128根骨骼
	const uint32_t maxBones = 12 * 128;

	for (int i = 0; i < numFrameResources; i++)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(
			m_d3dDevice.Get(),
			8,//过程常量+平面反射常量+Cube反射CB
			m_scene->GetSubmeshNum(), //对象CB
			m_scene->GetMaterialNum(),//材质缓冲区
			maxBones)); // 骨骼缓冲区
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

}

void BoxGameApp::BuildPSO()
{
	//创建渲染管线状态对象

	//================不透明================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc{};
	opaquePsoDesc.InputLayout = { m_inputLayout.data(),(UINT)m_inputLayout.size() };
	opaquePsoDesc.pRootSignature = m_rootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()),
		m_shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["standardPS"]->GetBufferPointer()),
		m_shaders["standardPS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = m_backBufferFormat;
	opaquePsoDesc.SampleDesc.Count= m_is4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m_is4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = m_depthStencilFormat;
	

	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&opaquePsoDesc,
		IID_PPV_ARGS(&m_PSOs["Opaque"])));

	//================蒙皮================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPsoDesc = opaquePsoDesc;
	skinnedPsoDesc.InputLayout = { m_skinInputLayout.data(),(UINT)m_skinInputLayout.size() };
	skinnedPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["skinnedVS"]->GetBufferPointer()),
		m_shaders["skinnedVS"]->GetBufferSize()
	};
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&skinnedPsoDesc,
		IID_PPV_ARGS(&m_PSOs["Skinned"])));

	//================地形================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC terrainPsoDesc = opaquePsoDesc;
	terrainPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["terrainPS"]->GetBufferPointer()),
		m_shaders["terrainPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&terrainPsoDesc,
		IID_PPV_ARGS(&m_PSOs["Terrain"])));

	//================反射对象渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ReflectOpaque = opaquePsoDesc;
	ReflectOpaque.RasterizerState.FrontCounterClockwise = TRUE;   //三角形绕序翻转
	ReflectOpaque.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&ReflectOpaque,
		IID_PPV_ARGS(&m_PSOs["ReflectOpaque"])));

	//================透明混合物体渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC tranBlendPsoDesc = opaquePsoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC transBlendDesc;//透明混合描述
	transBlendDesc.BlendEnable = true;//开启透明混合
	transBlendDesc.LogicOpEnable = false;//关闭逻辑运算符
	transBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; //源RGB混合因子用源alpha
	transBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//目标RGB混合因子用源alpha取反
	transBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;     //RGB混合运算符加法 
	transBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;  //源A混合因子为1
	transBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;//目标A混合因子为0
	transBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//A混合运算符加法
	transBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;    //逻辑运算符,没用
	transBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//全写入后台缓冲区

	// 默认关闭,用于代替AlphaTest,但要开启MASS
	tranBlendPsoDesc.BlendState.AlphaToCoverageEnable = false;
	// 可以为最多8个渲染目标设置不同的混合参数
	tranBlendPsoDesc.BlendState.RenderTarget[0] = transBlendDesc;
	tranBlendPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; //关闭深度写入
	tranBlendPsoDesc.DepthStencilState.DepthEnable = true;
	tranBlendPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&tranBlendPsoDesc,
		IID_PPV_ARGS(&m_PSOs["Transparent"])));

	//================镜面渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorPsoDesc = opaquePsoDesc;
	mirrorPsoDesc.PS = { 
		(BYTE*)m_shaders["mirrorPS"]->GetBufferPointer(),
						 m_shaders["mirrorPS"]->GetBufferSize() 
	};

	mirrorPsoDesc.BlendState.RenderTarget[0] = transBlendDesc;//用透明混合的
	mirrorPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; //关闭深度写入
	mirrorPsoDesc.DepthStencilState.DepthEnable = true;
	mirrorPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	mirrorPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&mirrorPsoDesc, IID_PPV_ARGS(&m_PSOs["Mirror"])));

	//================动态水面渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wavePsoDesc = mirrorPsoDesc;
	wavePsoDesc.PS = {
		(BYTE*)m_shaders["wavePS"]->GetBufferPointer(),
						 m_shaders["wavePS"]->GetBufferSize()
	};
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&wavePsoDesc, IID_PPV_ARGS(&m_PSOs["Wave"])));

	//================开启AlphaTest渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["alphaTestPS"]->GetBufferPointer()),
		m_shaders["alphaTestPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&alphaTestedPsoDesc,
		IID_PPV_ARGS(&m_PSOs["AlphaTest"])));

	//================线框渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wirePsoDesc = opaquePsoDesc;
	wirePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&wirePsoDesc,
		IID_PPV_ARGS(&m_PSOs["Wire"])));

	//================天空盒渲染================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyBoxPsoDesc = opaquePsoDesc;
	skyBoxPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面不剔除
	//深度测试<=保留 防止清理深度缓冲为1时,NDC归一化为1的值被剔除
	skyBoxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyBoxPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["skyBoxVS"]->GetBufferPointer()),
		m_shaders["skyBoxVS"]->GetBufferSize()
	};
	skyBoxPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["skyBoxPS"]->GetBufferPointer()),
		m_shaders["skyBoxPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&skyBoxPsoDesc,
		IID_PPV_ARGS(&m_PSOs["SkyBox"])));

	//================调试层渲染PSO================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.InputLayout = { m_colorInputLayout.data(),(UINT)m_colorInputLayout.size() };
	debugPsoDesc.VS=
	{
		reinterpret_cast<BYTE*>(m_shaders["colorVS"]->GetBufferPointer()),
		m_shaders["colorVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["colorPS"]->GetBufferPointer()),
		m_shaders["colorPS"]->GetBufferSize()
	};
	debugPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//不剔除
	debugPsoDesc.DepthStencilState.DepthEnable = FALSE;
	debugPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;//线

	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(
		&debugPsoDesc,
		IID_PPV_ARGS(&m_PSOs["Debug"])));

}

void BoxGameApp::BuildScene()
{
	m_scene = std::make_unique<Scene>(
		m_d3dDevice.Get(), 
		m_commandList.Get(), 
		m_cbvSrvDescSize);

	// ======== 构建纹理 ========
	m_scene->RegisterTexture("brick_diff",    L"../textures/brick_diff.dds");             // 砖_diff
	m_scene->RegisterTexture("brick_nor",     L"../textures/brick_nor.dds");              // 砖_nor
																				          
	m_scene->RegisterTexture("grass_diff",    L"../textures/grass_diff.dds");             // 草地1_diff
	m_scene->RegisterTexture("grass_nor",     L"../textures/grass_nor.dds");              // 草地1_nor
											  									          
	m_scene->RegisterTexture("shazi_diff",    L"../textures/shazi_diff.dds");             // 草地2_diff
	m_scene->RegisterTexture("shazi_nor",     L"../textures/shazi_nor.dds");              // 草地2_nor
																				          
	m_scene->RegisterTexture("shashi_diff",   L"../textures/shashi_diff.dds");            // 草地3_diff
	m_scene->RegisterTexture("shashi_nor",    L"../textures/shashi_nor.dds");             // 草地3_nor
																				          
	m_scene->RegisterTexture("rock_diff",     L"../textures/rock_diff.dds");              // 草地4_diff
	m_scene->RegisterTexture("rock_nor",      L"../textures/rock_nor.dds");               // 草地4_nor
																				          
	m_scene->RegisterTexture("grass1_diff",   L"../textures/grass1_diff.dds");            // 草地5_diff
	m_scene->RegisterTexture("grass1_nor",    L"../textures/grass1_nor.dds");             // 草地5_nor
																				          
	m_scene->RegisterTexture("skyBox_tex",    L"../textures/skyBox.dds", true);           // 天空_cube

	m_scene->RegisterTexture("terrainHeight", L"../textures/terrain_hei.dds", false,true);// 地形高度

	m_scene->RegisterTexture("water_low_nor", L"../textures/water_low_nor.dds");		  // 水低波_nor
	m_scene->RegisterTexture("water_high_nor",L"../textures/water_high_nor.dds");		  // 水高波_nor

	// 构建SRV堆
	m_scene->SaveSRVToHeap(
		m_d3dDevice.Get(),
		m_globalSrvHeap.Get(),
		m_srvSlotCount, 
		m_cbvSrvDescSize);
	m_srvSlotCount += m_scene->GetSRVCount();

	//高度采样器
	auto heightSampler = [this](float u, float v)->float
		{
			return m_scene->GetSamplerBilinear(u, v);
		};

	

	// ======== 构建网格 ======== 
	GeoGenerator gen;
	auto terrain = gen.CreatTerrain(500, 500, 200, 200,
		heightSampler, m_scene->GetTerrainHeightScale());     // 高度地形
	//auto planeMirror = gen.CreatGrid(4, 6, 4, 6);           // 平面镜
	auto water = gen.CreatGrid(500, 500, 50, 50);             // 水面
	auto sphere = gen.CreatSphere(0.5f, 20, 20);              // 天空盒						           
	auto box = gen.CreatBox(1, 1, 1, 1);                      // 基本几何体
	auto mesh1 = gen.LoadMesh("../models/AK-H.fbx");          // 外部模型 
	auto skinnedData = gen.LoadSkinnedMesh("../models/Start Walking.fbx");
	auto& matIndex = skinnedData.materialIndices;

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

	m_scene->RegisterGeometry<Vertex>("geo_terrain", std::move(terrain), "sub_terrain"); // 对象网格
	m_scene->RegisterGeometry<Vertex>("geo_mesh1",   std::move(mesh1),   "sub_mesh1");   // 外部模型
	m_scene->RegisterGeometry<Vertex>("geo_box",     std::move(box),     "sub_box");	 // 基本几何体
	m_scene->RegisterGeometry<Vertex>("geo_sphere",  std::move(sphere),  "sub_sphere");	 // 天空盒
	//m_scene->RegisterGeometry<Vertex>("geo_planeMirror", std::move(planeMirror), "sub_planeMirror"); // 平面镜
	m_scene->RegisterGeometry<Vertex>("geo_water",   std::move(water),   "sub_water");   // 水面

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
	

	// ======== 构建场景对象 ======== 

	//地形
	Node* obj_terrain = m_scene->AddRenderObject("terrain", "geo_terrain", "sub_terrain", grassMat, RenderLayer::Terrain);
	obj_terrain->SetLocalTransform({ -50, -3, 3.0f });
	//添加碰撞
	obj_terrain->AddComponent<CollisionComponent>()
		       ->SetCollisionType(ECollisionType::Triangle);

	//水面
	m_scene->AddRenderObject("water", "geo_water", "sub_water", waterMat, RenderLayer::Wave)
		   ->SetLocalTransform({ -50, -1, 3.0f });
	
	//天空盒对象
	m_scene->AddRenderObject("skyBox", "geo_sphere", "sub_sphere", skyMat, RenderLayer::SkyBox)
		   ->GetComponent<RenderComponent>()->neverCull = true; //设置为不剔除

	
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
			obj_box->AddComponent<CollisionComponent>()
				   ->SetCollisionType(ECollisionType::OBB);
		}
	}
	//外部模型对象
	Node* obj_Ak = m_scene->AddRenderObject("mesh1", "geo_mesh1", "sub_mesh1", plasticMat, RenderLayer::Opaque);
		obj_Ak->SetLocalTransform(
			{ 0.0f, 1.0f, 0.0f },
			{ XM_PI * 0.5f, 0, 0 },
			{ 0.005f,  0.005f,  0.005f });
	obj_Ak->AddComponent<CollisionComponent>()
		  ->SetCollisionType(ECollisionType::AABB);

	//构造子网格描述
	std::vector<SubmeshDesc> desc;
	desc.reserve(submeshNames.size());
	for (auto& name : submeshNames)
	{
		SubmeshDesc d = {};
		d.submeshName = name;
		d.layer = RenderLayer::Skinned;
		d.mat = defaultMat;
		desc.push_back(d);
	}


	Node* obj_player = m_scene->AddRenderObject("player", "geo_player", desc);
	obj_player->SetLocalTransform(
		{ -3.0f, 0.0f, 7.0f },
		{ 0, 0, 0 },
		{ 0.05f,  0.05f,  0.05f });
	obj_player->AddComponent<CollisionComponent>()
		->SetCollisionType(ECollisionType::Triangle);

	auto* skComp = obj_player->AddComponent<SkeletonComponent>();
	skComp->m_skeleton = m_scene->GetSkeleton("player_skeleton");
	//skComp->Play(m_scene->GetAnimate(animateName.c_str()));
	m_scene->AttachToView(obj_player, skComp);

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

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BoxGameApp::GetStaticSamplers()
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


	return 
	{ 
		pointWarp, pointClamp, linearWarp, 
		linearClamp, anisoWarp, anisoClamp 
	};
}


