#include "PCH.h"
#include "GeoGenerator.h"

//模型导入
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment(lib,"assimpd.lib")

using namespace DirectX;

// 将assimp的矩阵转换为DX的矩阵(4x4)
static XMFLOAT4X4 AiToDXMat4x4(const aiMatrix4x4& mat)
{
	XMFLOAT4X4 M;
	M._11 = mat.a1; M._12 = mat.a2; M._13 = mat.a3; M._14 = mat.a4;
	M._21 = mat.b1; M._22 = mat.b2; M._23 = mat.b3; M._24 = mat.b4;
	M._31 = mat.c1; M._32 = mat.c2; M._33 = mat.c3; M._34 = mat.c4;
	M._41 = mat.d1; M._42 = mat.d2; M._43 = mat.d3; M._44 = mat.d4;
	return M;
}

// 通过名字查找当前节点是否是骨骼(保存在mesh中)
static aiBone* FindBone(const aiScene* scene, const std::string& name)
{
	for (uint32_t m =0;m< scene->mNumMeshes;++m)
	{
		aiMesh* mesh = scene->mMeshes[m];//获取网格
		for (uint32_t b =0;b< mesh->mNumBones;++b)//遍历网格中的骨骼
		{
			// 如果与网格中的骨骼匹配就返回
			if (name == mesh->mBones[b]->mName.C_Str())
				return mesh->mBones[b];
		}

	}
	return nullptr;
}

// 遍历骨骼层次,DFS深度优先+显式栈
// 保证拓扑序(根在前)
static void TraverseBoneLayer(aiNode* root, //根节点
							  const aiScene* scene, //场景入口
							  std::vector<Bone>& bones)//输出骨骼(拓扑序)
							 
{

	//声明显式栈
	struct EntryStack
	{
		aiNode* node;   //遍历的节点
		int parentIndex;//父节点索引
	};

	std::vector<EntryStack> stack;
	stack.push_back({ root,-1 });//从根节点开始遍历,父索引为-1

	while (!stack.empty())
	{
		//弹出尾部元素
		EntryStack ele = stack.back();
		stack.pop_back();

		int myIndex = -1;//初始化本节点索引

		//判断当前节点是否为骨骼
		if (aiBone* aiBone = FindBone(scene, ele.node->mName.C_Str()))
		{
			//填写骨骼结构体
			Bone bone;
			bone.name = aiBone->mName.C_Str();
			bone.InvBindPose = AiToDXMat4x4(aiBone->mOffsetMatrix);
			bone.LocalBindPose = AiToDXMat4x4(ele.node->mTransformation);//默认姿态
			bone.parentIndex = ele.parentIndex;

			//先记录索引
			myIndex = bones.size();
			//再写入
			bones.push_back(bone);
		}
		// 不是骨骼就继续

		// 逆序压栈,保证遍历顺序为根后子
		// 取出该节点所有的孩子压栈
		for (int i = ele.node->mNumChildren - 1; i >= 0; --i)
		{
			stack.push_back(
				{
				ele.node->mChildren[i], //压入子节点
				//如果根节点不是骨骼,就将当前节点的父索引设为最近父骨骼索引
				myIndex >= 0 ? myIndex : ele.parentIndex 
				});
		}

	}

}

// 插值动画帧(因为SRT时间轴不一定对齐)
// 线性插值(ST)返回3d向量
static aiVector3D  LerpVec(const aiVectorKey* keys, uint32_t num, float t)
{
	if (num == 0)return aiVector3D(0, 0, 0);// 没有帧
	if (num == 1)return keys[0].mValue;     // 一帧
	//首尾超出部分按首位帧补齐
	if (t < keys[0].mTime)return keys[0].mValue;
	if (t > keys[num-1].mTime)return keys[num-1].mValue;

	//中间部分线性插值
	for (uint32_t i = 0; i < num - 1; ++i)
	{
		if (t >= keys[i].mTime && t <= keys[i + 1].mTime)
		{
			//比例
			float a = (t - keys[i].mTime) / (keys[i + 1].mTime - keys[i].mTime);
			//插值
			return keys[i].mValue * (1.0f - a) + keys[i + 1].mValue * a;
		}
	}
	return keys[num - 1].mValue;
}

