#pragma once
#include <d3dcompiler.h>
#include <memory>
//渲染数据
#include "RenderLayer.h" // 渲染层级结构体
#include "RenderData.h"  // 渲染数据结构体
#include "Frustum.h"     // 视锥体结构体
//几何体导入
#include "GeoGenerator.h"// 导入几何体网格,模板需要实现
#include "MeshGeometry.h"// 几何体数据
#include "D3DTools.h"    // DX工具方法

class FrameResource;
class RenderComponent;
class CollisionComponent;
class SubmeshDraw;
class Node;
class RenderComponent;
class CollisionComponent;
class SkeletonComponent;
class AnimationClip;
class Skeleton;

extern enum class ECollisionType :uint32_t;

//绘制统计信息
struct DrawStats
{
    UINT totalObjects   = 0;  // 绘制对象总数
    UINT visibleObjects = 0;  // 视锥体内对象
    UINT culledObjects  = 0;  // 剔除对象
    UINT drawnSubmeshs  = 0;  // 实际提交绘制
    UINT drawCalls      = 0;  // 提交绘制次数(不包含2D文字)
    // 每个渲染层级的绘制对象
    UINT perLayerDrawn[(int)RenderLayer::Count] = {}; 
};

//材质层级描述,用于注册多重材质
struct MaterialLayerDesc
{
	std::string diffTex;                // 漫反射纹理名
	std::string norTex ;                // 法线纹理名
	DirectX::XMFLOAT2 uvScale = {1, 1}; // 纹理缩放
    DirectX::XMFLOAT2 uvSpeed = {0, 0}; // 速度
    float rotation = 0.0f;              // 旋转
	float weight   = 1.0f;              // 权重
};

struct SubmeshDesc
{
    std::string              submeshName;
    Material*                mat           = nullptr;
    RenderLayer              layer         = RenderLayer::Opaque;
    D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    DirectX::XMFLOAT4X4      texTransform  = DxMath::Identity4x4();
};


//管理所有场景对象的更新和绘制
class Scene
{
public:
    Scene(ID3D12Device* device,
          ID3D12GraphicsCommandList* cmdList,
          UINT cbvSrvUavDescSize);// 纹理描述符大小,用于偏移
    ~Scene();
    
	// 更新obj/mat常量缓冲区数据
	void Update(float dt, FrameResource* frame);

	// 绘制(同一绘制层级的一起绘制)
	void Draw(ID3D12PipelineState* pso,          // 绘制PSO
		      RenderLayer layer,                 // 绘制层级
		      FrameResource* frame);             // 帧资源

    // ====== 注册资源 ======

    // 注册纹理 (天空盒纹理要最后注册)
    Texture* RegisterTexture(const std::string& name,  // 纹理名
                             const std::wstring& path, // 纹理路径
                             bool isCubeMap = false,   // 是否为环境贴图 
                             bool isHeight  = false);  // 是否是高度图


    // 注册单网格几何体（一个子网格）
    template<class T>                                                                    
    MeshGeometry* RegisterGeometry(const std::string& geoName,                           // 几何体名字
                                   GeoGenerator::TMeshData<T>&& data,                    // 几何体数据
                                   const std::string& submeshName,                       // 子网格名字
                                   EVertexLayout layout = EVertexLayout::Standard);      // 顶点布局
    // 注册多子网格几何体                                                                
    template<class T>                                                                    
    MeshGeometry* RegisterGeometry(const std::string& geoName,                           // 几何体名字
                                   std::vector<std::pair<std::string,                    // pair 打包两个值,方便取出子网格名字
                                   GeoGenerator::TMeshData<T>>>&& submeshes,             // 子网格表
                                   EVertexLayout layout = EVertexLayout::Standard);      // 顶点布局

    // 注册材质(多层材质,自动推导出纹理索引)
	Material* RegisterMaterial(const std::string& name,                                   // 材质名称
		                       const DirectX::XMFLOAT4& albedo = { 1,1,1,1 },             // 漫反射反照率
		                       const DirectX::XMFLOAT3& fresnelR0 = { 0.04f,0.04f,0.04f },// F0
		                       float roughness = 0.5f,                                    // 粗糙度
                               const std::vector<MaterialLayerDesc>& layerDescs = {});    // 纹理层

    // 注册骨架(所有模型的骨架保存在一处)
    Skeleton* RegisterSkeleton(const std::string& name, Skeleton&& skeleton);

    // 注册动画
    AnimationClip* RegisterAnimate(const std::string& name, AnimationClip&& clip);

