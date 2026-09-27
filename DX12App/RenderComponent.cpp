#include "RenderComponent.h"
#include "Node.h"


SubmeshDraw& RenderComponent::AddSubmesh(MeshGeometry* geo, 
                                         const std::string& submeshName, 
                                         Material* mat, 
                                         RenderLayer layer, 
                                         UINT objCBIndex)
{
    //填充子网格信息
    SubmeshDraw sub;
    sub.owner = owner;
    sub.renderComp = this;
    sub.meshGeo = geo;
    sub.submeshName = submeshName;
    sub.material = mat;
    sub.renderLayer = layer;
    sub.objCBIndex = objCBIndex;

    if (geo)//查找网格几何体有无该子网格
    {
        auto it = geo->meshGeometry.find(submeshName);
        if (it != geo->meshGeometry.end())
        {
            sub.layout = geo->layout;
            sub.submesh =it->second;
        }
    }
    m_submeshes.push_back(sub);
    return m_submeshes.back();
}

SubmeshDraw* RenderComponent::GetSubmesh(const std::string& submeshName)
{
    for (auto& sub : m_submeshes)
        if (sub.submeshName == submeshName)return &sub;
    return nullptr;
  
}

void RenderComponent::MarkDirty()
{
	for (auto& sub : m_submeshes)
		sub.MarkDirty();
	//节点同时更新
	owner->MarkDirty();
}

void RenderComponent::SetMaterialAll(Material* mat)
{
    for (auto& sub : m_submeshes)
    {
        sub.material = mat;
        sub.MarkDirty();
    }
    //节点同时更新
    owner->MarkDirty();
}

void RenderComponent::SetTexTransformAll(float u, float v)
{
    DirectX::XMFLOAT4X4 texTran;
    DirectX::XMStoreFloat4x4(&texTran, DirectX::XMMatrixScaling(u, v, 1.0f));
    for (auto& sub : m_submeshes)
    {
        sub.texTransform = texTran;
        sub.MarkDirty();
    }
	//节点同时更新
	owner->MarkDirty();
}