// 球面插值(R)返回四元数
static aiQuaternion  SLerpQuat(const aiQuatKey* keys, uint32_t num, float t)
{
	if (num == 0)return aiQuaternion(1, 0, 0,0);// 没有帧
	if (num == 1)return keys[0].mValue;     // 一帧
	//首尾超出部分按首位帧补齐
	if (t < keys[0].mTime)return keys[0].mValue;
	if (t > keys[num - 1].mTime)return keys[num - 1].mValue;

	//中间部分球面插值
	for (uint32_t i = 0; i < num - 1; ++i)
	{
		if (t >= keys[i].mTime && t <= keys[i + 1].mTime)
		{
			//比例
			float a = (t - keys[i].mTime) / (keys[i + 1].mTime - keys[i].mTime);
			aiQuaternion r;
			//内部球面插值函数
			aiQuaternion::Interpolate(r, keys[i].mValue, keys[i + 1].mValue, a);
			return r;
		}
	}
	return keys[num - 1].mValue;
}

GeoGenerator::SkinnedData GeoGenerator::LoadSkinnedMesh(std::string fileName)
{
	SkinnedData skinnedData;

	unsigned int flags = aiProcess_Triangulate | //变为三角面
						 aiProcess_LimitBoneWeights| // 限制每顶点骨骼数(max 4)
						 aiProcess_ConvertToLeftHanded; //左手坐标系


	const aiScene* scene = aiImportFile(fileName.c_str(), flags);//导入

	//错误输出
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		OutputDebugStringA(aiGetErrorString());
		return SkinnedData();
	}
	auto debugStr = fileName + " import success!\n";
	OutputDebugStringA(debugStr.c_str());

	SkeletonMeshData& meshData = skinnedData.mesh;

	// ======= 骨架提取 =======
	std::vector<Bone> bones; //骨架
	//遍历节点数获取所有骨骼
	TraverseBoneLayer(scene->mRootNode, scene, bones);
	//放入骨架结构体
	skinnedData.skeleton.SetBones(std::move(bones));
	
	//设置调试信息
	std::string boneStr = "boneNum" + std::to_string(skinnedData.skeleton.GetBonesCount()) + "\n";
	OutputDebugStringA(boneStr.c_str());


	//初始化包围盒
	meshData.bound_min = { FLT_MAX, FLT_MAX, FLT_MAX };
	meshData.bound_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	// ======= 填充顶点数据 =======
	for (UINT meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		//顶点偏移
		uint32_t vertexOffset = static_cast<int32_t>(meshData.vertices.size());
		//顶点
		for (UINT v = 0; v < mesh->mNumVertices; v++)
		{

			aiVector3D pos = mesh->mVertices[v];
			aiVector3D nor = mesh->mNormals ? mesh->mNormals[v] : aiVector3D(0, 1, 0);
			aiVector3D uv = (mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][v] : aiVector3D(0, 0, 0));
			aiVector3D tan = (mesh->mTangents ? mesh->mTangents[v] : aiVector3D(1, 0, 0));

			SkeletonVertex vert = {};
			vert.pos      = { pos.x, pos.y, pos.z };
			vert.normal   = { nor.x, nor.y, nor.z };
			vert.uv       = { uv.x, uv.y };
			vert.tangentU = { tan.x, tan.y, tan.z };
			//先置空,后面填骨骼权重
			for (int k = 0; k < 4; ++k)
			{
				vert.boneWeights[k] = 0;
				vert.boneIndices[k] = 0;
			}
			meshData.vertices.push_back(vert);

			meshData.bound_min.x = std::min(meshData.bound_min.x, pos.x);
			meshData.bound_min.y = std::min(meshData.bound_min.y, pos.y);
			meshData.bound_min.z = std::min(meshData.bound_min.z, pos.z);
			meshData.bound_max.x = std::max(meshData.bound_max.x, pos.x);
			meshData.bound_max.y = std::max(meshData.bound_max.y, pos.y);
			meshData.bound_max.z = std::max(meshData.bound_max.z, pos.z);
		}

		// ======= 填写骨骼权重,索引 =======
		for (uint32_t b = 0; b < mesh->mNumBones; ++b)
		{
			//不是骨骼就继续
			aiBone* bone = mesh->mBones[b];
			int boneIndex = skinnedData.skeleton.GetBoneIndex(bone->mName.C_Str());
			if (boneIndex < 0)continue;
			//先取骨骼权重
			for (uint32_t i =0;i< bone->mNumWeights;++i)
			{
				const aiVertexWeight& weight = bone->mWeights[i];
				// 获取骨骼影响的顶点索引
				uint32_t vIndex = vertexOffset + weight.mVertexId;
				if (vIndex >= meshData.vertices.size())continue;
				// 获取骨骼影响的顶点
				SkeletonVertex& v = meshData.vertices[vIndex];

				//插入权重
				for (int k = 0; k < 4; ++k)
				{
					//有空就插
					if (v.boneWeights[k] == 0.0f)
					{
						//权重和索引
						v.boneWeights[k] = weight.mWeight;
						assert(boneIndex <= 256);
						v.boneIndices[k] = (uint8_t)boneIndex;
						break;
					}
				}

			}
		}
		//索引
		for (UINT f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace face = mesh->mFaces[f];
			for (UINT i = 0; i < face.mNumIndices; i++)
			{
				meshData.indices.push_back(vertexOffset + face.mIndices[i]);
			}
		}
	}

	// 权重归一化
	for (auto& v : meshData.vertices)
	{
		float sum = v.boneWeights[0] + v.boneWeights[1] + 
					v.boneWeights[2] + v.boneWeights[3];
		//有骨骼权重
		if (sum > 1e-8)
		{
			float a = 1.0f / sum;
			for (int i = 0; i < 4; ++i)
				v.boneWeights[i] *= a;
		}
		else//没有就将第一个骨骼权重设为1
		{
			v.boneWeights[0] = 1.0f;
			v.boneIndices[0] = 0;
		}
	}

	//输出调试信息
	std::string vertexStr = "vertexNum" + std::to_string(meshData.vertices.size()) + "\n";
	OutputDebugStringA(vertexStr.c_str());
	std::string indexStr = "indexNum" + std::to_string(meshData.indices.size()) + "\n";
	OutputDebugStringA(indexStr.c_str());

	// ======== 提取动画 ========

	const int boneCount = skinnedData.skeleton.GetBonesCount();

	//遍历动画片段
	for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
	{
		aiAnimation* anim = scene->mAnimations[a];
		AnimationClip clip;
		clip.name = anim->mName.C_Str();

		clip.boneAnimations.resize(boneCount);

		//获取动画tick,用来获取描述
		float ticksPerSecond = (anim->mTicksPerSecond > 0.0) ? (float)anim->mTicksPerSecond : 25.0f;

		//遍历所有骨骼的动画片段
		for (uint32_t c = 0; c < anim->mNumChannels; ++c)
		{
			//根据名字获取骨骼动画索引
			aiNodeAnim* ch = anim->mChannels[c];
			int boneIndex = skinnedData.skeleton.GetBoneIndex(ch->mNodeName.C_Str());
			if (boneIndex < 0)continue;
			//获取骨骼动画(要填充的)
			BoneAnimation& ba = clip.boneAnimations[boneIndex];

			//获取三种变换(SRT)的所有关键帧对应的时间
			std::vector<float> times;
			times.reserve(ch->mNumPositionKeys + ch->mNumRotationKeys + ch->mNumScalingKeys);

			//填充T
			for (UINT i = 0; i < ch->mNumPositionKeys; ++i)
				times.push_back((float)ch->mPositionKeys[i].mTime);
			//填充R
			for (UINT i = 0; i < ch->mNumRotationKeys; ++i)
				times.push_back((float)ch->mRotationKeys[i].mTime);
			//填充S
			for (UINT i = 0; i < ch->mNumScalingKeys; ++i)
				times.push_back((float)ch->mScalingKeys[i].mTime);


			//排序(升序)+去重
			//因为SRT,开始结束的时间可能不同
			//所以得到所有时段(最先开始,最后结束)的关键帧对三种变换进行均匀采样
			std::sort(times.begin(), times.end());
			times.erase(std::unique(times.begin(), times.end()), times.end());

			//动画关键帧
			ba.keyFrames.reserve(times.size());
			for (float t : times)
			{
				//TS线性插值
				aiVector3D T = LerpVec(ch->mPositionKeys, ch->mNumPositionKeys, t);
				aiVector3D S = LerpVec(ch->mScalingKeys, ch->mNumScalingKeys, t);
				//R球面插值
				aiQuaternion R = SLerpQuat(ch->mRotationKeys, ch->mNumRotationKeys, t);

				//填充关键帧
				KeyFrame keyFrame;
				keyFrame.curTime = t / ticksPerSecond; // 用ticksPerSecond将单位转换成秒
				keyFrame.translation = { T.x, T.y, T.z };
				keyFrame.scale = { S.x, S.y, S.z };
				keyFrame.rotationQuat = { R.x, R.y, R.z, R.w };

				ba.keyFrames.push_back(keyFrame);
			}
		}

		//输出调试信息
		std::string animStr = "anim: " + clip.name+ "  bones: " + std::to_string(clip.boneAnimations.size()) + "\n";
		OutputDebugStringA(animStr.c_str());

		//移入skinnedData
		skinnedData.clips[clip.name] = std::move(clip);
	}

	aiReleaseImport(scene);//释放场景
	return skinnedData;
}

