#pragma once
#include <typeindex>
#include <typeinfo>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "DxMath.h"
#include "Component.h"

extern const int numFrameResources;

// 组件id,将类型本身当作键存储
using ComponentTypeID = std::type_index;

//变换模式切换枚举
enum class ETranMode
{
	FOLLOW_PARENT, // 跟随父节点变换(累乘父节点变换矩阵)
	IGNORE_PARENT, // 独立变换(直接应用世界矩阵)
};



// 节点类
// 组件的容器,可以搭载不同组件(碰撞,渲染...)
// 自身拥有基础变换逻辑(世界变换矩阵)
// 有层级关系(可绑定父子级,实现链式变换...)
class Node
{
public:
	Node() = default;
	//禁止隐式转换
	explicit Node(const std::string& n) :name(n) {}
	virtual ~Node() = default;


	// ======== 矩阵变换 ========
	// 设置本地变换矩阵(每个节点独立)
	Node& SetLocalTransform( const DirectX::XMFLOAT3& pos,
							 const DirectX::XMFLOAT3& rot = { 0,0,0 },
							 const DirectX::XMFLOAT3& scale = { 1,1,1 });
	
	void SetTranMode(ETranMode mode, bool isKeepPos);// 更换变换模式
	void UpdateWorld();// 更新到世界矩阵(可以选择应用父节点矩阵变换)

	// ======== 层级 ========
	void SetParent(Node* p); // 设置父节点
	void Detach();			 // 解除层级绑定

	// ======== 脏标识 ========
	void MarkDirty();
	bool IsDirty() const { return numFrameDirty > 0; }

	// ======== 组件管理 ========

	//添加组件
	template <class T>
	T* AddComponent(std::unique_ptr<T> comp); // 传入构建好的组件

	template <class T,class...Args>
	T* AddComponent(Args&&... args);          // 传入参数内部构建组件

	//获取组件,没有返回空(根据组件类型找,T)
	template <class T>
	T* GetComponent();

	//是否有组件(根据组件类型判断,T)
	template <class T>
	bool IsComponent();

	//移除组件(根据组件类型移除,T)
	template<class T>
	void RemoveComponent();

	//获取所有组件
	const std::unordered_map<ComponentTypeID, 
		  std::unique_ptr<Component>>& GetComponentAll() 
		  const { return m_components; }

public:

	std::string name;					           
	
	// 节点
	Node* parent = nullptr;								 // 父节点(获取父节点变换矩阵)
	std::vector<Node*> children;						 // 子节点集合(反向遍历)
	
	// 变换
	DirectX::XMFLOAT4X4 localTransform = DxMath::Identity4x4(); // 本地变换
	DirectX::XMFLOAT4X4 worldTransform = DxMath::Identity4x4(); // 世界变换,最终输出矩阵
	ETranMode tranMode = ETranMode::FOLLOW_PARENT;       // 变换模式

	// 更新
	int numFrameDirty = numFrameResources; // 脏标识

private:

	//组件容器,使用类型索引查找
	std::unordered_map<ComponentTypeID, std::unique_ptr<Component>> m_components;
	 
};


//...参数包,打包组件初始化所用的数据
template<class T, class ...Args>
inline T* Node::AddComponent(Args && ...args)
{
	auto comp = std::make_unique<T>(std::forward<Args>(args)...);
	return AddComponent<T>(std::move(comp));
}

template<class T>
inline T* Node::AddComponent(std::unique_ptr<T> comp)
{
	// 断言:Component一定是T的基类(保证只有组件才能装入容器)
	static_assert(std::is_base_of<Component, T>::value, "T not Component");
	T* pComp = comp.get();
	//注册组件拥有者
	pComp->owner = this; 
	//装入组件容器
	m_components[std::type_index(typeid(T))] = std::move(comp);

	pComp->OnAttach();
	MarkDirty();
	return pComp;

}



template<class T>
inline T* Node::GetComponent()
{
	auto it = m_components.find(std::type_index(typeid(T)));

	return it == m_components.end() ? nullptr : static_cast<T*>(it->second.get());
}

template<class T>
inline bool Node::IsComponent()
{
	return m_components.find(std::type_index(typeid(T))) != m_components.end();
}

template<class T>
inline void Node::RemoveComponent()
{
	auto it = m_components.find(std::type_index(typeid(T)));
	if(it == m_components.end()) return;
	it->second->OnDetach();// 解绑
	m_components.erase(it);// 移除
	MarkDirty(); // 更新脏标识

}
