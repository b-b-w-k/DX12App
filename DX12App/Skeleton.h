#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>


// 骨骼结构体

struct Bone
{
	std::string name;
	int parentIndex;

	// 网格空间=骨架空间
	// 初始绑定姿态矩阵和骨架空间矩阵分别是顶点和骨骼在此空间中的变换
	//
	// 逆初始绑定姿态矩阵(逆骨架空间矩阵)
	// 将顶点从网格空间->骨骼局部空间
	// 同一坐标系空间下,乘对象逆矩阵会变换至对象的局部空间
	DirectX::XMFLOAT4X4 InvBindPose;

	// 绑定姿态下，本骨骼相对父骨骼的局部变换矩阵
	// 在没有动画时使用,用它作为局部矩阵参与累积
	DirectX::XMFLOAT4X4 LocalBindPose;

};

// 骨架树
// 包含所有骨骼的层次结构
class  Skeleton
{
public:

	int GetBoneIndex(const std::string& boneName)const
	{
		auto it = m_boneIndices.find(boneName);
		return it == m_boneIndices.end() ? -1 : it->second;
	}
	const Bone& GetBone(int boneIndex) const {return m_bones[boneIndex];}
	const std::vector<Bone>& GetBones()const {return m_bones;}
	int GetBonesCount() const { return m_bones.size(); }

	void SetBones(std::vector<Bone>&& bones)
	{
		m_bones = std::move(bones);
		m_boneIndices.clear();
		//重建骨骼索引表
		for (int i = 0; i < bones.size(); ++i)
			m_boneIndices[bones[i].name] = i;
	}

private:

	// 保证拓扑序(父骨骼在前)
	std::vector<Bone> m_bones; //骨骼表

	// 通过名字查找骨骼索引
	std::unordered_map<std::string, int> m_boneIndices; //骨骼索引表
};