GeoGenerator::VertexMeshData GeoGenerator::LoadMesh(const std::string fileName)
{
	VertexMeshData meshData;

	unsigned int flags = aiProcess_Triangulate| //变为三角面
		aiProcess_ConvertToLeftHanded;

	const aiScene* scene = aiImportFile(fileName.c_str(), flags);//导入

	//错误输出
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		auto err = aiGetErrorString();
		OutputDebugStringA(err);
		return VertexMeshData();
	}
	auto debugStr = fileName + " import success!\n";
	OutputDebugStringA(debugStr.c_str());

	meshData.bound_min = { FLT_MAX, FLT_MAX, FLT_MAX };
	meshData.bound_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (UINT meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
	{
		aiMesh* mesh=scene->mMeshes[meshIndex];
		//顶点偏移
		uint32_t vertexOffset = static_cast<int32_t>(meshData.vertices.size());
		//顶点
		for (UINT  v = 0; v < mesh->mNumVertices; v++)
		{
			
			aiVector3D pos = mesh->mVertices[v];
			aiVector3D nor = mesh->mNormals[v];
			aiVector3D uv = (mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][v] : aiVector3D(0, 0, 0));
			aiVector3D tan = (mesh->mTangents ? mesh->mTangents[v] : aiVector3D(1, 0, 0));
			meshData.vertices.emplace_back(Vertex(
				pos.x, pos.y, pos.z,
				nor.x, nor.y, nor.z,
				uv.x, uv.y,
				tan.x, tan.y, tan.z));

			meshData.bound_min.x = std::min(meshData.bound_min.x, pos.x);
			meshData.bound_min.y = std::min(meshData.bound_min.y, pos.y);
			meshData.bound_min.z = std::min(meshData.bound_min.z, pos.z);
			meshData.bound_max.x = std::max(meshData.bound_max.x, pos.x);
			meshData.bound_max.y = std::max(meshData.bound_max.y, pos.y);
			meshData.bound_max.z = std::max(meshData.bound_max.z, pos.z);
		}
		//索引
		for (UINT f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace face = mesh->mFaces[f];
			for (UINT  i = 0; i < face.mNumIndices; i++)
			{
				meshData.indices.push_back(vertexOffset + face.mIndices[i]);
			}
		}
	}
	std::string vertexStr = "vertexNum" + std::to_string(meshData.vertices.size()) + "\n";
	OutputDebugStringA(vertexStr.c_str());
	std::string indexStr = "indexNum" + std::to_string(meshData.indices.size()) + "\n";
	OutputDebugStringA(indexStr.c_str());

	aiReleaseImport(scene);//释放场景
	return meshData;
}