	// 将纹理资源描述符拷贝到堆中,用于着色器访问
    //资源注册完后执行
	void SaveSRVToHeap(ID3D12Device* device,          // 设备指针
		               ID3D12DescriptorHeap* srvHeap, // SRV堆指针
		               UINT startSlot,                // 空闲槽位开始
		               UINT cbvSrvUavDescSize);       // SRV描述符大小

    // ====== 添加场景对象 ======

    //添加节点(空节点,用于挂载)
    Node* AddNode(const std::string& name, 
                  Node* parent = nullptr);

	//添加空气墙节点(用Center,extents构建碰撞箱)
    Node* AddAirWall(const std::string& name,
                     const DirectX::XMFLOAT3& center,
                     const DirectX::XMFLOAT3& extents,
		             Node* parent = nullptr);

    // 添加空气墙节点(用Min,Max构建碰撞箱)
	Node* AddAirWallMinMax(const std::string& name,
		                   const DirectX::XMFLOAT3& mn,
		                   const DirectX::XMFLOAT3& mx,
		                   Node* parent = nullptr);


    // 添加单子网格渲染对象
    Node* AddRenderObject(const std::string& name,
                          const std::string& geoName,
                          const std::string& submeshName,
                          Material* mat,
                          RenderLayer layer = RenderLayer::Opaque,
                          Node* parent = nullptr);

    // 添加多子网格渲染对象
    Node* AddRenderObject(const std::string& name,
		                  const std::string& geoName,
		                  const std::vector<SubmeshDesc>& submeshDesc,
		                  Node* parent = nullptr);

    // 给已有节点添加碰撞组件
	CollisionComponent* AddCollisionFromNode(Node* node, ECollisionType type);
	// 给已有节点添加蒙皮组件
	SkeletonComponent*  AddSkeletonFromNode(Node* node, const std::string& skeletonName);

    // ====== 组件对象视图分派重载 ====== 

	void AttachToView(Node* node, RenderComponent* comp);   
	void AttachToView(Node* node, CollisionComponent* comp);
	void AttachToView(Node* node, SkeletonComponent* comp);
    
    // ====== 设置成员 ======
     
