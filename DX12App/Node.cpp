#include "Node.h"
#include "RenderData.h"

using namespace DirectX;

Node& Node::SetLocalTransform(const XMFLOAT3& pos,
							  const XMFLOAT3& rot, 
							  const XMFLOAT3& scale)
{
	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, scale.z);
	XMMATRIX r = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
	XMMATRIX t = XMMatrixTranslation(pos.x, pos.y, pos.z);
	XMStoreFloat4x4(&localTransform, s * r * t); // 设置本地变换矩阵

	MarkDirty();
	return *this;
}



void Node::SetTranMode(ETranMode mode, bool isKeepPos)
{
	
	if (mode == tranMode) return;

	if (isKeepPos)// 需要保持世界位置
	{
		// 由follow->ingore
		if (mode == ETranMode::IGNORE_PARENT)
		{
			
			localTransform = worldTransform;
		}
		else // ingore->follow
		{
			if (parent)
			{
				//为自身世界变换乘父级世界变换的逆矩阵

				XMMATRIX W = XMLoadFloat4x4(&worldTransform);
				XMMATRIX P = XMLoadFloat4x4(&parent->worldTransform);
				XMMATRIX invP = XMMatrixInverse(nullptr,P);
				XMStoreFloat4x4(&worldTransform, W * invP);
			}
			else // 无父节点与变换到ingore一致
			{
				localTransform = worldTransform;
			}
		}
		
	}
	tranMode = mode;// 应用模式
	MarkDirty();    // 更新标志
}

void Node::UpdateWorld()
{
	XMFLOAT4X4 newWorld;

	if (!parent || tranMode == ETranMode::IGNORE_PARENT)
	{
		//直接应用本地变换
		newWorld = localTransform;
	}
	else
	{
		//应用父级变换(父级已整合之前所有的变换)
		XMMATRIX L = XMLoadFloat4x4(&localTransform);
		XMMATRIX W = XMLoadFloat4x4(&parent->worldTransform);
		XMStoreFloat4x4(&newWorld, L * W);
	}

	//比较原变换矩阵是否与新变换相同
	if (memcmp(&newWorld, &worldTransform, sizeof(XMMATRIX))!=0)
	{
		worldTransform = newWorld;
		MarkDirty();//更新标志
	}

}

void Node::SetParent(Node* p)
{
	// 节点无效返回
	if (p ==parent)return;

	// 防止环状引用
	for (Node* i = p; i; i = i->parent)
		if (i == this) return;
	// 先移除原父节点对自身的引用
	Detach();
	// 设置新父节点
	parent = p;
	if (p) p->children.push_back(this);
	// 标记更新
	MarkDirty();

}

void Node::Detach()
{
	if (!parent) return;
	auto& kids = parent->children;
	//移除父节点对自身的引用
	kids.erase(std::remove(kids.begin(), kids.end(), this), kids.end());
	parent = nullptr;
	// 标记更新
	MarkDirty();
}

void Node::MarkDirty()
{
	numFrameDirty = numFrameResources;
}