GeoGenerator::VertexMeshData GeoGenerator::CreatGrid(float width, float depth, 
													 uint32_t col, uint32_t row)
{
	VertexMeshData meshData;

	uint32_t vertexCount = col * row;//顶点数
	uint32_t faceCount = (col - 1) * (row - 1) * 2;//面数

	float halfWidth = width * 0.5f;
	float halfDepth = depth * 0.5f;

	float dx = width / (row - 1);
	float dz = depth / (col - 1);

	float du = 1.0f / (row - 1);
	float dv = 1.0f / (col - 1);
	meshData.vertices.resize(vertexCount);
	for (uint32_t i = 0; i < col; ++i)
	{
		float z = -halfDepth + i * dz;

		for (uint32_t j = 0; j < row; ++j)
		{
			float x = -halfWidth + j * dx;
			meshData.vertices[i * row + j].pos = XMFLOAT3(x, 0.0f, z);
			meshData.vertices[i * row + j].normal= XMFLOAT3(0.0f, 1.0f, 0.0f);
			//meshData.vertices[i * row + j].color =XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			meshData.vertices[i * row + j].tangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
			//uv
			meshData.vertices[i * row + j].uv.x = j * du;
			meshData.vertices[i * row + j].uv.y = i * dv;


		}
	}
	//包围盒
	meshData.bound_min = { -halfWidth, 0.0f, -halfDepth };
	meshData.bound_max = { halfWidth, 0.0f,  halfDepth };

	//索引
	meshData.indices.resize(faceCount * 3);
	uint32_t k = 0;
	for (uint32_t i = 0; i < col-1; ++i)
	{
		for (uint32_t j = 0; j < row-1; ++j)
		{ 
			meshData.indices[k] = i * row + j;
			meshData.indices[k + 1] = (i + 1)* row + j;
			meshData.indices[k + 2] = i * row + j + 1;

			meshData.indices[k + 3] = (i + 1) * row + j;
			meshData.indices[k + 4] = (i + 1) * row + j + 1;
			meshData.indices[k + 5] =  i* row + j + 1;

			k += 6;
		}
	}
	return meshData;

}