    // 设置父节点
    void SetParent(Node* child, Node* parent);
    // 脱离节点树
    void Detach(Node* child);                            
    // 设置视锥体(用于剔除),每帧更新
    void SetFrustum(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
    { 
        m_frustum.BulidFrustumFromViewProj(view, proj);
    }
    // 设置观察位置
    void SetCameraEye(DirectX::XMFLOAT3 eye) { m_cameraEye = eye; }
    // 重置绘制信息
    void ResetDrawStats() { m_drawStats = {}; }

    // ====== 移除对象 ======

    // 移除节点
    void RemoveNode(const std::string& name);     // 移除节点

    // ====== 查询======

	// 获取单个节点
	Node* GetNode(const std::string& name);

	// 获取所有场景对象(要渲染)
	const std::vector<Node*>& GetRenderObjects() { return m_renderNodes; }
	// 获取所有碰撞对象(要碰撞)
	const std::vector<Node*>& GetCollisionNodes() const { return m_collisionNodes; }
    // 获取所有蒙皮对象(有动画)
	const std::vector<Node*>& GetSkeletonNodes() const { return m_skeletonNodes; }

	// 获取单个资源数据
	Texture*      GetTexture (const std::string& name);   // 纹理
	MeshGeometry* GetGeometry(const std::string& name);   // 网格
	Material*     GetMaterial(const std::string& name);   // 材质
    Skeleton*     GetSkeleton(const std::string& name);   // 骨骼
    AnimationClip*GetAnimate (const std::string& name);   // 动画

    // 获取数量
	UINT GetMaterialNum() const { return (UINT)m_materials.size(); }
	UINT GetTextureNum()  const { return (UINT)m_textures.size(); }
    UINT GetGeometryNum() const { return (UINT)m_geometries.size(); }
    UINT GetSkeletonNum() const { return (UINT)m_skeletons.size(); }
    UINT GetAnimateNum()  const { return (UINT)m_animateClips.size(); }
    UINT GetRenderNum()   const { return (UINT)m_renderNodes.size(); }
    UINT GetSubmeshNum()  const { return  m_totalSubmeshs;}
   
	// 获取绘制信息
	const DrawStats& GetDrawStats() { return m_drawStats; }
    // 获取视锥体(debug可视化)
    const Frustum& GetFrustum()const { return m_frustum; }

    // 获得地形信息
    const float GetTerrainHeightScale()const { return m_terrainHeightScale; }
    // 双线性采样器,地形高度采样用(仅限R8,单通道8位)
    float GetSamplerBilinear(float u, float v)const;

	// 获取用掉的槽位数量
	UINT GetSRVCount() const { return m_srvCount; }
    // 获取天空盒纹理索引
    UINT GetSkyBoxIndex() const { return m_skyBoxSrvIndex; }
    


private:
    
    // 索引池分配和回收
    int AllocObjCBIndex();
    void RecycleObjCBIndex(int index);
    // 对象到观察点的距离
    float DistSqSubmeshToEye(const SubmeshDraw* sub);

	void CullFrustum();                      // 视锥体剔除
	void ReBuildNodeList();                  // 重建对象线性表(BFS拓扑排序)
	void UpdateWorldTransforms();            // 更新节点对象的世界变换矩阵(根据线性表)
	void UpdateBounds();                     // 更新节点对象的包围盒(根据世界变换矩阵)
	void UpdateObjCB(FrameResource* frame);  // 更新节点对象的对象常量缓冲区
	void UpdateMatCB(FrameResource* frame);  // 更新节点对象的材质常量缓冲区
	void UpdateBoneCB(FrameResource* frame); // 更新节点对象的骨骼常量缓冲区
   
private:

    // DX指针
    ID3D12Device* m_device = nullptr;                 // 设备指针
    ID3D12GraphicsCommandList* m_cmdList = nullptr;   // 命令列表指针
    UINT m_cbvSrvDescSize = 0;                        // 纹理描述符大小,用于堆中句柄偏移


    // 资源(数据层,真实数据)
    std::unordered_map<std::string, std::unique_ptr<Texture>>       m_textures;     // 所有纹理资源
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>  m_geometries;   // 所有网格数据
    std::unordered_map<std::string, std::unique_ptr<Material>>      m_materials;    // 所有材质数据
    std::unordered_map<std::string, std::unique_ptr<Skeleton>>      m_skeletons;    // 所有骨架数据
    std::unordered_map<std::string, std::unique_ptr<AnimationClip>> m_animateClips; // 所有骨架数据

	//地形高度图(地形高度数据)
	std::vector<float> m_terrainHeightData;     // 地形高度图数据(R8)
	UINT  m_terrainHeightW = 0;                 // 地形高度图宽
	UINT  m_terrainHeightH = 0;                 // 地形高度图高
	float m_terrainHeightScale = 0.0f;          // 地形高度图缩放
	bool m_isHeightTex = false;                 // 是否有高度纹理

    // 场景对象(逻辑层,组织对象)
    std::vector<std::unique_ptr<Node>> m_nodes;                   // 所有节点(拥有者)

    // 场景对象视图表
	std::vector<Node*> m_renderNodes;                             // 场景中所有要渲染的对象
	std::vector<Node*> m_collisionNodes;                          // 场景中所有要碰撞的对象
	std::vector<Node*> m_skeletonNodes;                           // 场景中所有有骨骼的对象

	std::unordered_map<std::string, Node*> m_nodeByName;          // 便于查找场景对象
	std::vector<SubmeshDraw*> m_layers[(int)RenderLayer::Count];  // 渲染层级数组,相同渲染层级统一渲染
    std::vector<SubmeshDraw*> m_visibleList;                      // 可见列表,用于绘制时剔除不可见对象

    // BFS拓扑排序(线性表,节点对象重排)
    std::vector<Node*> m_nodeList;                                // 节点更新线性表
    bool m_updateDirty = true;                                    // 线性表更新标识

    //Debug 信息
    DirectX::XMFLOAT3 m_cameraEye = { 0,0,0 };                    // 摄像机位置(透明物体排序)
    Frustum m_frustum;                                            // 视锥体(可视化)
    DrawStats m_drawStats;                                        // 绘制信息统计
    int m_totalSubmeshs =0;                                       // 子网格总数

	// SRV 堆,由全局堆管理,场景只使用
    ID3D12DescriptorHeap* m_srvHeap = nullptr;                    // srv堆指针
    UINT m_srvStartSlot = 0;                                      // 纹理在全局堆里的起始槽位
    UINT m_srvCount = 0;                                          // 用掉的槽位数量
    std::vector<std::string> m_tex2DList;                         // 普通2D纹理名顺序,根据名字数组决定纹理描述符在堆中的顺序
    std::string m_skyBoxName;                                     // 天空盒纹理名
    int m_skyBoxSrvIndex = 0;                                     // 天空盒纹理在堆中的索引

    // 自动索引分配
    std::vector<int> m_freeCBIndexPool;                           // 空闲常量缓冲区索引池
    int      m_nextObjCBIndex = 0;                                // 自动推出下一个ObjCB的索引
    int      m_nextMatIndex   = 0;                                // 自动推出下一个MatCB的索引
    uint32_t m_nextBoneOffset = 0;                                // 自动推出下一个骨骼的偏移

};

template<class T>
inline MeshGeometry* Scene::RegisterGeometry(const std::string& geoName, 
                                             GeoGenerator::TMeshData<T>&& data, 
                                             const std::string& submeshName, 
                                             EVertexLayout layout)
{
    return RegisterGeometry<T>(geoName, { {submeshName,std::move(data)}}, layout);
}

template<class T>
inline MeshGeometry* Scene::RegisterGeometry(const std::string& geoName, 
                                             std::vector<std::pair<std::string, 
                                             GeoGenerator::TMeshData<T>>>&& submeshes, 
                                             EVertexLayout layout)
{
    using namespace DirectX;

    //多子网格

    // 总顶点和索引
    std::vector<T>   allVertices;
    std::vector<uint32_t> allIndices;
    std::unordered_map<std::string, SubmeshGeometry> allSubMeshs;

    //记录顶点.索引数量
    size_t totalVerts = 0;
    size_t totalIdxs = 0;

    //统计顶点.索引总数
    for (const auto& it : submeshes)
    {
        totalVerts += it.second.vertices.size();
        totalIdxs +=  it.second.indices.size();
    }

    //预先分配内存
    allVertices.reserve(totalVerts);
    allIndices. reserve(totalIdxs);

    for (auto& it : submeshes)
    {
        //pair的两个内容
        auto& submeshName = it.first;
        auto& meshData = it.second;

        //定义子网格
        SubmeshGeometry sub;

        sub.indexCount = (UINT)meshData.indices.size();// 索引数量
        sub.startIndex = (UINT)allIndices.size();      // 开始索引
        sub.vertexOffset = (INT)allVertices.size();    // 顶点偏移

        XMFLOAT3 mn = meshData.bound_min;
        XMFLOAT3 mx = meshData.bound_max;
        //子网格包围盒(1个中心点+中心到角点的距离)
        sub.Bounds.Center =
        {
            (mn.x + mx.x) * 0.5f,
            (mn.y + mx.y) * 0.5f,
            (mn.z + mx.z) * 0.5f
        };
        sub.Bounds.Extents =
        {
            (mx.x - mn.x) * 0.5f,
            (mx.y - mn.y) * 0.5f,
            (mx.z - mn.z) * 0.5f
        };

        //子网格包围球(1个中点+半径)
        sub.Spheres.Center = sub.Bounds.Center;
        float hx = sub.Bounds.Extents.x;
        float hy = sub.Bounds.Extents.y;
        float hz = sub.Bounds.Extents.z;
        sub.Spheres.Radius = sqrtf(hx * hx + hy * hy + hz * hz);

        //传入子网格表
        allSubMeshs[submeshName] = sub;

        //移动vector,顶点到总顶点
        allVertices.insert(allVertices.end(),
            std::make_move_iterator(meshData.vertices.begin()),
            std::make_move_iterator(meshData.vertices.end()));

        //移动vector,索引到总索引
        allIndices.insert(allIndices.end(),
            std::make_move_iterator(meshData.indices.begin()),
            std::make_move_iterator(meshData.indices.end()));

    }

    //获取总大小
    UINT vbByteSize = (UINT)allVertices.size() * sizeof(T);
    UINT ibByteSize = (UINT)allIndices.size() * sizeof(uint32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->name   = geoName;
    geo->layout = layout;


    //复制到内存
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
    CopyMemory(geo->vertexBufferCPU->GetBufferPointer(),
        allVertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->indexBufferCPU));
    CopyMemory(geo->indexBufferCPU->GetBufferPointer(),
        allIndices.data(), ibByteSize);

    //复制到显存
    geo->vertexBufferGPU = D3DTools::CreateDefaultBuffer(
        m_device, m_cmdList, allVertices.data(),
        vbByteSize, geo->vertexBufferUPload);

    geo->indexBufferGPU = D3DTools::CreateDefaultBuffer(
        m_device, m_cmdList, allIndices.data(),
        ibByteSize, geo->indexBufferUPload);

    //填写网格信息
    geo->vertexByteStride = sizeof(T);          // 顶点步长
    geo->vertexByteSize = vbByteSize;           // 顶点大小
    geo->indexFormat = DXGI_FORMAT_R32_UINT;    // 索引格式
    geo->indexByteSize = ibByteSize;            // 索引大小
    geo->meshGeometry = std::move(allSubMeshs); // 子网格表

    //移入网格表
    m_geometries[geoName] = std::move(geo);
    return m_geometries[geoName].get();
    
}