GeoGenerator::VertexMeshData GeoGenerator::CreatBox(float width, float height, 
													float depth, uint32_t sub)
{
	VertexMeshData meshData;

	Vertex v[24];

	float halfW = width  * 0.5f;
	float halfH = height * 0.5f;
	float halfD = depth  * 0.5f;

	// 前
	v[ 0] = Vertex(-halfW, -halfH, -halfD, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	v[ 1] = Vertex(-halfW, +halfH, -halfD, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[ 2] = Vertex(+halfW, +halfH, -halfD, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[ 3] = Vertex(+halfW, -halfH, -halfD, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	// 后
	v[ 4] = Vertex(-halfW, -halfH, +halfD, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[ 5] = Vertex(+halfW, -halfH, +halfD, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[ 6] = Vertex(+halfW, +halfH, +halfD, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[ 7] = Vertex(-halfW, +halfH, +halfD, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);

	// 顶
	v[ 8] = Vertex(-halfW, +halfH, -halfD, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	v[ 9] = Vertex(-halfW, +halfH, +halfD, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[10] = Vertex(+halfW, +halfH, +halfD, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[11] = Vertex(+halfW, +halfH, -halfD, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	// 底
	v[12] = Vertex(-halfW, -halfH, -halfD, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[13] = Vertex(+halfW, -halfH, -halfD, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[14] = Vertex(+halfW, -halfH, +halfD, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[15] = Vertex(-halfW, -halfH, +halfD, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
					
	// 左
	v[16] = Vertex(-halfW, -halfH, +halfD, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f);
	v[17] = Vertex(-halfW, +halfH, +halfD, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	v[18] = Vertex(-halfW, +halfH, -halfD, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	v[19] = Vertex(-halfW, -halfH, -halfD, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f);
					
	// 右
	v[20] = Vertex(+halfW, -halfH, -halfD, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	v[21] = Vertex(+halfW, +halfH, -halfD, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[22] = Vertex(+halfW, +halfH, +halfD, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[23] = Vertex(+halfW, -halfH, +halfD, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);

	//添加元素
	meshData.vertices.assign(&v[0], &v[24]);

	meshData.bound_min = { -halfW, -halfH, -halfD };
	meshData.bound_max = {  halfW,  halfH,  halfD };
	uint32_t i[36];

	// 前
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// 后
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// 顶
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// 底
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// 左
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// 右
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	//添加元素
	meshData.indices.assign(&i[0], &i[36]);

	//细分...
	sub = std::min<uint32_t>(sub, 6u);//最大细分控制在6
	for (uint32_t i = 0; i < sub; i++)
	{
		Subdivide(meshData);
	}
	return meshData;

} 

GeoGenerator::VertexMeshData GeoGenerator::CreatSphere(float radius, uint32_t row, uint32_t col)
{
	VertexMeshData meshData;
	//顶底顶点
	Vertex topVertex(0.0f,  radius, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	Vertex botVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	meshData.vertices.push_back(topVertex);

	float phiStep   =XM_PI  / col;//俯仰
	float thetaStep =XM_2PI / row;//偏航

	for (uint32_t i = 0; i <= col - 1; ++i)
	{
		float phi = i * phiStep;
		for (uint32_t j = 0; j <= row; ++j)
		{
			float theta = j * thetaStep;

			Vertex v;
			//位置
			v.pos.x = radius * sinf(phi) * cosf(theta);
			v.pos.y = radius * cosf(phi);
			v.pos.z = radius * sinf(phi) * sinf(theta);
			//切线
			v.tangentU.x =-radius * sinf(phi) * sinf(theta);
			v.tangentU.y = 0.0f;
			v.tangentU.z = radius * sinf(phi) * cosf(theta);

			XMVECTOR T = XMLoadFloat3(&v.tangentU);
			XMStoreFloat3(&v.tangentU,XMVector3Normalize(T));

			XMVECTOR p = XMLoadFloat3(&v.pos);
			//圆心(0,0)到点位置的向量归一化=法线
			XMStoreFloat3(&v.normal, XMVector3Normalize(p));

			//纹理坐标
			v.uv.x = theta / XM_2PI;
			v.uv.y = phi   / XM_PI;

			meshData.vertices.push_back(v);

		}
	}

	meshData.vertices.push_back(botVertex);

	//包围盒
	meshData.bound_min = { -radius, -radius, -radius };
	meshData.bound_max = { radius,  radius,  radius };

	for (uint32_t i = 1; i <= row; i++)//第二行与topVector相连
	{
		meshData.indices.push_back(0);//topVector
		meshData.indices.push_back(i + 1);
		meshData.indices.push_back(i);

	}
	//中间行
	uint32_t baseIndex = 1;//第二行开始
	uint32_t colVecIndex = row + 1;//列索引(加上之前的)

	for (uint32_t i = 0; i < col-1; i++)//到倒数第二行为止
	{
		for (uint32_t j = 0; j < row; j++)
		{
			meshData.indices.push_back(baseIndex + i * colVecIndex + j);
			meshData.indices.push_back(baseIndex + i * colVecIndex + j + 1);
			meshData.indices.push_back(baseIndex + (i + 1) * colVecIndex + j);

			meshData.indices.push_back(baseIndex + (i + 1) * colVecIndex + j);
			meshData.indices.push_back(baseIndex + i * colVecIndex + j + 1);
			meshData.indices.push_back(baseIndex + (i + 1) * colVecIndex + j + 1);

		}
	}
	//最后一个顶点的索引bottVector
	uint32_t endIndex = (uint32_t)meshData.vertices.size() - 1;
	//从倒数第三行开始
	baseIndex = endIndex - colVecIndex;

	for (uint32_t i = 0; i < col; i++)//倒数第二行与botVector相连
	{
		meshData.indices.push_back(endIndex);//botVector
		meshData.indices.push_back(baseIndex+i);
		meshData.indices.push_back(baseIndex+i + 1);
	}

	return meshData;
}

GeoGenerator::ColorMeshData GeoGenerator::CreateFrustum(float nz, float fz, 
														float nh, float nw, 
														float fh, float fw)
{
	ColorMeshData meshData;

	//8个顶点: 近平面4,远平面4
	meshData.vertices =
	{
		//近
		ColorVertex{XMFLOAT3(-nw, -nh, nz),XMFLOAT4(DirectX::Colors::Green)}, // 左下
		ColorVertex{XMFLOAT3( nw, -nh, nz),XMFLOAT4(DirectX::Colors::Red)},   // 右下
		ColorVertex{XMFLOAT3( nw,  nh, nz),XMFLOAT4(DirectX::Colors::Yellow)},// 左上
		ColorVertex{XMFLOAT3(-nw,  nh, nz),XMFLOAT4(DirectX::Colors::White)}, // 右上

		//远
		ColorVertex{XMFLOAT3(-fw, -fh, fz),XMFLOAT4(DirectX::Colors::Black)}, // 左下
		ColorVertex{XMFLOAT3( fw, -fh, fz),XMFLOAT4(DirectX::Colors::Blue)},  // 右下
		ColorVertex{XMFLOAT3( fw,  fh, fz),XMFLOAT4(DirectX::Colors::Orange)},// 左上
		ColorVertex{XMFLOAT3(-fw,  fh, fz),XMFLOAT4(DirectX::Colors::Purple)} // 右上
	};

	//索引
	meshData.indices =
	{
		// 近
		0, 1, 2, 0, 2, 3,
		// 远
		4, 6, 5, 4, 7, 6,
		// 底
		0, 4, 5, 0, 5, 1,
		// 右
		1, 5, 6, 1, 6, 2,
		// 顶
		2, 6, 7, 2, 7, 3,
		// 左
		3, 7, 4, 3, 4, 0
	};

	return meshData;
}

GeoGenerator::VertexMeshData GeoGenerator::CreatTerrain(float width, float depth, 
														uint32_t col, uint32_t row, 
														const HeightSampler& heightSampler, 
														float heightScale, float uvScale)
{
	VertexMeshData meshData;

	const uint32_t vertexCount = col * row;
	const uint32_t faceCount = (col - 1) * (row - 1) * 2;

	const float halfWidth = width * 0.5f;
	const float halfDepth = depth * 0.5f;

	//位置坐标步长
	const float offsetX = width / (col - 1);//col对应列数(x方向多少列)
	const float offsetZ = depth / (row - 1);

	//UV坐标步长
	const float offsetU = 1.0f / (col - 1);
	const float offsetV = 1.0f / (row - 1);

	// 一次性采样高度图,存表
	std::vector<float> heights(col * row);

	meshData.vertices.resize(vertexCount);

	//创建顶点
	for (uint32_t i = 0; i < row; ++i) //外层深度
	{
		const float v = i * offsetV;//采样高度V
		const float z = -halfDepth + i * offsetZ;//世界z

		for (uint32_t j = 0; j < col; ++j) //内层宽度
		{
			const float u = j * offsetU;//采样高度U
			const float x = -halfWidth + j * offsetX;//世界x

			//采样高度图
			float h = heightSampler ? heightSampler(u, v) : 0.0f;
			heights[i * col + j] = h;//存表
			float y = h * heightScale;

			Vertex& vertex = meshData.vertices[i * col + j];
			vertex.pos = XMFLOAT3(x, y, z);
			vertex.uv  = XMFLOAT2(u * uvScale, v * uvScale);
			//先填充,稍后计算
			vertex.normal   = XMFLOAT3(0, 1, 0);       
			vertex.tangentU = XMFLOAT3(1, 0, 0);       
		}
	}
	//中心差分计算法线,切线
	if (heightSampler)
	{
		for (uint32_t i = 0; i < row ; ++i) //外层深度
		{
			const float v = i * offsetV;//采样高度V

			for (uint32_t j = 0; j < col ; ++j) //内层宽度
			{
				const float u = j * offsetU;//采样高度U

				//当前点高度
				const float h = heights[i * col + j] * heightScale;

				// 取邻域，越界用自身（相当于单边差分 → 0）
				const float hL = (j > 0)       ? heights[i * col + (j - 1)] * heightScale : h;//左
				const float hR = (j < col - 1) ? heights[i * col + (j + 1)] * heightScale : h;//右
				const float hT = (i > 0)       ? heights[(i - 1) * col + j] * heightScale : h;//上
				const float hB = (i < row - 1) ? heights[(i + 1) * col + j] * heightScale : h;//上

				// 邻域的实际世界距离（dx/dz 就是网格步长）
				const float dhdx = (hR - hL) / (2.0f * offsetX);
				const float dhdz = (hB - hT) / (2.0f * offsetZ);


				//求法线 n=normalize(-dh/dx, 1, -dh/dz)
				XMVECTOR n = XMVectorSet(-dhdx, 1.0f, -dhdz, 0.0f);
				n = XMVector3Normalize(n);

				//切线(X方向) tangentU=normalize(dx, dh, 0) 
				XMVECTOR t = XMVectorSet(1.0f, dhdx, 0.0f, 0.0f);
				t = XMVector3Normalize(t);

				Vertex& vertex = meshData.vertices[i * col + j];
				DirectX::XMStoreFloat3(&vertex.normal, n);
				DirectX::XMStoreFloat3(&vertex.tangentU, t);
			}
		}
	}

	//索引计算
	meshData.indices.resize(faceCount * 3);
	uint32_t k = 0;
	for (uint32_t i = 0; i < row-1; ++i)
	{
		for (uint32_t j = 0; j < col-1; ++j)
		{
			// 两个三角形组成一个 quad
			meshData.indices[k] = i * col + j;
			meshData.indices[k + 1] = (i + 1) * col + j;
			meshData.indices[k + 2] = i * col + j + 1;

			meshData.indices[k + 3] = (i + 1) * col + j;
			meshData.indices[k + 4] = (i + 1) * col + j + 1;
			meshData.indices[k + 5] = i * col + j + 1;
			k += 6;
		}
	}

	//包围盒计算
	XMFLOAT3 mn = { FLT_MAX, FLT_MAX, FLT_MAX };
	XMFLOAT3 mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (const auto& vert : meshData.vertices)
	{
		mn.x = std::min(mn.x, vert.pos.x);
		mn.y = std::min(mn.y, vert.pos.y);
		mn.z = std::min(mn.z, vert.pos.z);
		mx.x = std::max(mx.x, vert.pos.x);
		mx.y = std::max(mx.y, vert.pos.y);
		mx.z = std::max(mx.z, vert.pos.z);
	}
	meshData.bound_min = mn;
	meshData.bound_max = mx;

	return meshData;
}

void GeoGenerator::Subdivide(VertexMeshData& meshData)
{
	VertexMeshData inputCopy = meshData;

	//重置源meshData,后面要放入细分后的数据
	meshData.vertices.resize(0);
	meshData.indices.resize(0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	//三角面数量
	uint32_t numTri = (uint32_t)inputCopy.indices.size() / 3;
	for (uint32_t i = 0; i < numTri; i++)
	{
		//使用索引取出能构成三角形的顶点
		Vertex v0 = inputCopy.vertices[inputCopy.indices[i * 3 + 0]];
		Vertex v1 = inputCopy.vertices[inputCopy.indices[i * 3 + 1]];
		Vertex v2 = inputCopy.vertices[inputCopy.indices[i * 3 + 2]];
	
		//创建三角形三条边的中点
		Vertex m0 = MidPoint(v0, v1);
		Vertex m1 = MidPoint(v1, v2);
		Vertex m2 = MidPoint(v0, v2);


		//添加顶点
		meshData.vertices.push_back(v0);
		meshData.vertices.push_back(v1);
		meshData.vertices.push_back(v2);
		meshData.vertices.push_back(m0);
		meshData.vertices.push_back(m1);
		meshData.vertices.push_back(m2);

		//添加索引
		//左下
		meshData.indices.push_back(i * 6 + 0);
		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 5);
		//中间
		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 4);
		meshData.indices.push_back(i * 6 + 5);
		//右下
		meshData.indices.push_back(i * 6 + 5);
		meshData.indices.push_back(i * 6 + 4);
		meshData.indices.push_back(i * 6 + 2);
		//上面
		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 1);
		meshData.indices.push_back(i * 6 + 4);

	}
}

Vertex GeoGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
{
	//求两个顶点的中点
	//两个原顶点
	XMVECTOR p0 = XMLoadFloat3(&v0.pos);
	XMVECTOR p1 = XMLoadFloat3(&v1.pos);
	//原法线
	XMVECTOR n0 = XMLoadFloat3(&v0.normal);
	XMVECTOR n1 = XMLoadFloat3(&v1.normal);
	//原切线
	XMVECTOR t0 = XMLoadFloat3(&v0.tangentU);
	XMVECTOR t1 = XMLoadFloat3(&v1.tangentU);
	//原纹理
	XMVECTOR u0 = XMLoadFloat2(&v0.uv);
	XMVECTOR u1 = XMLoadFloat2(&v1.uv);

	//中点位置
	
	XMVECTOR pos = XMVectorScale(XMVectorAdd(p0, p1), 0.5f);
	XMVECTOR nor = XMVector3Normalize(XMVectorScale(XMVectorAdd(n0, n1), 0.5f));
	XMVECTOR tan = XMVector3Normalize(XMVectorScale(XMVectorAdd(t0, t1), 0.5f));
	XMVECTOR uv  = XMVectorScale(XMVectorAdd(u0, u1), 0.5f);

	//中点
	Vertex midV;
	XMStoreFloat3(&midV.pos, pos);
	XMStoreFloat3(&midV.normal, nor);
	XMStoreFloat3(&midV.tangentU, tan);
	XMStoreFloat2(&midV.uv, uv);
	
	return midV;
}
